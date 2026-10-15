#include <common/Execution.hpp>

namespace execution
{
  namespace
  {
    bool areAdjacent(
        kernel::CachedDatabaseAccess &db,
        kernel::DRelId mine,
        kernel::DRelId theirs,
        common::Adjacency adjacency)
    {
      if (adjacency == common::Adjacency::BoundaryToSB)
      {
        throw std::logic_error("BoundaryToSB is not a data-graph adjacency constraint");
      }
      kernel::DNodeId mineNode = db.endpoint(mine, common::myEndpoint(adjacency));
      kernel::DNodeId theirNode = db.endpoint(theirs, common::neighborEndpoint(adjacency));
      return mineNode == theirNode;
    }
  }

  // ===========================================================================
  // SharedBlockIterator
  // ===========================================================================
  // TODO: refactor shared block related stuff in a different file

  std::optional<SharedBlockRow>
  SharedBlockIterator::next()
  {
    auto row =
        manager_->rowAt(
            key_,
            next_index_);

    if (row)
    {
      ++next_index_;
    }

    return row;
  }

  // ===========================================================================
  // SharedBlockIteratorManager
  // ===========================================================================

  SharedBlockIterator SharedBlockIteratorManager::newIterator(
      qg::BlockId block_id,
      std::optional<k::DRelId> boundary)
  {
    const ib::SharedBlockIR &block =
        blocks_.at(block_id);

    // A block with an inner boundary requires an outer DRel.
    if (block.boundary_node.has_value() !=
        boundary.has_value())
    {
      throw std::logic_error(
          "Shared block boundary mismatch");
    }

    ++metrics_.iteratorCreations;

    return SharedBlockIterator{
        this,
        SharedBlockCacheKey{
            .block_id = block_id,
            .boundary = boundary}};
  }

  // TODO: 
  // - look at the Entry class (Execution.hpp), solve errors here (entry.iterator)
  SharedBlockIteratorManager::Entry& SharedBlockIteratorManager::getOrCreateEntry(const SharedBlockCacheKey &key)
  {
    auto it = entries_.find(key);
    if (it != entries_.end())
    {
      return *it->second;
    }

    auto entry = std::make_unique<Entry>();
    entry->producer.emplace(makeProducer(key.block_id, key.boundary));
    Entry *result = entry.get();
    entries_.emplace(key, std::move(entry));
    ++metrics_.cacheEntriesCreated;
    return *result;
  }

  bool
  SharedBlockIteratorManager::produceOne(Entry &entry)
  {
    if (entry.exhausted)
    {
      return false;
    }

    ++metrics_.producerResumptions;

    if (!entry.started)
    {
      entry.iterator.emplace(entry.producer->begin());
      entry.started = true;
    }
    else
    {
      ++(*entry.iterator);
    }

    if (*entry.iterator == entry.producer->end())
    {
      entry.exhausted = true;

      ++metrics_.producerExhaustions;

      return false;
    }

    entry.rows.push_back(**entry.iterator);

    ++metrics_.rowsProduced;

    return true;
  }

  std::optional<SharedBlockRow>
  SharedBlockIteratorManager::rowAt(
      const SharedBlockCacheKey &key,
      size_t index)
  {
    ++metrics_.rowRequests;

    Entry &entry =
        getOrCreateEntry(key);

    if (index < entry.rows.size())
    {
      ++metrics_.cachedRowsReturned;
      return entry.rows[index];
    }

    while (
        index >= entry.rows.size() &&
        !entry.exhausted)
    {
      if (!produceOne(entry))
      {
        break;
      }
    }

    if (index >= entry.rows.size())
    {
      return std::nullopt;
    }

    return entry.rows[index];
  }

  // ---------------------------------------------------------------------------
  // Shared block structural helpers
  // ---------------------------------------------------------------------------

