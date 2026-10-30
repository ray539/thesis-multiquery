#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>

namespace qg = query_graph;

namespace ir_builder
{
  BuildResult IRBuilder::run(const std::vector<qg::QueryGraph> &queryGraphs, int maxLayer)
  {
    if (maxLayer < 0)
    {
      throw CanonicalizationError(
          "maxLayer must be >= 0");
    }

    // phase 1:
    // - local variables -> globally unqiue Ids for nodes and relationships in each query graph
    GlobalEntities globalEntities = buildGlobalEntities(queryGraphs);
    // phase 2
    // - user predicates -> atomic DSL for relationships
    collectPredicates(queryGraphs, globalEntities);
    // phase 3
    // - build signatures
    // previous[relIdx] = predicate id for this relIdx
    auto [signatures, pRegistry] = buildAndInternSignatures(globalEntities, maxLayer);
    // phase 4
    // - build collapsed edge graphs based on level 0 signatures
    auto [edge_graphs, shared_block_registry] = buildCollapsedEdgeGraphs(queryGraphs, globalEntities, signatures.at(0));

    BuildResult buildResult{
      .edgeGraphs = std::move(edge_graphs),
      .predicateRegistry = std::move(pRegistry),
      .signatures = std::move(signatures),
      .shared_block_registry = std::move(shared_block_registry)
    };
    return buildResult;
  }
}