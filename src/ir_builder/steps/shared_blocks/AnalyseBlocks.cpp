#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>
namespace qg = query_graph;
namespace cmn = common;

/**
 * second step of shared block edge graph builder
 */

namespace ir_builder
{
  namespace edge_graph_helpers
  {

    // asdasd
    bool node_in_block(const EIdxRaw node, const std::vector<EIdxRaw> &nodes_in_block)
    {
      return std::find(nodes_in_block.begin(), nodes_in_block.end(), node) != nodes_in_block.end();
    };

    /**
     * get boundary of the shared block
     * - this is an edge graph node which disconnects the graph if removed
     * - this node may not exist, which would mean that the block is the entire query graph. That's also ok
     */
    std::optional<EIdxRaw> get_boundary(const EdgeGraphRaw &edge_graph_raw, const std::vector<EIdxRaw> &nodes_in_block)
    {
      /**
       * find all boundary candidates, those which touch the outside graph
       */
      std::vector<EIdxRaw> boundary_candidates;
      for (EIdxRaw node_index : nodes_in_block)
      {
        bool touches_outside = false;
        for (const auto &edge : edge_graph_raw.adj(node_index))
        {
          if (!node_in_block(edge.other, nodes_in_block))
          {
            touches_outside = true;
            break;
          }
        }

        if (touches_outside)
        {
          boundary_candidates.push_back(node_index);
        }
      }

      if (boundary_candidates.size() > 1)
      {
        throw std::runtime_error(
            "shared block must have <= node "
            "which touches the rest of the query; found " +
            std::to_string(
                boundary_candidates.size()));
      }
      if (boundary_candidates.size() == 1)
      {
        // only one boundary. Find the EIdx of this boundary
        return boundary_candidates[0];
      }

      // no boundary exists
      return std::nullopt;
    }

    /**
     * do basic checks on the validity of the qg::SharedBlock such as
     * - whether block actually includes valid variables in the QG
     * - whether 2 variables occur twice. Could dedupe, but this shouldn't happen
     * - whether a relationship appears in 2 shared block occurrences at once
     *
     * populates global relIdx_to_occurrence_id
     * returns the EIdxs in the block
     */
    std::vector<EIdxRaw> check_and_get_nodes_in_block(
        const std::unordered_map<qg::VariableId, EIdxRaw, IntStrongHash> &varid_to_index,
        // std::unordered_map<EIdxRaw, size_t> node_to_occurrence_id,
        // size_t occurence_id,
        const qg::SharedBlock &block)
    {
      std::vector<EIdxRaw> nodes_in_block;
      for (qg::VariableId varId : block.patternRelationshipIDs)
      {
        if (!varid_to_index.contains(varId))
        {
          throw std::runtime_error("shared block references relationship not present in the query graph");
        }
        EIdxRaw node_index = varid_to_index.at(varId);
        if (node_in_block(node_index, nodes_in_block))
        {
          throw std::runtime_error("same relationship occurs twice inside one shared block annotation");
        }

        nodes_in_block.push_back(node_index);
      }
      if (nodes_in_block.empty())
      {
        throw std::runtime_error("block is empty");
      }

      return nodes_in_block;
    }

    /**
     * Check that the induced shared block is connected using dfs
     * - since the edge graph is bidirectional, should be fine if we just DFS from 0
     */
    void check_connected(const EdgeGraphRaw &raw_adj, const std::vector<EIdxRaw> &nodes_in_block)
    {

      std::unordered_set<EIdxRaw, IntStrongHash> visited;
      std::vector<EIdxRaw> stack{nodes_in_block.at(0)};
      while (!stack.empty())
      {
        EIdxRaw current = stack.back();
        stack.pop_back();
        if (!visited.insert(current).second)
          continue;
        for (const EdgeGraphRaw::EGraphAdjacency &edge : raw_adj.adj(current))
        {
          if (node_in_block(edge.other, nodes_in_block))
          {
            stack.push_back(edge.other);
          }
        }
      }
      if (visited.size() != nodes_in_block.size())
      {
        throw std::runtime_error("relationships inside a shared block do not form a connected edge graph");
      }
    }

    /**
     * change a qg::SharedBlock into a block occurrence
     * - fill in node_to_occ_id with nodes in the new block
     */
    BlockOccurrence process_block(
        const EdgeGraphRaw &raw_adj,
        const std::unordered_map<qg::VariableId, EIdxRaw, IntStrongHash> &varid_to_index,
        const qg::SharedBlock &block)
    {
      if (block.patternRelationshipIDs.empty())
        throw std::runtime_error("shared block contains no relationships");
      std::vector<EIdxRaw> nodes_in_block = check_and_get_nodes_in_block(varid_to_index, block);
      check_connected(raw_adj, nodes_in_block);

      std::optional<BlockOccurrence::Boundary> boundary;
      std::optional<EIdxRaw> outer_boundary_ = get_boundary(raw_adj, nodes_in_block);
      if (outer_boundary_.has_value())
      {
        auto outer_boundary = outer_boundary_.value();
        size_t local_boundary = std::find(nodes_in_block.begin(),
                                          nodes_in_block.end(),
                                          outer_boundary) -
                                nodes_in_block.begin();
        boundary = BlockOccurrence::Boundary {
          .boundary_local_index = local_boundary,
          .boundary_outer_index = outer_boundary
        };        
      }

      return BlockOccurrence{
          .block_id = block.block_id,
          .outer_node_indices = std::move(nodes_in_block),
          .boundary = std::move(boundary)
      };
    }