  std::vector<
      SharedBlockIteratorManager::BoundConstraint>
  SharedBlockIteratorManager::boundConstraints(
      const ib::SharedBlockIR &block,
      ib::SharedBlockIR::EIdx current,
      const LocalAssignment &assignment)
  {
    std::vector<BoundConstraint> result;

    // Prevent duplicate constraints when the edge graph stores both
    // directions and we also inspect incoming edges below.
    std::set<std::pair<size_t, int>>
        seen;

    const auto &graph =
        block.edge_graph;

    // Edges directly stored from current -> other.
    for (const auto &edge :
         graph.adj(current))
    {
      if (edge.adjType ==
          common::Adjacency::BoundaryToSB)
      {
        continue;
      }

      auto it =
          assignment.find(edge.other);

      if (it == assignment.end())
      {
        continue;
      }

      auto key =
          std::pair{
              edge.other.val,
              static_cast<int>(
                  edge.adjType)};

      if (!seen.insert(key).second)
      {
        continue;
      }

      result.push_back(
          BoundConstraint{
              .adjacency = edge.adjType,
              .otherDRel = it->second});
    }

    // Also support graphs where only other -> current was stored.
    for (const auto &[otherIdx, otherDRel] :
         assignment)
    {
      for (const auto &edge :
           graph.adj(otherIdx))
      {
        if (edge.other != current)
        {
          continue;
        }

        if (edge.adjType ==
            common::Adjacency::BoundaryToSB)
        {
          continue;
        }

        common::Adjacency reversed =
            common::reverseAdjacency(
                edge.adjType);

        auto key =
            std::pair{
                otherIdx.val,
                static_cast<int>(
                    reversed)};

        if (!seen.insert(key).second)
        {
          continue;
        }

        result.push_back(
            BoundConstraint{
                .adjacency = reversed,
                .otherDRel = otherDRel});
      }
    }

    return result;
  }

  std::optional<ib::SharedBlockIR::EIdx>
  SharedBlockIteratorManager::chooseNextNode(
      const ib::SharedBlockIR &block,
      const LocalAssignment &assignment) const
  {
    const auto &graph =
        block.edge_graph;

    std::optional<ib::SharedBlockIR::EIdx>
        best;

    size_t bestBoundNeighbors = 0;
    size_t bestDegree = 0;

    for (size_t i = 0;
         i < graph.num_nodes();
         ++i)
    {
      ib::SharedBlockIR::EIdx idx{i};

      if (assignment.contains(idx))
      {
        continue;
      }

      size_t boundNeighbors = 0;
      size_t degree = 0;

      for (const auto &edge :
           graph.adj(idx))
      {
        if (edge.adjType ==
            common::Adjacency::BoundaryToSB)
        {
          continue;
        }

        ++degree;

        if (assignment.contains(edge.other))
        {
          ++boundNeighbors;
        }
      }

      // Also count incoming-only edges.
      for (const auto &[otherIdx, _] :
           assignment)
      {
        for (const auto &edge :
             graph.adj(otherIdx))
        {
          if (
              edge.other == idx &&
              edge.adjType !=
                  common::Adjacency::BoundaryToSB)
          {
            ++boundNeighbors;
          }
        }
      }

      // Once matching has started, maintain a connected
      // processed component.
      if (
          !assignment.empty() &&
          boundNeighbors == 0)
      {
        continue;
      }

      if (
          !best ||
          boundNeighbors >
              bestBoundNeighbors ||
          (boundNeighbors ==
               bestBoundNeighbors &&
           degree > bestDegree) ||
          (boundNeighbors ==
               bestBoundNeighbors &&
           degree == bestDegree &&
           idx.val < best->val))
      {
        best = idx;
        bestBoundNeighbors =
            boundNeighbors;
        bestDegree = degree;
      }
    }

    if (!best &&
        assignment.size() <
            graph.num_nodes())
    {
      throw std::logic_error(
          "Shared block edge graph is disconnected "
          "from its already matched component");
    }

    return best;
  }

  k::RelationshipCursor
  SharedBlockIteratorManager::candidateCursor(
      const std::vector<BoundConstraint>
          &constraints)
  {
    // New root relationship.
    if (constraints.empty())
    {
      return db_.getRelationshipCursor();
    }

    // Use one already-bound neighbor to create a
    // narrow adjacency cursor.
    const BoundConstraint &source =
        constraints.front();

    k::DNodeId sharedNode =
        db_.endpoint(
            source.otherDRel,
            common::neighborEndpoint(
                source.adjacency));

    common::Endpoint candidateEndpoint =
        common::myEndpoint(
            source.adjacency);

    return db_.adjacent(
        sharedNode,
        candidateEndpoint ==
                common::Endpoint::Start
            ? common::Direction::Outgoing
            : common::Direction::Incoming);
  }

