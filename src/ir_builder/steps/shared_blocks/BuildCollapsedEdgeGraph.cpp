#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>
namespace qg = query_graph;
namespace cmn = common;

namespace ir_builder
{
  namespace edge_graph_helpers
  {

    /**
     * return
     * - first: the sb reference object
     * - second: the collapsed node id
     */
    SharedBlockReference create_shared_reference(
        const EdgeGraphRaw &raw_edge_graph,
        const std::unordered_map<ir_builder::EIdxRaw, node_classification::nc, IntStrongHash> &classification_of_node,
        const qg::VariableId &collapsed_boundary_node,
        const BlockOccurrence &block_occurrence)
    {
      /**
       * CREATE NEW SHARED REFERENCE OBJECT
       */
      SharedBlockReference shared_block_reference{
          .boundary_node = collapsed_boundary_node,
          .block_id = block_occurrence.block_id,
          .back_mapping = {}};

      /**
       * construct back mapping
       * - block node -> PatternRelationshipIR
       * - note that since boundary has it's own outer node which is already matched before coming here,
       *   we don't want to backmap it again
       */
      size_t block_eidx = 0;
      for (EIdxRaw node_index_occ : block_occurrence.outer_node_indices)
      {
        if (block_occurrence.boundary.has_value() && block_occurrence.boundary->boundary_outer_index == node_index_occ)
          continue;
        assert(
            std::holds_alternative<node_classification::Inner>(classification_of_node.at(node_index_occ)));

        // NOTE: our strong types aren't default constructible. Therefore must use emplace
        // - this throws error on duplicate keys

        shared_block_reference.back_mapping
            .emplace(
                SharedBlockIR::EIdx(block_eidx),
                raw_edge_graph.node_at(node_index_occ).lv);
        block_eidx++;
      }

      return shared_block_reference;
    }

    /**
     * return
     * - nodes: `vector<collapsed::NodeType>`: nodes in collapsed graph
     * - collapsed_index_of: `std::unordered_map<EIdxRaw, EIdxCollapsed>`: collapsed index of
     *   all boundary / outer nodes. Does NOT include inner nodes as these are in a shared block
     * - dangling_edges: edges from boundary nodes to shared block nodes
     */
    std::tuple<
        std::vector<NodeType>,
        std::unordered_map<EIdxRaw, EIdxCollapsed, IntStrongHash>,
        std::vector<EdgeGraphCollapsed::EGraphEdge>>
    getCollapsedNodes(
        const EdgeGraphRaw &raw_edge_graph,
        const std::vector<BlockOccurrence> block_occurrences,
        const std::unordered_map<ir_builder::EIdxRaw, node_classification::nc, IntStrongHash> &classification_of_node)
    {
      std::vector<NodeType> nodes;
      // Maps raw node index -> collapsed node EIdx for BOUNDARY / OUTER nodes only
      // inner nodes don't need to be mapped
      std::unordered_map<EIdxRaw, EIdxCollapsed, IntStrongHash> collapsed_index_of;
      // boundary -> shared block edges
      std::vector<EdgeGraphCollapsed::EGraphEdge> dangling_edges;

      // occ_id -> SharedBlockReference
      std::unordered_map<size_t, EIdxCollapsed> occ_to_sb_reference_idx;
      auto register_new_node = [&](const NodeType &&node)
      {
        size_t new_id = nodes.size();
        nodes.push_back(node);
        return EIdxCollapsed(new_id);
      };

      for (size_t node_index = 0; node_index < raw_edge_graph.num_nodes(); node_index++)
      {
        if (std::holds_alternative<node_classification::Outer>(classification_of_node.at(EIdxRaw(node_index))))
        {
          EIdxCollapsed new_idx = register_new_node(raw_edge_graph.node_at(EIdxRaw(node_index)));
          collapsed_index_of.emplace(node_index, new_idx);
        }

        else if (const auto *boundary = std::get_if<node_classification::Boundary>(&classification_of_node.at(EIdxRaw(node_index))))
        {
          /**
           * each occurrence must have exactly one boundary
           */
          EIdxCollapsed boundary_idx_collapsed = register_new_node(raw_edge_graph.node_at(EIdxRaw(node_index)));
          collapsed_index_of.emplace(node_index, boundary_idx_collapsed);

          for (size_t occ_id : boundary->occurrence_ids)
          {
            if (!occ_to_sb_reference_idx.contains(occ_id))
            {
              const BlockOccurrence &block_occurrence = block_occurrences.at(occ_id);
              auto new_ref = create_shared_reference(
                  raw_edge_graph,
                  classification_of_node,
                  raw_edge_graph.node_at(EIdxRaw(node_index)).lv,
                  block_occurrence);
              EIdxCollapsed sb_idx_collapsed = register_new_node(std::move(new_ref));
              occ_to_sb_reference_idx.emplace(occ_id, sb_idx_collapsed);
            }
            EIdxCollapsed sb_idx_collapsed = occ_to_sb_reference_idx.at(occ_id);
            // now we need to link (boundary_idx_collapsed) to sb_collapsed_index
            // - edge type is ONE WAY link from boundary to
            dangling_edges.push_back({cmn::Adjacency::BoundaryToSB,
                                      boundary_idx_collapsed,
                                      sb_idx_collapsed});
          }
        }
      }

      return std::make_tuple(nodes, collapsed_index_of, dangling_edges);
    }

    EdgeGraphCollapsed buildCollapsedGraph(
        const EdgeGraphRaw &raw_edge_graph,
        const std::vector<BlockOccurrence> &block_occurrences,
        const std::unordered_map<ir_builder::EIdxRaw, node_classification::nc, IntStrongHash> &classification_of_node)
    {
      auto [nodes, collapsed_index_of, dangling_edges] = getCollapsedNodes(raw_edge_graph, block_occurrences, classification_of_node);
      EdgeGraphCollapsed edge_graph_collapsed(nodes);

      /**
       * add raw edge graph nodes
       */
      for (size_t raw_from = 0; raw_from < raw_edge_graph.num_nodes(); raw_from++)
      {
        if (!collapsed_index_of.contains(EIdxRaw(raw_from)))
          continue;
        EIdxCollapsed collapsed_from = collapsed_index_of.at(EIdxRaw(raw_from));
        for (auto &edge : raw_edge_graph.adj(EIdxRaw(raw_from)))
        {
          if (!collapsed_index_of.contains(edge.other))
            continue;
          // both collapsed_from and collapsed_to are outer / boundary. They should not
          // be a shared block
          EIdxCollapsed collapsed_to = collapsed_index_of.at(edge.other);
          assert(
              std::holds_alternative<PatternRelationshipIR>(nodes[collapsed_from.val]));
          assert(
              std::holds_alternative<PatternRelationshipIR>(nodes[collapsed_to.val]));

          edge_graph_collapsed.add_edge({edge.adjType, collapsed_from, collapsed_to});
        }
      }

      /**
       * add dangling edges
       */
      for (auto &edge : dangling_edges)
      {
        edge_graph_collapsed.add_edge(edge);
      }
      return edge_graph_collapsed;
    }

  } // namespace edge_graph_helpers

} // namespace ir_builder