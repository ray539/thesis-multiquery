#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>
namespace qg = query_graph;
namespace cmn = common;

/**
 * third step of shared block edge graph builder
 */

namespace ir_builder
{
  namespace edge_graph_helpers
  {
    /**
     * extract predicates from the `BlockOccurrence`.
     * - give in signatures because we need to use the LEVEL 0 predicate, not the level k predicate shown on
     * later: might treat each input shared block as another query graph, and map to those predicates
     */
    std::vector<PredicateId> extract_predicate_ids(const raw::EdgeGraphRaw &edge_graph_raw, const std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> &signatures, const BlockOccurrence &occurrence)
    {
      std::vector<PredicateId> predicate_ids;
      for (raw::EIdx rel_index : occurrence.outer_node_indices)
      {
        GlobalRelId global_rel_id = edge_graph_raw.node_at(rel_index).global_rel_id;
        PredicateId base_predicate = signatures.at(global_rel_id); // later: change to use hierarchical
        predicate_ids.push_back(base_predicate);
      }
      return predicate_ids;
    }

    // build BlockShape, the canonical representation of the shared block
    BlockShape buildBlockShape(
        const BlockOccurrence &occurrence,
        const raw::EdgeGraphRaw &edge_graph_raw,
        const std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> &signatures,
        std::unordered_map<raw::EIdx, SharedBlockIR::EIdx, IntStrongHash> &outer_to_local)
    {
      const size_t block_size = occurrence.outer_node_indices.size();
      BlockShape shape;
      shape.boundary_local_index = occurrence.boundary.has_value()
                                       ? std::make_optional(SharedBlockIR::EIdx(occurrence.boundary->boundary_local_index))
                                       : std::nullopt;

      shape.predicate_ids = extract_predicate_ids(edge_graph_raw, signatures, occurrence);

      for (size_t local_index = 0; local_index < block_size; ++local_index)
      {
        raw::EIdx outer_index = occurrence.outer_node_indices[local_index];
        for (const raw::EdgeGraphRaw::EGraphAdjacency &edge : edge_graph_raw.adj(outer_index))
        {
          if (!outer_to_local.contains(edge.other))
            continue;
        
          SharedBlockIR::EIdx neighbor_local = outer_to_local.at(edge.other);
          shape.edges.emplace_back(
              local_index,
              neighbor_local,
              static_cast<int>(edge.adjType));
        }
      }
      std::sort(shape.edges.begin(), shape.edges.end());
      return shape;
    }

    /**
     * turn block shape into adjacency list.
     * - block shape includes list of edges, so just use that
     */
    std::vector<std::vector<SharedBlockIR::EdgeGraphSB::EGraphAdjacency>> adj_from_shape(BlockShape &shape)
    {
      std::vector<std::vector<SharedBlockIR::EdgeGraphSB::EGraphAdjacency>> adj(shape.predicate_ids.size());
      for (auto [a, b, adjType] : shape.edges)
      {
        adj[a.val].push_back({.adjType = common::Adjacency(adjType),
                              .other = b});
      };
      return adj;
    }

    /**
     * map each edge graph node to index in the shared block
     */
    std::unordered_map<raw::EIdx, SharedBlockIR::EIdx, IntStrongHash> get_outer_to_local(const BlockOccurrence &occurrence)
    {
      std::unordered_map<raw::EIdx, SharedBlockIR::EIdx, IntStrongHash> outer_to_local;
      for (size_t local_index = 0; local_index < occurrence.outer_node_indices.size(); ++local_index)
      {
        outer_to_local.emplace(occurrence.outer_node_indices[local_index], SharedBlockIR::EIdx(local_index));
      }
      return outer_to_local;
    }

    std::pair<SharedBlockIR, BlockShape> makeSharedBlockIR(
        const BlockOccurrence &occurrence,
        const raw::EdgeGraphRaw &edge_graph_raw,
        const std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> &signatures)
    {
      const size_t block_size = occurrence.outer_node_indices.size();
      std::unordered_map<raw::EIdx, SharedBlockIR::EIdx, IntStrongHash> outer_to_local = get_outer_to_local(occurrence);
      BlockShape shape = buildBlockShape(occurrence, edge_graph_raw, signatures, outer_to_local);
      std::vector<std::vector<SharedBlockIR::EdgeGraphSB::EGraphAdjacency>> adj = adj_from_shape(shape);

      SharedBlockIR result{
          .block_id = occurrence.block_id,
          .edge_graph =
              SharedBlockIR::EdgeGraphSB{
                  std::move(shape.predicate_ids),
                  std::move(adj),
              },
          .boundary_node =
              occurrence.boundary.has_value() ? std::make_optional(SharedBlockIR::EIdx{
                                                    occurrence.boundary->boundary_local_index})
                                              : std::nullopt};
      return {
          std::move(result),
          std::move(shape)};
    }

  } // namespace edge_graph_helpers
} // namespace ir_builder