  bool
  SharedBlockIteratorManager::satisfiesConstraints(
      k::DRelId candidate,
      const std::vector<BoundConstraint>
          &constraints)
  {
    for (const auto &constraint :
         constraints)
    {
      if (!areAdjacent(
              db_,
              candidate,
              constraint.otherDRel,
              constraint.adjacency))
      {
        return false;
      }
    }

    return true;
  }

  // ---------------------------------------------------------------------------
  // Shared block DFS
  // ---------------------------------------------------------------------------

  SharedBlockIteratorManager::Producer
  SharedBlockIteratorManager::enumerateBlockDFS(
      const ib::SharedBlockIR &block,
      LocalAssignment &assignment,
      std::unordered_set<
          k::DRelId,
          IntStrongHash> &usedRelationships)
  {
    size_t depth =
        assignment.size();

    auto &calls =
        metrics_
            .recursiveCallsAtDepth[block.block_id];

    if (calls.size() <= depth)
    {
      calls.resize(
          depth + 1,
          0);
    }

    ++calls[depth];

    if (
        assignment.size() ==
        block.edge_graph.num_nodes())
    {
      co_yield SharedBlockRow{
          .relationships =
              assignment};

      co_return;
    }

    auto nextOpt =
        chooseNextNode(
            block,
            assignment);

    if (!nextOpt)
    {
      co_return;
    }

    ib::SharedBlockIR::EIdx qrel =
        *nextOpt;

    std::vector<BoundConstraint>
        constraints =
            boundConstraints(
                block,
                qrel,
                assignment);

    k::RelationshipCursor cursor =
        candidateCursor(
            constraints);

    k::DRelId drel{0};

    while (cursor.next(drel))
    {
      ++metrics_
            .candidateRelationshipsVisited;

      if (usedRelationships.contains(drel))
      {
        ++metrics_
              .rejectedByUniqueness;

        continue;
      }

      if (!satisfiesConstraints(
              drel,
              constraints))
      {
        ++metrics_
              .rejectedByAdjacency;

        continue;
      }

      ib::PredicateId predicate =
          block.edge_graph.node_at(qrel);

      if (!predicateEvaluator_.evaluate(
              predicate,
              drel))
      {
        ++metrics_
              .rejectedByPredicate;

        continue;
      }

      assignment.emplace(
          qrel,
          drel);

      usedRelationships.insert(
          drel);

      for (auto &&row :
           enumerateBlockDFS(
               block,
               assignment,
               usedRelationships))
      {
        co_yield SharedBlockRow{
            row};
      }

      usedRelationships.erase(
          drel);

      assignment.erase(
          qrel);
    }
  }

  SharedBlockIteratorManager::Producer
  SharedBlockIteratorManager::makeProducer(
      qg::BlockId block_id,
      std::optional<k::DRelId> boundary)
  {
    const ib::SharedBlockIR &block =
        blocks_.at(block_id);

    LocalAssignment assignment;

    std::unordered_set<
        k::DRelId,
        IntStrongHash>
        usedRelationships;

    if (block.boundary_node)
    {
      if (!boundary)
      {
        throw std::logic_error(
            "Shared block requires a boundary DRel");
      }

      ib::SharedBlockIR::EIdx innerBoundary =
          *block.boundary_node;

      ib::PredicateId boundaryPredicate =
          block.edge_graph.node_at(
              innerBoundary);

      // The duplicated inner boundary still has to satisfy
      // the block-local predicate.
      if (!predicateEvaluator_.evaluate(
              boundaryPredicate,
              *boundary))
      {
        co_return;
      }

      assignment.emplace(
          innerBoundary,
          *boundary);

      usedRelationships.insert(
          *boundary);
    }
    else if (boundary)
    {
      throw std::logic_error(
          "Boundary DRel supplied to boundary-free shared block");
    }

    for (auto &&row :
         enumerateBlockDFS(
             block,
             assignment,
             usedRelationships))
    {
      co_yield SharedBlockRow{
          row};
    }
  }

  // ===========================================================================
  // Executor structural helpers
  // ===========================================================================

