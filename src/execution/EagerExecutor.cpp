#include <common/Execution.hpp>

namespace execution
{
  EagerExecutor::EagerExecutor(
      const ib::BuildResult &buildResult,
      k::CachedDatabaseAccess &database,
      PredicateEvaluator &predicateEvaluator,
      EagerExecutorOptions options)
      : buildResult_{buildResult},
        database_{database},
        predicateEvaluator_{predicateEvaluator},
        options_{options},
        candidateSpace_{
            CandidateSpaceBuilder{
                buildResult_,
                database_,
                predicateEvaluator_}
                .build(options_.signatureLevel)}
  {
    std::cout << "EagerExecutor::constructor" << std::endl;
    std::cout << "Candidate space: " << std::endl;
    std::cout << to_string(candidateSpace_) << std::endl;
  }

  // ---------------------------------------------------------------------------
  // Public execution
  // ---------------------------------------------------------------------------

  std::vector<Embedding> EagerExecutor::executeQuery(size_t queryIndex)
  {
    if (queryIndex >= buildResult_.edgeGraphs.size())
    {
      throw std::out_of_range(
          "EagerExecutor::executeQuery: invalid query index");
    }

    metrics_ = EagerExecutionMetrics{};

    const ib::EdgeGraphCollapsed &edgeGraph =
        buildResult_.edgeGraphs.at(queryIndex);

    Embedding embedding;

    std::unordered_set<
        k::DRelId,
        IntStrongHash>
        usedRelationships;

    std::unordered_set<
        ib::EIdxCollapsed,
        IntStrongHash>
        alreadyBound;

    std::vector<Embedding> output;

    backtrack(
        queryIndex,
        edgeGraph,
        embedding,
        usedRelationships,
        alreadyBound,
        output);

    return output;
  }

  // ---------------------------------------------------------------------------
  // Main backtracking
  // ---------------------------------------------------------------------------

  void EagerExecutor::backtrack(
      size_t queryIndex,
      const ib::EdgeGraphCollapsed &edgeGraph,
      Embedding &embedding,
      std::unordered_set<k::DRelId, IntStrongHash> &usedDataRelationships,
      std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &alreadyBound,
      std::vector<Embedding> &output)
  {
    size_t depth = alreadyBound.size();

    if (metrics_.recursiveCallsAtDepth.size() <= depth)
      metrics_.recursiveCallsAtDepth.resize(depth + 1);

    ++metrics_.recursiveCallsAtDepth[depth];

    std::optional<ib::EIdxCollapsed> qnode_ = chooseNextNode(queryIndex, edgeGraph, alreadyBound);

    // std::cout << "backtrack call" << std::endl;
    // std::cout << "  already bound: ";
    // for (auto& x : alreadyBound) {
    //   std::cout << ir_builder::to_string(x) << " ";
    // }
    // std::cout << std::endl;
    // std::cout << "  next node chosen: " << (qnode_.has_value() ? ir_builder::to_string(*qnode_) : "") << std::endl;


    if (!qnode_)
    {
      ++metrics_.completeMatches;
      output.push_back(embedding);
      return;
    }
    ib::EIdxCollapsed qnode = *qnode_;
    // Ordering is deterministic for all branches in this implementation.
    if (metrics_.matchingOrder.size() == depth)
      metrics_.matchingOrder.push_back(qnode);

    ib::PatternRelationshipIR queryRelationship = relationshipAt(edgeGraph, qnode);
    ib::PredicateId qnodePredicate = predicateOf(queryIndex, queryRelationship);
    std::vector<BoundNeighbor> qnodeNeighbors = boundNeighbors(queryIndex, edgeGraph, qnode, embedding, alreadyBound);

    // expansion candidate
    const std::vector<k::DRelId> *expansionCandidates = chooseExpansionCandidates(qnodePredicate, qnodeNeighbors);
    /**
     * If there are no bound neighbors, this relationship begins a new connected
     * component and we scan the whole candidate set.
     * - this happens when it is the first node being matched
     */
    if (expansionCandidates == nullptr)
    {
      expansionCandidates = &candidateSpace_.candidates(qnodePredicate);
    }
    // mark this node as bound for next recursive call
    alreadyBound.insert(qnode);
    for (k::DRelId candidate : *expansionCandidates)
    {
      ++metrics_.candidatesVisited;
      if (usedDataRelationships.contains(candidate))
      {
        ++metrics_.rejectedByUniqueness;
        continue;
      }
      if (!satisfiesBoundNeighbors(candidate, qnodeNeighbors))
      {
        ++metrics_.rejectedByAdjacency;
        continue;
      }

      auto [embeddingIt, embeddingInserted] = embedding.relationships.emplace(queryRelationship.lv, candidate);
      if (!embeddingInserted)
      {
        throw std::logic_error(
            "EagerExecutor: query relationship was already bound");
      }
      auto [usedIt, usedInserted] = usedDataRelationships.insert(candidate);

      if (!usedInserted)
      {
        throw std::logic_error(
            "EagerExecutor: relationship uniqueness bookkeeping error");
      }

      backtrack(
          queryIndex,
          edgeGraph,
          embedding,
          usedDataRelationships,
          alreadyBound,
          output);

      usedDataRelationships.erase(usedIt);
      embedding.relationships.erase(embeddingIt);
    }

    alreadyBound.erase(qnode);
  }

