#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>
namespace qg = query_graph;
namespace cmn = common;

namespace ir_builder
{
  namespace edge_graph_helpers
  {

    /**
     * Build final PatternRelationshipIR object by
     * - inspecting eac qrel queryGraph.patternRelationships
     * - using `local_to_global` and `global_relationships` registry to get required fields
     */
    inline std::vector<PatternRelationshipIR> buildRelationshipIRs(
        const qg::QueryGraph &queryGraph,
        const QueryLocalToGlobal &local_to_global,
        const std::unordered_map<GlobalRelId, PatternRelationshipInternal, IntStrongHash> &global_relationships)
    {
      std::vector<PatternRelationshipIR> result;
      for (const auto &qrel : queryGraph.patternRelationships)
      {
        GlobalRelId global_rel_id = local_to_global.rel_to_globalID.at(qrel.variable);
        const auto &qr_internal = global_relationships.at(global_rel_id);
        result.push_back(
            PatternRelationshipIR{
                .global_rel_id = global_rel_id,
                .lv = qrel.variable,
                .start = qr_internal.localStartNode,
                .end = qr_internal.localEndNode,
                .signatureId = qr_internal.signatureId,
            });
      }
      return result;
    }
    /**
     * Build the complete ordinary edge graph before any shared blocks are
     * collapsed.
     */
    std::vector<std::vector<EdgeGraphRaw::EGraphAdjacency>> buildRawAdjacency(const std::vector<PatternRelationshipIR> &qrels)
    {
      std::vector<std::vector<EdgeGraphRaw::EGraphAdjacency>> adj(qrels.size());
      auto add = [&](size_t a, size_t b, cmn::Adjacency type)
      {
        adj[a].push_back(EdgeGraphRaw::EGraphAdjacency{
            .adjType = type,
            .other = EIdxRaw(b),
        });
        adj[b].push_back(EdgeGraphRaw::EGraphAdjacency{
            .adjType = reverseAdjacency(type),
            .other = EIdxRaw(a),
        });
      };

      for (size_t i = 0; i < qrels.size(); ++i)
      {
        for (size_t j = i + 1;
             j < qrels.size();
             ++j)
        {
          const auto &a = qrels[i];
          const auto &b = qrels[j];

          if (a.start == b.start)
            add(i, j, cmn::Adjacency::StartStart);

          if (a.start == b.end)
            add(i, j, cmn::Adjacency::StartEnd);

          if (a.end == b.start)
            add(i, j, cmn::Adjacency::EndStart);

          if (a.end == b.end)
            add(i, j, cmn::Adjacency::EndEnd);
        }
      }
      return adj;
    }

    EdgeGraphRaw buildRawEdgeGraph(
        const qg::QueryGraph &queryGraph,
        const QueryLocalToGlobal &local_to_global,
        const std::unordered_map<GlobalRelId, PatternRelationshipInternal, IntStrongHash> &global_relationships)
    {
      std::vector<PatternRelationshipIR> qrels = buildRelationshipIRs(queryGraph, local_to_global, global_relationships);
      auto raw_adj = buildRawAdjacency(qrels);
      return EdgeGraphRaw(qrels, raw_adj);
    }
  } // namespace edge_graph_helpers
}