  bool Executor::sharedBlockEligible(
      const ib::collapsed::SharedBlockReference &reference,
      const Embedding &embedding) const
  {
    const ib::SharedBlockIR &block = buildResult_
                                         .shared_block_registry
                                         .at(reference.block_id);

    if (!block.boundary_node)
    {
      return true;
    }

    return embedding.relationships.contains(reference.boundary_node);
  }

  std::vector<Executor::BoundConstraint>
  Executor::regularBoundConstraints(
      const ib::collapsed::EdgeGraphCollapsed &edgeGraph,
      ib::collapsed::EIdx current,
      const Embedding &embedding,
      const std::unordered_set<
          ib::collapsed::EIdx,
          IntStrongHash> &processed)
  {
    std::vector<BoundConstraint> result;

    // (collapsed_idx, adjtype) set just in case there are duplicate edge graph edges
    std::set<std::pair<ib::collapsed::EIdx, int>> seen;
    for (const auto &edge : edgeGraph.adj(current))
    {
      if (!isOrdinaryAdjacency(edge.adjType))
        continue;

      if (!processed.contains(edge.other))
        continue;

      auto otherNode = edgeGraph.node_at(edge.other);
      const auto *otherRel = std::get_if<ib::PatternRelationshipIR>(&otherNode);
      if (!otherRel)
        continue;

      if (!embedding.relationships.contains(otherRel->lv))
        continue;

      if (!seen.insert({edge.other, (int)edge.adjType}).second)
      {
        // duplicate edge
        continue;
      }
      result.push_back(BoundConstraint{
          .adjacency = edge.adjType,
          .otherDRel = embedding.relationships.at(otherRel->lv)});
    }

    return result;
  }

  k::RelationshipCursor Executor::regularCandidateCursor(const std::vector<BoundConstraint> &constraints)
  {
    if (constraints.empty())
      return cachedDatabase_.getRelationshipCursor();
    const BoundConstraint &source = constraints.front();

    k::DNodeId sharedNode = cachedDatabase_.endpoint(source.otherDRel, common::neighborEndpoint(source.adjacency));

    common::Endpoint myEndpoint = common::myEndpoint(source.adjacency);
    return cachedDatabase_.adjacent(sharedNode, directionForEndpoint(myEndpoint));
  }

  bool Executor::satisfiesConstraints(k::DRelId candidate, const std::vector<BoundConstraint> &constraints)
  {
    for (const auto &constraint : constraints)
    {
      if (!areAdjacent(
              cachedDatabase_,
              candidate,
              constraint.otherDRel,
              constraint.adjacency))
      {
        return false;
      }
    }

    return true;
  }

  // ---------------------------------------------------------------------------
  // Dynamic structural order
  // ---------------------------------------------------------------------------

