#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>

namespace qg = query_graph;


namespace ir_builder {
BuildResult IRBuilder::run(const std::vector<qg::QueryGraph> &queryGraphs, int maxLayer = 3)
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
  auto [signatures, pRegistry] = buildAndInternSignatures(queryGraphs, globalEntities, maxLayer);

  BuildResult buildResult{
      .predicateRegistry = pRegistry,
      .signatures = signatures};
  // final query graphs
  for (auto &[_, rel] : globalEntities.relationships)
  {
    buildResult
        .queryGraphs[rel.queryIndex]
        .patternRelationships
        .push_back(
            PatternRelationshipIR{
                rel.localId,
                rel.globalId,
                rel.localStartNode,
                rel.localEndNode,
                rel.signatureId});
  }
  return buildResult;
}

}