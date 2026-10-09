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

    std::vector<PredicateId> extract_predicate_ids(const raw::EdgeGraphRaw &edge_graph_raw, const std::unordered_map<GlobalRelId, PredicateId> &signatures,  const BlockOccurrence &occurrence)
    {
      std::vector<PredicateId> predicate_ids;
      for (size_t occ_rel_idx = 0; occ_rel_idx < occurrence.relationship_indices.size(); ++occ_rel_idx)
      {
        raw::EIdx qrel_index = occurrence.relationship_indices[occ_rel_idx];
        GlobalRelId global_rel_id = edge_graph_raw.node_at(qrel_index).global_rel_id;
        PredicateId base_predicate = signatures.at(global_rel_id); // later: change to use hierarchical
        predicate_ids.push_back(base_predicate.val);
      }
      return predicate_ids;
    }

    // build BlockShape, the canonical representation of the shared block
    BlockShape buildBlockShape(
        const BlockOccurrence &occurrence,
        const raw::EdgeGraphRaw &edge_graph_raw,
        const std::unordered_map<GlobalRelId, PredicateId> &signatures,
        std::unordered_map<raw::EIdx, SharedBlockIR::EIdx> &qrel_to_local)
    {
      const size_t block_size = occurrence.relationship_indices.size();
      BlockShape shape;
      shape.boundary_local_index = occurrence.boundary_local_index;
      shape.predicate_ids = extract_predicate_ids(edge_graph_raw, signatures, occurrence);

      for (size_t occ_rel_idx = 0; occ_rel_idx < block_size; ++occ_rel_idx)
      {
        raw::EIdx qrel_index = occurrence.relationship_indices[occ_rel_idx];
        for (const raw::EdgeGraphRaw::EGraphAdjacency &edge : edge_graph_raw.adj(qrel_index))
        {
          if (!qrel_to_local.contains(edge.other)) continue;
          SharedBlockIR::EIdx neighbor_local = qrel_to_local.at(edge.other);
          shape.edges.emplace_back(
              occ_rel_idx,
              neighbor_local,
              static_cast<int>(edge.adjType));
        }
      }
      std::sort(shape.edges.begin(), shape.edges.end());
      return shape;
    }

    std::vector<std::vector<SharedBlockIR::EdgeGraphSB::EGraphAdjacency>> adj_from_shape(BlockShape& shape) {
      std::vector<std::vector<SharedBlockIR::EdgeGraphSB::EGraphAdjacency>> adj (shape.predicate_ids.size());
      for (auto [a, b, adjType] : shape.edges) {
        adj[a.val].push_back({
          .adjType = common::Adjacency(adjType), 
          .other = b
        });
      };
      return adj;
    }

    /**
     * map each edge graph node (qrel index) to index in the shared block
     */
    std::unordered_map<raw::EIdx, SharedBlockIR::EIdx> get_qrel_to_local(const BlockOccurrence &occurrence) {
      std::unordered_map<raw::EIdx, SharedBlockIR::EIdx> qrel_to_local;
      for (size_t occ_rel_idx = 0; occ_rel_idx < occurrence.relationship_indices.size(); ++occ_rel_idx)
      {
        qrel_to_local.emplace(occurrence.relationship_indices[occ_rel_idx], SharedBlockIR::EIdx(occ_rel_idx));
      }
      return qrel_to_local;
    }

    /**
     * change BlockOccurrence to SharedBlockIR
     */
    std::pair<SharedBlockIR, BlockShape> makeSharedBlockIR(
        const BlockOccurrence &occurrence,
        const raw::EdgeGraphRaw &edge_graph_raw,
        const std::unordered_map<GlobalRelId, PredicateId> &signatures)
    {
      const size_t block_size = occurrence.relationship_indices.size();
      std::unordered_map<raw::EIdx, SharedBlockIR::EIdx> qrel_to_local = get_qrel_to_local(occurrence);
      BlockShape shape = buildBlockShape(occurrence, edge_graph_raw, signatures, qrel_to_local);
      std::vector<std::vector<SharedBlockIR::EdgeGraphSB::EGraphAdjacency>> adj = adj_from_shape(shape);

      SharedBlockIR result{
          .block_id = occurrence.block_id,
          .edge_graph =
              SharedBlockIR::EdgeGraphSB{
                  std::move(shape.predicate_ids),
                  std::move(adj),
              },
          .boundary_node =
              SharedBlockIR::EIdx{
                  occurrence.boundary_local_index},
      };
      return {
          std::move(result),
          std::move(shape)};
    }

  } // namespace edge_graph_helpers
} // namespace ir_builder