    // mapping of qg.patternRelationship -> EIdx in our raw edge graph
    std::unordered_map<qg::VariableId, EIdxRaw, IntStrongHash> get_varid_to_eidx(const qg::QueryGraph &queryGraph)
    {
      std::unordered_map<qg::VariableId, EIdxRaw, IntStrongHash> varid_to_index;
      for (size_t qrel_idx = 0; qrel_idx < queryGraph.patternRelationships.size(); ++qrel_idx)
      {
        const auto [_, inserted] = varid_to_index.emplace(queryGraph.patternRelationships[qrel_idx].variable, EIdxRaw(qrel_idx));
        if (!inserted)
        {
          throw std::runtime_error("relationship VariableId occurs more than once in QueryGraph");
        }
      }
      return varid_to_index;
    }

    /**
     * Register the first occurrence of a BlockId.
     *
     * Later occurrences must have:
     *
     * - same local predicate at every local EIdx;
     * - same edge structure;
     * - same boundary EIdx.
     *
     * It is up to the USER to make sure that their shared block definitions are valid
     * Also, we don't check graph isomorphism properly. User must order edges properly. (Could fix this, requiring user to order the edges)
     */
    void registerOrValidateBlock(
        const BlockOccurrence &occurrence,
        const EdgeGraphRaw &edge_graph_raw,
        std::unordered_map<qg::BlockId, SharedBlockIR, IntStrongHash> &shared_block_of,
        std::unordered_map<qg::BlockId, BlockShape, IntStrongHash> &blockshape_of,
        const std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> &signatures)
    {
      auto [block_ir, shape] = makeSharedBlockIR(occurrence, edge_graph_raw, signatures);
      if (!blockshape_of.contains(occurrence.block_id))
      {
        blockshape_of.emplace(occurrence.block_id, shape);
        shared_block_of.emplace(occurrence.block_id, std::move(block_ir));
        return;
      }
      if ((blockshape_of.at(occurrence.block_id) != shape))
      {
        throw std::runtime_error(
            "two SharedBlock occurrences have the same BlockId "
            "but different predicate/adjacency structure");
      }
    }

    std::unordered_map<EIdxRaw, node_classification::nc, IntStrongHash> classify_nodes(
        const std::unordered_map<EIdxRaw, std::vector<size_t>, IntStrongHash> &occs_of_node,
        const std::vector<BlockOccurrence> &block_occurrences)
    {
      std::unordered_map<EIdxRaw, node_classification::nc, IntStrongHash> classification_of_node;
      for (auto &[node, occ_ids] : occs_of_node)
      {
        if (occ_ids.size() == 0)
        {
          classification_of_node.emplace(node, node_classification::Outer{});
        }
        else if (occ_ids.size() == 1)
        {
          size_t occ_id = occ_ids[0];
          auto occ = block_occurrences.at(occ_id);
          if (occ.boundary.has_value() && occ.boundary->boundary_outer_index == node)
          {
            classification_of_node.emplace(node, node_classification::Boundary{.occurrence_ids = {occ_id}});
            continue;
          }
          classification_of_node.emplace(node, node_classification::Inner{.occurrence_id = occ_id});
        }
        else
        {
          classification_of_node.emplace(
              node,
              node_classification::Boundary{
                  .occurrence_ids = occ_ids});
        }
      }
      return classification_of_node;
    }

    std::pair<std::vector<BlockOccurrence>, std::unordered_map<EIdxRaw, node_classification::nc, IntStrongHash>> analyseBlocks(
        const qg::QueryGraph &queryGraph,
        const EdgeGraphRaw &raw_edge_graph,
        std::unordered_map<qg::BlockId, SharedBlockIR, IntStrongHash> &shared_block_of,
        std::unordered_map<qg::BlockId, BlockShape, IntStrongHash> &blockshape_of,
        const std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> &signatures)
    {
      // mapping of qg.patternRelationship -> EIdx in our raw edge graph
      std::unordered_map<qg::VariableId, EIdxRaw, IntStrongHash> varid_to_index = get_varid_to_eidx(queryGraph);
      // mapping of node -> all block occurrences it occurs in
      std::unordered_map<EIdxRaw, std::vector<size_t>, IntStrongHash> occs_of_node;
      for (size_t node = 0; node < raw_edge_graph.num_nodes(); node += 1)
      {
        occs_of_node.emplace(EIdxRaw(node), std::vector<size_t>{});
      }

      // turn user marked regions into BlockOccurrence objects
      std::vector<BlockOccurrence> block_occurrences;
      size_t occurrence_id = 0;
      for (const qg::SharedBlock &block : queryGraph.sharedBlocks)
      {
        BlockOccurrence occ = process_block(raw_edge_graph, varid_to_index, block);
        block_occurrences.push_back(occ);
        for (auto &node_index : occ.outer_node_indices)
        {
          occs_of_node.at(node_index).push_back(occurrence_id);
        }
        occurrence_id++;
      }
      // validate validity of occs_of_node
      // - any node belonging in 2+ occurrences must be the boundary of those occurrences
      for (const auto &[node_index, occs] : occs_of_node)
      {
        if (occs.size() > 2)
        {
          for (size_t occ_id : occs)
          {
            auto& occ = block_occurrences[occ_id];
            if (occ.boundary.has_value() && occ.boundary->boundary_outer_index != node_index)
            {
              throw std::runtime_error("non boundary node belongs to 2+ shared blocks");
            }
          }
        }
      }

      // register block occurrences with global registry
      for (const auto &occurrence : block_occurrences)
      {
        registerOrValidateBlock(occurrence, raw_edge_graph, shared_block_of, blockshape_of, signatures);
      }

      auto classification_of_node = classify_nodes(occs_of_node, block_occurrences);
      return {block_occurrences, classification_of_node};
    }
  } // namespace edge_graph_helpers
} // namespace ir_builder