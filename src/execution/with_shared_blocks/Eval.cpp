#include <common/Execution.hpp>

namespace execution
{
  namespace h = dfs_helper;

  // ===========================================================================
  // Executor structural helpers
  // ===========================================================================

  bool Executor::sharedBlockEligible(
      const ib::SharedBlockReference &reference,
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



  std::optional<ib::EIdxCollapsed> Executor::chooseNextOuterNode(
      const ib::EdgeGraphCollapsed &edgeGraph,
      const Embedding &embedding,
      const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &processed) const
  {
    /**
     * either
     * - not a SB reference
     * - if SB reference: whether boundary node has already been matched
     */
    auto eligible = [&](ib::EIdxCollapsed idx)
    {
      auto node = edgeGraph.node_at(idx);
      if (auto *block = std::get_if<ib::SharedBlockReference>(&node))
      {
        return sharedBlockEligible(*block, embedding);
      }
      return true;
    };

    if (processed.empty())
    {
      // no edge graph nodes have been processed yet.
      // choose best regular node (according to degree) (which is not a SharedBlockReference)
      std::vector<size_t> all_indices(edgeGraph.num_nodes());
      std::iota(all_indices.begin(), all_indices.end(), 0);
      std::optional<size_t> bestRegular = h::indexWhichArgmaxes(
          all_indices,
          [&](size_t i)
          {
            return std::holds_alternative<ib::PatternRelationshipIR>(edgeGraph.node_at(ib::EIdxCollapsed(i)));
          },
          [&](size_t i)
          {
            return edgeGraph.adj(ib::EIdxCollapsed(i)).size();
          });
      if (bestRegular.has_value())
      {
        return ib::EIdxCollapsed(*bestRegular);
      }
      // The whole graph is a shared block reference.
      if (edgeGraph.num_nodes() != 1 || !eligible(ib::EIdxCollapsed(0)))
        throw std::logic_error("Collapsed graph has no valid starting node");
      return ib::EIdxCollapsed(0);
    }
    // choose next eligible node (if SB, boundary bound) which maximzies
    // - (boundNeighbors, degree)
    auto processedNeighborCount = edgeGraph.frontier(processed);

    std::vector<size_t> indices = processedNeighborCount
      | std::views::keys
      | std::views::transform(
        [&](ib::EIdxCollapsed e) -> size_t {
          return e.val;          
        })
      | std::ranges::to<std::vector<size_t>>();


    auto best = h::indexWhichArgmaxes(
        indices,
        [&](size_t i)
        {
          ib::EIdxCollapsed idx(i);
          size_t boundNeighbors = processedNeighborCount.at(idx);
          return eligible(ib::EIdxCollapsed(i)) && boundNeighbors >= 1;
        },
        [&](size_t i)
        {
          ib::EIdxCollapsed idx(i);
          size_t boundNeighbors = processedNeighborCount.at(idx);
          size_t degree = edgeGraph.adj(idx).size();
          return boundNeighbors * 10000 + degree;
        });
    if (best.has_value())
    {
      return ib::EIdxCollapsed(*best);
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
      const ib::EdgeGraphCollapsed &edgeGraph,
      QueryExecutionMetrics &metrics,

      // VarId -> DRelId map
      Embedding &embedding,

      // seen data relationships
      std::unordered_set<
          k::DRelId,
          IntStrongHash> &usedRelationships,

      // processed EIdxes
      std::unordered_set<
          ib::EIdxCollapsed,
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

    ib::EIdxCollapsed qidx = *qidx_;
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
      std::vector<h::BoundConstraint> constraints = h::regularBoundConstraints(
          edgeGraph,
          qidx,
          embedding,
          processed);
      k::RelationshipCursor cursor = h::regularCandidateCursor(constraints, cachedDatabase_);
      k::DRelId drel{0};
      while (cursor.next(drel))
      {
        ++metrics.regularCandidatesVisited;
        if (usedRelationships.contains(drel))
        {
          ++metrics.regularRejectedByUniqueness;
          continue;
        }
        if (!satisfiesConstraints(drel, constraints, cachedDatabase_))
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
    const auto &reference = std::get<ib::SharedBlockReference>(node);
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

    auto canExtendEmbeddingWith = [&](const SharedBlockRow &blockRow)
    {
      for (const auto &[innerIdx, drel] : blockRow.relationships)
      {
        if (!reference.back_mapping.contains(innerIdx))
        {
          // back mapping doesn't map back boundary index because it is already matched
          continue;
        }
        if (usedRelationships.contains(drel))
        {
          return false;
        }
      }
      return true;
    };

    auto extendEmbeddingWith = [&](const SharedBlockRow &blockRow)
    {
      for (const auto &[SBIdx, drel] : blockRow.relationships)
      {
        if (!reference.back_mapping.contains(SBIdx))
        {
          // back mapping doesn't map back boundary index because it is already matched
          continue;
        }
        qg::VariableId qvar = reference.back_mapping.at(SBIdx);
        if (embedding.relationships.contains(qvar))
        {
          // all qvars in the block should be new
          throw std::logic_error("shared block variable appears in outer embedding");
        }
        embedding.relationships.emplace(qvar, drel);
        usedRelationships.insert(drel);
      }
    };

    auto unextendEmbeddingWith = [&](const SharedBlockRow &blockRow)
    {
      for (const auto &[SBIdx, drel] : blockRow.relationships)
      {
        if (!reference.back_mapping.contains(SBIdx))
        {
          // back mapping doesn't map back boundary index because it is already matched
          continue;
        }
        qg::VariableId qvar = reference.back_mapping.at(SBIdx);
        assert(embedding.relationships.contains(qvar));
        embedding.relationships.erase(qvar);
        usedRelationships.erase(drel);
      }
    };

    SharedBlockIterator iterator = sharedBlockManager_.newIterator(reference.block_id, boundaryDRel);
    while (auto blockRow = iterator.next())
    {
      ++metrics.sharedBlockRowsVisited;
      if (!canExtendEmbeddingWith(blockRow.value())) {
        continue;
      }
      extendEmbeddingWith(blockRow.value());
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
      unextendEmbeddingWith(blockRow.value());
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
    std::unordered_set<ib::EIdxCollapsed, IntStrongHash> processed;
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