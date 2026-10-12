#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>
namespace qg = query_graph;
namespace cmn = common;

namespace ir_builder
{
  namespace edge_graph_helpers
  {

    size_t register_new_node(const collapsed::NodeType &node, std::vector<collapsed::NodeType> nodes)
    {
      size_t new_id = nodes.size();
      nodes.push_back(node);
      return new_id;
    };

    /**
     * return
     * - first: the sb reference object
     * - second: the collapsed node id
     */
    SharedBlockReference create_shared_reference(
        const raw::EdgeGraphRaw &raw_edge_graph,
        const std::unordered_map<ir_builder::raw::EIdx, node_classification::nc> &classification_of_node,
        const collapsed::EIdx &collapsed_boundary_node,
        const BlockOccurrence &block_occurrence,

        // modifies these
        std::unordered_map<raw::EIdx, collapsed::EIdx> &collapsed_index,
        std::vector<collapsed::NodeType> &nodes)
    {
      /**
       * CREATE NEW SHARED REFERENCE OBJECT
       */
      SharedBlockReference shared_block_reference{
          .index = 0, // undetermineed yet
          .boundary_node = collapsed_boundary_node,
          .block_id = block_occurrence.block_id,
          .back_mapping = {}};

      /**
       * construct back mapping
       * - block node -> PatternRelationshipIR
       */
      size_t block_eidx = 0;
      for (raw::EIdx node_index_occ : block_occurrence.relationship_indices)
      {
        if (node_index_occ == block_occurrence.boundary_relationship_index)
          continue;
        assert(
            std::holds_alternative<node_classification::Inner>(classification_of_node.at(node_index_occ)));

        shared_block_reference.back_mapping[SharedBlockIR::EIdx(block_eidx)] = raw_edge_graph.node_at(node_index_occ);
        block_eidx++;
      }

      /**
       * register rest of nodes in the shared block
       */
      size_t sb_index_collapsed = nodes.size();
      for (raw::EIdx node_in_occ : block_occurrence.relationship_indices)
      {
        if (node_in_occ == block_occurrence.boundary_relationship_index)
          continue;

        collapsed_index.emplace(node_in_occ, sb_index_collapsed);
      }
      shared_block_reference.index = sb_index_collapsed;
      register_new_node(shared_block_reference, nodes);

      return shared_block_reference;
    }

    // std::unordered_map<ir_builder::raw::EIdx, ir_builder::edge_graph_helpers::node_classification::nc>
    std::vector<collapsed::NodeType> getCollapsedNodes(
        const raw::EdgeGraphRaw &raw_edge_graph,
        const std::vector<BlockOccurrence> block_occurrences,
        const std::unordered_map<ir_builder::raw::EIdx, node_classification::nc> &classification_of_node)
    {

      std::vector<collapsed::NodeType> nodes;

      // Maps original qrel index -> collapsed outer EIdx.
      std::unordered_map<raw::EIdx, collapsed::EIdx> collapsed_index;
      // std::vector<size_t> collapsed_index_of_node(raw_edge_graph.num_nodes());
      std::unordered_set<size_t> created_occs;

      for (size_t node_index = 0; node_index < raw_edge_graph.num_nodes(); node_index++)
      {
        if (std::holds_alternative<node_classification::Outer>(classification_of_node.at(node_index)))
        {
          auto node_index_outer = node_index;
          size_t new_idx = register_new_node(raw_edge_graph.node_at(node_index_outer), nodes);
          collapsed_index.emplace(node_index_outer, new_idx);
        }

        else if (const auto *boundary = std::get_if<node_classification::Boundary>(&classification_of_node.at(node_index)))
        {
          /**
           * each occurrence must have exactly one boundary
           */
          size_t boundary_idx_collapsed = register_new_node(raw_edge_graph.node_at(node_index), nodes);
          collapsed_index.emplace(node_index, boundary_idx_collapsed);
          std::unordered_map<size_t, SharedBlockReference> occ_to_sb_reference;

          for (size_t occ_id : boundary->occurrence_ids)
          {
            const BlockOccurrence &block_occurrence = block_occurrences.at(occ_id);

            PatternRelationshipIR pr;
            SharedBlockReference* sb_reference = nullptr;
            collapsed::EIdx sb_collapsed_index;
            if (!created_occs.contains(occ_id))
            {
              auto new_ref = create_shared_reference(
                  raw_edge_graph,
                  classification_of_node,
                  boundary_idx_collapsed,
                  block_occurrence,
                  collapsed_index,
                  nodes);
              occ_to_sb_reference.emplace(occ_id, new_ref);
              sb_reference = &occ_to_sb_reference.at(occ_id);
              sb_collapsed_index = sb_reference->index;
              created_occs.insert(occ_id);
            }
            else
            {
              sb_reference = &occ_to_sb_reference.at(occ_id);
              sb_collapsed_index = sb_reference->index;
            }
            // now we need to link (boundary_idx_collapsed) to sb_collapsed_index vi
            
          }
        }
      }
    }

  } // namespace edge_graph_helpers

} // namespace ir_builder