  // ---------------------------------------------------------------------------
  // Ordering strategy dispatcher
  // ---------------------------------------------------------------------------

  std::optional<ib::EIdxCollapsed> EagerExecutor::chooseNextNode(
      size_t queryIndex,
      const ib::EdgeGraphCollapsed &edgeGraph,
      const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &processed) const
  {
    switch (options_.ordering)
    {
    case EagerOrderingStrategy::SmallestSetFirst:
      return chooseSmallestSetFirst(
          queryIndex,
          edgeGraph,
          processed);

    case EagerOrderingStrategy::ConnectedGreedy:
      return chooseConnectedGreedy(
          queryIndex,
          edgeGraph,
          processed);

    case EagerOrderingStrategy::LazyGreedy:
      return chooseLazyGreedy(
          edgeGraph,
          processed);
    }

    throw std::logic_error(
        "EagerExecutor: unknown ordering strategy");
  }

  // ---------------------------------------------------------------------------
  // Smallest set first
  //
  // Increasing |C(ri)|.
  //
  // No connectivity restriction.
  // ---------------------------------------------------------------------------
  std::optional<ib::EIdxCollapsed> EagerExecutor::chooseSmallestSetFirst(
      size_t queryIndex,
      const ib::EdgeGraphCollapsed &edgeGraph,
      const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &alreadyBound) const
  {
    std::optional<ib::EIdxCollapsed> best;
    size_t bestSize = 0;
    for (size_t i = 0; i < edgeGraph.num_nodes(); ++i)
    {
      ib::EIdxCollapsed node{i};
      if (alreadyBound.contains(node))
        continue;

      size_t size = candidateSetSize(queryIndex, edgeGraph, node);

      if (!best ||
          size < bestSize ||
          (size == bestSize && node.val < best->val))
      {
        best = node;
        bestSize = size;
      }
    }

    return best;
  }

  // ---------------------------------------------------------------------------
  // Connected greedy
  //
  // First node:
  //
  //      minimize |C(ri)| / deg(ri)
  //
  // Subsequent nodes:
  //
  //      only consider nodes with >= 1 bound neighbor
  //
  //      minimize |C(ri)| / (1 + boundDegree(ri))
  //
  // If the current connected component is complete, start a new one using the
  // first-node rule.
  // ---------------------------------------------------------------------------
  std::optional<ib::EIdxCollapsed> EagerExecutor::chooseConnectedGreedy(
      size_t queryIndex,
      const ib::EdgeGraphCollapsed &edgeGraph,
      const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &alreadyBound) const
  {
    std::vector<size_t> indices(edgeGraph.num_nodes());
    std::iota(indices.begin(), indices.end(), 0);

    auto unbound = [&](size_t i)
    {
      return !alreadyBound.contains(ib::EIdxCollapsed{i});
    };

    // First node of a connected component:
    //
    //     minimize |C(u)| / deg(u)
    auto chooseComponentStart = [&]()
    {
      return dfs_helper::indexWhichArgmins(
          indices,
          unbound,      // filter
          [&](size_t i) // scorer
          {
            ib::EIdxCollapsed node{i};
            size_t candidateSize = candidateSetSize(queryIndex, edgeGraph, node);
            size_t nodeDegree = degree(edgeGraph, node);

            if (candidateSize == 0)
              return 0.0L;

            if (nodeDegree == 0)
              return std::numeric_limits<long double>::infinity();

            return static_cast<long double>(candidateSize) /
                   static_cast<long double>(nodeDegree);
          });
    };

    if (alreadyBound.empty())
    {
      auto result = chooseComponentStart();
      if (!result)
        return std::nullopt;
      return ib::EIdxCollapsed{*result};
    }

    // Continue current connected component:
    //
    //     eligible iff boundDegree(u) >= 1
    //
    //     minimize |C(u)| / (1 + boundDegree(u))
    auto best = dfs_helper::indexWhichArgmins(
        indices,
        [&](size_t i) // filter
        {
          ib::EIdxCollapsed node{i};
          return unbound(i) && boundDegree(edgeGraph, node, alreadyBound) > 0;
        },
        [&](size_t i) // scorer
        {
          ib::EIdxCollapsed node{i};
          size_t candidateSize = candidateSetSize(queryIndex, edgeGraph, node);
          size_t nodeBoundDegree = boundDegree(edgeGraph, node, alreadyBound);
          return static_cast<long double>(candidateSize) /
                 static_cast<long double>(1 + nodeBoundDegree);
        });

    if (best)
      return ib::EIdxCollapsed{*best};

    // we assumed one connected component, so if the frontier is empty it can
    // only mean that the graph is fully matched
    if (alreadyBound.size() != edgeGraph.num_nodes()) {
      throw std::logic_error(
        "EagerExecutor: frontier empty before query graph was fully matched");
    }
    return std::nullopt; // ok, matching ends here because we are done
  }