  std::optional<ib::collapsed::EIdx> Executor::chooseNextOuterNode(
      const ib::collapsed::EdgeGraphCollapsed &edgeGraph,
      const Embedding &embedding,
      const std::unordered_set<ib::collapsed::EIdx, IntStrongHash> &processed) const
  {
    const size_t n = edgeGraph.num_nodes();

    /**
     * either
     * - not a SB reference
     * - if SB reference: whether boundary node has already been matched
     */
    auto eligible = [&](ib::collapsed::EIdx idx)
    {
      auto node = edgeGraph.node_at(idx);
      if (auto *block = std::get_if<ib::collapsed::SharedBlockReference>(&node))
      {
        return sharedBlockEligible(*block, embedding);
      }
      return true;
    };

    if (processed.empty())
    {
      // no edge graph nodes have been processed yet.
      // choose best regular node (which is not a SharedBlockReference)
      std::optional<ib::collapsed::EIdx> bestRegular;
      size_t bestDegree = 0;
      for (size_t i = 0; i < n; ++i)
      {
        ib::collapsed::EIdx idx{i};
        auto node = edgeGraph.node_at(idx);

        if (!std::holds_alternative<ib::PatternRelationshipIR>(node))
          continue;

        size_t degree = edgeGraph.adj(idx).size();

        if (!bestRegular || degree > bestDegree)
        {
          bestRegular = idx;
          bestDegree = degree;
        }
      }
      if (bestRegular)
      {
        return bestRegular;
      }

      // The whole graph is a shared block reference.
      if (edgeGraph.num_nodes() != 1 || !eligible(ib::collapsed::EIdx(0)))
        throw std::logic_error("Collapsed graph has no valid starting node");
      return ib::collapsed::EIdx(0);
    }

    // some EIdxs have already been processed. Pick next node from 'frontier',
    // ie nodes adjacent to those already processed
    std::unordered_set<ib::collapsed::EIdx, IntStrongHash> frontier;
    std::unordered_map<
        ib::collapsed::EIdx,
        size_t,
        IntStrongHash>
        processedNeighborCount;

    for (ib::collapsed::EIdx done : processed)
    {
      for (const auto &edge : edgeGraph.adj(done))
      {
        if (processed.contains(edge.other))
          continue;
        frontier.insert(edge.other);
        ++processedNeighborCount[edge.other];
      }
    }

    std::optional<ib::collapsed::EIdx> best;
    // (numBoundNeighbors, degree)
    std::pair<int, int> best_key{-1, -1};
    for (ib::collapsed::EIdx idx : frontier)
    {
      if (!eligible(idx))
        continue;
      size_t boundNeighbors = processedNeighborCount.at(idx);
      size_t degree = edgeGraph.adj(idx).size();
      std::pair<int, int> this_key(boundNeighbors, degree);
      if (this_key > best_key)
      {
        best = idx;
        best_key = this_key;
      }
    }

    if (best)
    {
      return best;
    }

    // some nodes have already been processed, but frontier is empty
    // - means that graph was disconnected. (Shouldn't happen)
    throw std::logic_error("couldn't find next node. Edge graph has bad shape / was disconnected");
  }

  // ===========================================================================
  // Outer DFS
  // ===========================================================================

