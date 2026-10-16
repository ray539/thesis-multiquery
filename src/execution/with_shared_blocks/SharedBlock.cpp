#include <common/Execution.hpp>

namespace execution
{

  // ===========================================================================
  // SharedBlockIterator
  // ===========================================================================
  std::optional<SharedBlockRow> SharedBlockIterator::next()
  {
    auto row = manager_->rowAt(key_, next_index_);
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
    const ib::SharedBlockIR &block = blocks_.at(block_id);

    // A block with an inner boundary requires an outer DRel.
    if (block.boundary_node.has_value() != boundary.has_value())
    {
      throw std::logic_error("Shared block boundary mismatch");
    }
    ++metrics_.iteratorCreations;
    return SharedBlockIterator{
        this,
        SharedBlockCacheKey{
            .block_id = block_id,
            .boundary = boundary}};
  }

  SharedBlockIteratorManager::Entry &SharedBlockIteratorManager::getOrCreateEntry(const SharedBlockCacheKey &key)
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

  bool SharedBlockIteratorManager::produceOne(Entry &entry)
  {
    if (entry.exhausted)
    {
      return false;
    }
    ++metrics_.producerResumptions;
    if (!entry.iterator.has_value())
    {
      entry.iterator = entry.producer->begin();
    }
    else
    {
      ++(entry.iterator.value());
    }

    if (entry.iterator.value() == entry.producer->end())
    {
      entry.exhausted = true;
      ++metrics_.producerExhaustions;
      return false;
    }
    entry.rows.push_back(*entry.iterator.value());
    ++metrics_.rowsProduced;
    return true;
  }

  std::optional<SharedBlockRow>
  SharedBlockIteratorManager::rowAt(const SharedBlockCacheKey &key, size_t index)
  {
    ++metrics_.rowRequests;
    Entry &entry = getOrCreateEntry(key);
    if (index < entry.rows.size())
    {
      // enough rows, just return one
      ++metrics_.cachedRowsReturned;
      return entry.rows[index];
    }

    while (index >= entry.rows.size() && !entry.exhausted)
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
  // TODO: this looks like a clone of the functions in eval...
  // ---------------------------------------------------------------------------

  std::optional<ib::SharedBlockIR::EIdx> SharedBlockIteratorManager::chooseNextNode(
      const ib::SharedBlockIR &block,
      const std::unordered_map<
          ib::SharedBlockIR::EIdx,
          k::DRelId,
          IntStrongHash> &assignment) const
  {
    const auto &graph = block.edge_graph;
    std::vector<size_t> all_indices(graph.num_nodes());
    std::iota(all_indices.begin(), all_indices.end(), 0);

    std::optional<size_t> best = dfs_helper::indexWhichArgmaxes(
        all_indices,
        [&](size_t i)
        {
          return !assignment.contains(ib::SharedBlockIR::EIdx(i));
        },
        [&](size_t i)
        {
          size_t boundNeighbors = 0;
          size_t degree = 0;
          for (const auto &edge : graph.adj(ib::SharedBlockIR::EIdx(i)))
          {
            // shouldn't exist in edge graph
            assert(edge.adjType != common::Adjacency::BoundaryToSB);
            ++degree;
            if (assignment.contains(edge.other))
            {
              ++boundNeighbors;
            }
          }
          return boundNeighbors * 1000 + degree;
        });

    if (!best && assignment.size() < graph.num_nodes())
    {
      // haven't exhausted all nodes, but nothing to choose
      throw std::logic_error(
          "Shared block edge graph is disconnected "
          "from its already matched component");
    }

    return ib::SharedBlockIR::EIdx(*best);
  }

  // ---------------------------------------------------------------------------
  // Shared block DFS
  // ---------------------------------------------------------------------------

  SharedBlockIteratorManager::Producer
  SharedBlockIteratorManager::enumerateBlockDFS(
      const ib::SharedBlockIR &block,

      std::unordered_map<
          ib::SharedBlockIR::EIdx,
          k::DRelId,
          IntStrongHash> &assignment,

      std::unordered_set<
          k::DRelId,
          IntStrongHash> &usedRelationships)
  {
    size_t depth = assignment.size();
    auto &calls = metrics_.recursiveCallsAtDepth[block.block_id];
    if (calls.size() <= depth)
    {
      calls.resize(depth + 1, 0);
    }
    ++calls[depth];
    if (assignment.size() == block.edge_graph.num_nodes())
    {
      co_yield SharedBlockRow{
          .relationships = assignment};
      co_return;
    }
    auto nextOpt = chooseNextNode(block, assignment);
    if (!nextOpt)
    {
      co_return;
    }

    ib::SharedBlockIR::EIdx qrel = *nextOpt;

    std::vector<dfs_helper::BoundConstraint> constraints = dfs_helper::SBBoundConstraints(block.edge_graph, qrel, assignment);
    k::RelationshipCursor cursor = dfs_helper::regularCandidateCursor(constraints, db_);
    k::DRelId drel{0};
    while (cursor.next(drel))
    {
      ++metrics_.candidateRelationshipsVisited;
      if (usedRelationships.contains(drel))
      {
        ++metrics_.rejectedByUniqueness;
        continue;
      }

      if (!satisfiesConstraints(drel, constraints, db_))
      {
        ++metrics_.rejectedByAdjacency;
        continue;
      }

      ib::PredicateId predicate = block.edge_graph.node_at(qrel);

      if (!predicateEvaluator_.evaluate(predicate, drel))
      {
        ++metrics_.rejectedByPredicate;
        continue;
      }

      assignment.emplace(qrel, drel);
      usedRelationships.insert(drel);

      for (auto &&row :
           enumerateBlockDFS(
               block,
               assignment,
               usedRelationships))
      {
        co_yield SharedBlockRow{row};
      }

      usedRelationships.erase(drel);
      assignment.erase(qrel);
    }
  }

  SharedBlockIteratorManager::Producer
  SharedBlockIteratorManager::makeProducer(qg::BlockId block_id, std::optional<k::DRelId> boundary)
  {
    const ib::SharedBlockIR &block = blocks_.at(block_id);
    std::unordered_map<
        ib::SharedBlockIR::EIdx,
        k::DRelId,
        IntStrongHash>
        assignment;

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

      ib::SharedBlockIR::EIdx innerBoundary = *block.boundary_node;
      ib::PredicateId boundaryPredicate = block.edge_graph.node_at(innerBoundary);
      // The duplicated inner boundary still has to satisfy
      // the block-local predicate.
      if (!predicateEvaluator_.evaluate(boundaryPredicate, *boundary))
      {
        co_return;
      }
      assignment.emplace(innerBoundary, *boundary);
      usedRelationships.insert(*boundary);
    }
    else if (boundary)
    {
      throw std::logic_error("Boundary DRel supplied to boundary-free shared block");
    }

    for (auto &&row :
         enumerateBlockDFS(
             block,
             assignment,
             usedRelationships))
    {
      co_yield SharedBlockRow{row};
    }
  }

} // namespace execution