  // ---------------------------------------------------------------------------
  // Lazy greedy
  //
  // Uses no candidate-set cardinality information.
  // This is a copy of the algorithm in with_shared_blocks/Eval.cpp adapted for the lazy executor
  //
  // First:
  //      maximize degree
  //
  // Then, within the current connected component:
  //      maximize (degree, boundDegree)
  //
  // lexicographically.
  // ---------------------------------------------------------------------------
  std::optional<ib::EIdxCollapsed> EagerExecutor::chooseLazyGreedy(
      const ib::EdgeGraphCollapsed &edgeGraph,
      const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &alreadyBound) const
  {
    // ---------------------------------------------------------------------------
    // First node:
    //   choose maximum degree
    // ---------------------------------------------------------------------------
    if (alreadyBound.empty())
    {
      std::vector<size_t> allIndices(edgeGraph.num_nodes());
      std::iota(allIndices.begin(), allIndices.end(), 0);
      auto best = dfs_helper::indexWhichArgmaxes(
          allIndices,
          [](size_t)
          {
            return true;
          },
          [&](size_t i)
          {
            ib::EIdxCollapsed idx{i};
            return static_cast<int64_t>(edgeGraph.adj(idx).size());
          });
      if (!best)
        return std::nullopt;
      return ib::EIdxCollapsed{*best};
    }

    // ---------------------------------------------------------------------------
    // Later nodes:
    //
    // Only consider the frontier, then maximize lexicographically:
    //
    //   (boundDegree, degree)
    // ---------------------------------------------------------------------------
    auto processedNeighborCount = edgeGraph.frontier(alreadyBound);

    std::vector<size_t> indices =
        processedNeighborCount |
        std::views::keys |
        std::views::transform(
            [](ib::EIdxCollapsed idx)
            {
              return idx.val;
            }) |
        std::ranges::to<std::vector<size_t>>();

    auto best = dfs_helper::indexWhichArgmaxes(
        indices,
        [](size_t)
        {
          return true;
        },
        [&](size_t i)
        {
          ib::EIdxCollapsed idx{i};
          size_t boundDegree = processedNeighborCount.at(idx);
          size_t nodeDegree = edgeGraph.adj(idx).size();

          // nodeDegree cannot exceed this multiplier if the edge graph is simple.
          size_t multiplier = edgeGraph.num_nodes() + 1;
          return static_cast<int64_t>( boundDegree * multiplier + nodeDegree);
        });

    if (best)
      return ib::EIdxCollapsed{*best};

    if (alreadyBound.size() != edgeGraph.num_nodes()) {
      throw std::logic_error(
        "EagerExecutor: frontier empty before query graph was fully matched");
    }
    return std::nullopt;
  }

  // ---------------------------------------------------------------------------
  // Query graph statistics
  // ---------------------------------------------------------------------------

  size_t EagerExecutor::degree(
      const ib::EdgeGraphCollapsed &edgeGraph,
      ib::EIdxCollapsed node) const
  {
    std::unordered_set<
        ib::EIdxCollapsed,
        IntStrongHash>
        neighbors;

    for (const auto &edge : edgeGraph.adj(node))
    {
      if (edge.adjType == common::Adjacency::BoundaryToSB)
      {
        throw std::logic_error(
            "EagerExecutor does not support SharedBlockReference nodes");
      }

      neighbors.insert(edge.other);
    }

    return neighbors.size();
  }

  size_t EagerExecutor::boundDegree(
      const ib::EdgeGraphCollapsed &edgeGraph,
      ib::EIdxCollapsed node,
      const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &processed) const
  {
    std::unordered_set<
        ib::EIdxCollapsed,
        IntStrongHash>
        neighbors;

    for (const auto &edge : edgeGraph.adj(node))
    {
      if (edge.adjType == common::Adjacency::BoundaryToSB)
      {
        throw std::logic_error(
            "EagerExecutor does not support SharedBlockReference nodes");
      }

      if (processed.contains(edge.other))
        neighbors.insert(edge.other);
    }

    return neighbors.size();
  }