  std::generator<Embedding>
  Executor::enumerateOuterDFS(
      const ib::collapsed::EdgeGraphCollapsed &edgeGraph,
      QueryExecutionMetrics &metrics,

      // VarId -> DRelId map
      Embedding &embedding,

      // seen data relationships
      std::unordered_set<
          k::DRelId,
          IntStrongHash> &usedRelationships,

      // process EIdxes
      std::unordered_set<
          ib::collapsed::EIdx,
          IntStrongHash> &processed)
  {
    // resize depth
    size_t depth = processed.size();
    if (metrics.recursiveCallsAtDepth.size() <= depth)
    {
      metrics.recursiveCallsAtDepth.resize(depth + 1, 0);
    }
    ++metrics.recursiveCallsAtDepth[depth];

    if (depth == edgeGraph.num_nodes())
    {
      ++metrics.completeMatches;
      co_yield Embedding{embedding};
      co_return;
    }

    auto qidx_ = chooseNextOuterNode(edgeGraph, embedding, processed);
    if (!qidx_)
    {
      co_return;
    }
    ib::collapsed::EIdx qidx = *qidx_;

    // only store matching order for first dfs
    if (metrics.matchingOrder.size() == depth)
    {
      metrics.matchingOrder.push_back(qidx);
    }
    auto node = edgeGraph.node_at(qidx);

    // Ordinary relationship
    if (auto *qrel = std::get_if<ib::PatternRelationshipIR>(&node))
    {
      // bound constraints for nodes next to `qidx`, next chosen index
      std::vector<BoundConstraint> constraints = regularBoundConstraints(
          edgeGraph,
          qidx,
          embedding,
          processed);
      k::RelationshipCursor cursor = regularCandidateCursor(constraints);
      k::DRelId drel{0};
      while (cursor.next(drel))
      {
        ++metrics.regularCandidatesVisited;
        if (usedRelationships.contains(drel))
        {
          ++metrics.regularRejectedByUniqueness;
          continue;
        }
        if (!satisfiesConstraints(drel, constraints))
        {
          ++metrics.regularRejectedByAdjacency;
          continue;
        }
        if (!predicateEvaluator_.evaluate(qrel->signatureId, drel))
        {
          ++metrics.regularRejectedByPredicate;
          continue;
        }
        embedding.relationships.emplace(qrel->lv, drel);
        usedRelationships.insert(drel);
        processed.insert(qidx);
        for (auto &&result : enumerateOuterDFS(
                 edgeGraph,
                 metrics,
                 embedding,
                 usedRelationships,
                 processed))
        {
          co_yield Embedding{result};
        }
        processed.erase(qidx);
        usedRelationships.erase(drel);
        embedding.relationships.erase(qrel->lv);
      }
      co_return;
    }


    // Shared block reference
    // get SB reference object
    const auto &reference = std::get<ib::collapsed::SharedBlockReference>(node);
    const ib::SharedBlockIR &block = buildResult_
                                         .shared_block_registry
                                         .at(reference.block_id);
    std::optional<k::DRelId> boundaryDRel;
    if (block.boundary_node)
    {
      if (!embedding.relationships.contains(reference.boundary_node))
      {
        throw std::logic_error(
            "Shared block selected before outer boundary "
            "relationship was matched");
      }
      boundaryDRel = embedding.relationships.at(reference.boundary_node);
    }
    SharedBlockIterator iterator = sharedBlockManager_.newIterator(reference.block_id, boundaryDRel);

    while (auto blockRow = iterator.next())
    {
      ++metrics.sharedBlockRowsVisited;
      bool valid = true;
      std::vector<qg::VariableId> newlyBound;
      for (const auto &[innerIdx, drel] : blockRow->relationships)
      {
        if (!reference.back_mapping.contains(innerIdx)) {
          // blockRow: SB::EIdx -> Drel
          // back_mapping: SB::EIdx -> VarId
          // - by design, back mapping doesn't contain the shared boundary
          //   because it is already matched
          continue;
        }

        qg::VariableId qvar = reference.back_mapping.at(innerIdx);
        if (embedding.relationships.contains(qvar)) {
          // all qvars in the block should be new
          throw std::logic_error("shared block variable appears in outer embedding");
        }

        if (usedRelationships.contains(drel))
        {
          valid = false;
          break;
        }

        embedding.relationships.emplace(qvar, drel);
        usedRelationships.insert(drel);
        newlyBound.push_back(qvar);
      }

      // restore to state before extending with this shared block
      // TODO: refactor this part, maybe 2 functions 'can_extend' and 'extend'.
      // use 2 passes
      if (!valid)
      {
        ++metrics.sharedBlockRowsRejectedByOuterUniqueness;
        for (qg::VariableId qvar : newlyBound)
        {
          k::DRelId drel = embedding.relationships.at(qvar);
          usedRelationships.erase(drel);
          embedding.relationships.erase(qvar);
        }
        continue;
      }

      processed.insert(qidx);
      for (auto &&result :
           enumerateOuterDFS(
               edgeGraph,
               metrics,
               embedding,
               usedRelationships,
               processed))
      {
        co_yield Embedding{result};
      }

      // 'un-extend' the current embedding
      // TODO: refactor this part
      processed.erase(qidx);
      for (qg::VariableId qvar : newlyBound)
      {
        k::DRelId drel = embedding.relationships.at(qvar);
        usedRelationships.erase(drel);
        embedding.relationships.erase(qvar);
      }
    }
  }

  // ===========================================================================
  // Public Executor API
  // ===========================================================================
  std::generator<Embedding> Executor::enumerateQuery(size_t queryIndex)
  {
    if (queryIndex >= buildResult_.edgeGraphs.size())
    {
      throw std::out_of_range("invalid query index");
    }

    QueryExecutionMetrics &metrics = queryMetrics_.at(queryIndex);

    // Reset per-query enumeration statistics.
    metrics = QueryExecutionMetrics{};

    const auto &edgeGraph = buildResult_.edgeGraphs.at(queryIndex);

    Embedding embedding;

    std::unordered_set<k::DRelId, IntStrongHash> usedRelationships;

    std::unordered_set<ib::collapsed::EIdx, IntStrongHash> processed;
    for (auto &&result :
         enumerateOuterDFS(
             edgeGraph,
             metrics,
             embedding,
             usedRelationships,
             processed))
    {
      co_yield Embedding{
          result};
    }
  }

  QueryExecutionResult
  Executor::executeQuery(size_t queryIndex)
  {
    QueryExecutionResult result;
    for (auto &&embedding : enumerateQuery(queryIndex))
    {
      result.matches.push_back(embedding);
    }
    result.metrics = queryMetrics_.at(queryIndex);
    return result;
  }

} // namespace execution