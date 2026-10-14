#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>
namespace qg = query_graph;
namespace cmn = common;

namespace ir_builder
{
  namespace egh = edge_graph_helpers;

  collapsed::EdgeGraphCollapsed buildEdgeGraph(
      const qg::QueryGraph &queryGraph,
      const size_t qi,
      const GlobalEntities &ge,
      const std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> &signatures,
      std::unordered_map<qg::BlockId, SharedBlockIR, IntStrongHash> &shared_block_of,
      std::unordered_map<qg::BlockId, egh::BlockShape, IntStrongHash> &shape_of)
  {
    // 1. Build full, uncollapsed relationship edge graph.
    auto raw_edge_graph = egh::buildRawEdgeGraph(queryGraph, ge.queryLocalToGlobals.at(qi), ge.relationships);

    // 2. Resolve and validate shared-block occurrences.
    // - modify:
    //   - registered_shapes: BlockId->BlockShape
    //   - shared_block_registry: BlockId->SharedBlockIR
    // return node classifications
    auto [block_occurrences, classification_of_node] = egh::analyseBlocks(
        queryGraph,
        raw_edge_graph,
        shared_block_of,
        shape_of,
        signatures);

    // 3. Collapse each shared block occurrence into one outer EdgeGraph node.
    return egh::buildCollapsedGraph(raw_edge_graph, block_occurrences, classification_of_node);
  }

  std::pair<
      std::vector<collapsed::EdgeGraphCollapsed>,
      std::unordered_map<qg::BlockId, SharedBlockIR>>
  buildCollapsedEdgeGraphs(
      const std::vector<qg::QueryGraph> &queryGraphs,
      const GlobalEntities &ge,
      const std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> &signatures)
  {

    std::unordered_map<qg::BlockId, SharedBlockIR, IntStrongHash> shared_block_of;
    std::unordered_map<qg::BlockId, egh::BlockShape, IntStrongHash> shape_of;

    std::vector<collapsed::EdgeGraphCollapsed> edge_graphs;
    edge_graphs.reserve(queryGraphs.size());

    for (size_t qi = 0; qi < queryGraphs.size(); ++qi)
    {
      edge_graphs.push_back(
          buildEdgeGraph(
              queryGraphs[qi],
              qi,
              ge,
              signatures,
              shared_block_of,
              shape_of));
    }

    return {
        std::move(edge_graphs),
        std::move(shared_block_of)};
  }
}