  size_t EagerExecutor::candidateSetSize(
      size_t queryIndex,
      const ib::EdgeGraphCollapsed &edgeGraph,
      ib::EIdxCollapsed node) const
  {
    ib::PatternRelationshipIR relationship =
        relationshipAt(
            edgeGraph,
            node);

    ib::PredicateId predicate =
        predicateOf(
            queryIndex,
            relationship);

    return candidateSpace_.candidates(predicate).size();
  }

  // ---------------------------------------------------------------------------
  // Query relationship access
  // ---------------------------------------------------------------------------

  ib::PatternRelationshipIR EagerExecutor::relationshipAt(
      const ib::EdgeGraphCollapsed &edgeGraph,
      ib::EIdxCollapsed node) const
  {
    ib::NodeType value =
        edgeGraph.node_at(node);

    const auto *relationship =
        std::get_if<ib::PatternRelationshipIR>(&value);

    if (relationship == nullptr)
    {
      throw std::logic_error(
          "EagerExecutor currently does not support SharedBlockReference nodes");
    }

    return *relationship;
  }

  ib::PredicateId EagerExecutor::predicateOf(
      size_t queryIndex,
      const ib::PatternRelationshipIR &relationship) const
  {
    return candidateSpace_
        .query(queryIndex)
        .predicateOfRelationship
        .at(relationship.global_rel_id);
  }

  // ---------------------------------------------------------------------------
  // Bound-neighbor information
  // ---------------------------------------------------------------------------

  std::vector<EagerExecutor::BoundNeighbor> EagerExecutor::boundNeighbors(
      size_t queryIndex,
      const ib::EdgeGraphCollapsed &edgeGraph,
      ib::EIdxCollapsed qnode,
      const Embedding &embedding,
      const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &alreadyBound) const
  {
    std::vector<BoundNeighbor> result;

    for (const auto &edge : edgeGraph.adj(qnode))
    {
      if (!alreadyBound.contains(edge.other))
        continue;

      if (edge.adjType == common::Adjacency::BoundaryToSB)
      {
        throw std::logic_error(
            "EagerExecutor encountered BoundaryToSB");
      }

      ib::PatternRelationshipIR otherRelationship = relationshipAt(edgeGraph, edge.other);

      ib::PredicateId otherPredicate = predicateOf(queryIndex, otherRelationship);

      k::DRelId otherDRel = embedding.relationships.at(otherRelationship.lv);

      result.push_back(
          BoundNeighbor{
              .boundNode = edge.other,
              .boundNodePredicate = otherPredicate,
              .boundNodeDrel = otherDRel,
              .qnodeToBoundNode = edge.adjType,
          });
    }

    return result;
  }

  // ---------------------------------------------------------------------------
  // Choose which already-bound DRel to expand from
  // ---------------------------------------------------------------------------

  const std::vector<k::DRelId> *EagerExecutor::chooseExpansionCandidates(
      ib::PredicateId currentPredicate,
      const std::vector<BoundNeighbor> &boundNeighbors) const
  {
    const std::vector<k::DRelId> *best = nullptr;
    for (const BoundNeighbor &neighbor : boundNeighbors)
    {

      common::Adjacency boundNodeToQnode = common::reverseAdjacency(neighbor.qnodeToBoundNode);
      const std::vector<k::DRelId> &candidateList =
          candidateSpace_.adj(
              neighbor.boundNodePredicate,
              neighbor.boundNodeDrel,
              currentPredicate,
              boundNodeToQnode);

      if (best == nullptr || candidateList.size() < best->size())
      {
        best = &candidateList;
      }
    }
    return best;
  }

  // ---------------------------------------------------------------------------
  // Check all bound adjacency constraints
  // ---------------------------------------------------------------------------

  bool EagerExecutor::satisfiesBoundNeighbors(
      k::DRelId qnode,
      const std::vector<BoundNeighbor> &boundNeighbors)
  {
    for (const BoundNeighbor &neighbor : boundNeighbors)
    {
      common::Endpoint qnodeEndpoint = common::myEndpoint(neighbor.qnodeToBoundNode);
      common::Endpoint boundnodeEndpoint = common::neighborEndpoint(neighbor.qnodeToBoundNode);

      k::DNodeId candidateNode = database_.endpoint(qnode, qnodeEndpoint);
      k::DNodeId neighborNode = database_.endpoint(neighbor.boundNodeDrel, boundnodeEndpoint);

      if (candidateNode != neighborNode)
        return false;
    }

    return true;
  }

} // namespace execution