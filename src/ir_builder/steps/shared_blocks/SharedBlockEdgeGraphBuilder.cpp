#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>
namespace qg = query_graph;
namespace cmn = common;

namespace ir_builder
{
  namespace egh = edge_graph_helpers;
  class SharedBlockEdgeGraphBuilder
  {
  private:
    using SignatureLayers = std::vector<std::unordered_map<GlobalRelId, PredicateId>>;
    const SignatureLayers &signatures_;
    std::unordered_map<qg::BlockId, SharedBlockIR>
        shared_block_registry_;
    /**
     * BlockID -> BlockShape. Used internally to check whether 2 blocks are equal in shape
     */
    std::unordered_map<qg::BlockId, egh::BlockShape>
        registered_shapes_;

  public:
    explicit SharedBlockEdgeGraphBuilder(
        const SignatureLayers &signatures)
        : signatures_{signatures}
    {
      if (signatures_.empty())
      {
        throw std::runtime_error(
            "SharedBlockEdgeGraphBuilder requires layer-0 signatures");
      }
    }

  private:
    [[noreturn]]
    static void fail(const std::string &message)
    {
      throw std::runtime_error(
          "SharedBlockEdgeGraphBuilder: " + message);
    }

    collapsed::EdgeGraphCollapsed buildEdgeGraph(
        const qg::QueryGraph &queryGraph,
        size_t qi,
        GlobalEntities &ge)
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
          shared_block_registry_,
          registered_shapes_,
          signatures_.at(0));

      // 3. Collapse each shared block occurrence into one outer EdgeGraph node.
      return egh::buildCollapsedGraph(raw_edge_graph, block_occurrences, classification_of_node);
    }

  public:
    /**
     * Builds:
     *
     * 1. one collapsed EdgeGraph per query;
     * 2. the global context-independent SharedBlockIR registry.
     */
    std::pair<
        std::vector<collapsed::EdgeGraphCollapsed>,
        std::unordered_map<qg::BlockId, SharedBlockIR>>
    build(
        const std::vector<qg::QueryGraph> &queryGraphs,
        GlobalEntities &ge)
    {
      shared_block_registry_.clear();
      registered_shapes_.clear();

      std::vector<collapsed::EdgeGraphCollapsed> edge_graphs;
      edge_graphs.reserve(queryGraphs.size());

      for (size_t qi = 0; qi < queryGraphs.size(); ++qi)
      {
        edge_graphs.push_back(
            buildEdgeGraph(
                queryGraphs[qi],
                qi,
                ge));
      }

      return {
          std::move(edge_graphs),
          std::move(shared_block_registry_)};
    }
  };
}