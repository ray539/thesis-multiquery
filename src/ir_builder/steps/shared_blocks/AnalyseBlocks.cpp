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
    bool node_in_block(const raw::EIdx node, const std::vector<raw::EIdx> &nodes_in_block)
    {
      return std::find(nodes_in_block.begin(), nodes_in_block.end(), node) != nodes_in_block.end();
    };

    /**
     * get boundary of the shared block
     * - this is an edge graph node which disconnects the graph
     * -
     */
    raw::EIdx get_boundary(const raw::EdgeGraphRaw &edge_graph_raw, const std::vector<raw::EIdx> &nodes_in_block)
    {
      /**
       * find all boundary candidates, those which touch the outside graph
       */
      std::vector<raw::EIdx> boundary_candidates;
      for (raw::EIdx node_index : nodes_in_block)
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
      raw::EIdx boundary(0);
      if (boundary_candidates.size() == 1)
      {
        // only one boundary. Find the EIdx of this boundary
        boundary = boundary_candidates[0];
      }
      else
      {
        // entire block is disconnected, just choose any block to be the boundary
        boundary = nodes_in_block.at(0);
      }
      return boundary;
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
    std::vector<raw::EIdx> check_and_get_nodes_in_block(
        const std::unordered_map<qg::VariableId, raw::EIdx> &varid_to_index,
        // std::unordered_map<raw::EIdx, size_t> node_to_occurrence_id,
        // size_t occurence_id,
        const qg::SharedBlock &block)
    {
      std::vector<raw::EIdx> nodes_in_block;
      for (qg::VariableId varId : block.patternRelationshipIDs)
      {
        if (!varid_to_index.contains(varId))
        {
          throw std::runtime_error("shared block references relationship not present in the query graph");
        }
        raw::EIdx node_index = varid_to_index.at(varId);
        if (node_in_block(node_index, nodes_in_block))
        {
          throw std::runtime_error("same relationship occurs twice inside one shared block annotation");
        }

        nodes_in_block.push_back(node_index);
      }
      return nodes_in_block;
    }

    /**
     * Check that the induced shared block is connected using dfs
     */
    void check_connected(const raw::EdgeGraphRaw &raw_adj, const std::vector<raw::EIdx> &nodes_in_block, raw::EIdx boundary)
    {

      std::unordered_set<raw::EIdx> visited;
      std::vector<raw::EIdx> stack{boundary};
      while (!stack.empty())
      {
        raw::EIdx current = stack.back();
        stack.pop_back();
        if (!visited.insert(current).second)
          continue;
        for (const raw::EdgeGraphRaw::EGraphAdjacency &edge : raw_adj.adj(current))
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
        const raw::EdgeGraphRaw &raw_adj,
        const std::unordered_map<qg::VariableId, raw::EIdx> &varid_to_index,
        std::unordered_map<raw::EIdx, std::vector<size_t>> &occs_of_node,
        const qg::SharedBlock &block,
        size_t occurence_id)
    {

      if (block.patternRelationshipIDs.empty())
        throw std::runtime_error("shared block contains no relationships");
      std::vector<raw::EIdx> nodes_in_block = check_and_get_nodes_in_block(varid_to_index, block);
      raw::EIdx boundary = get_boundary(raw_adj, nodes_in_block);
      check_connected(raw_adj, nodes_in_block, boundary);

      size_t boundary_local = std::find(nodes_in_block.begin(),
                                        nodes_in_block.end(),
                                        boundary) -
                              nodes_in_block.begin();

      // check validity of the block
      // - for each node n in the block, unless n is a boundary it is not allowed to
      //   be in two blocks
      for (const raw::EIdx &node : nodes_in_block)
      {
        if (occs_of_node.contains(node) && node != boundary)
        {
          throw std::runtime_error("non boundary query relationship belongs to more than one shared block");
        }
        occs_of_node.at(node).push_back(occurence_id); // if it is a neighbor, bad news...
      }

      return BlockOccurrence{
          .block_id = block.block_id,
          .relationship_indices = std::move(nodes_in_block),
          .boundary_local_index = boundary_local,
          .boundary_relationship_index = boundary};
    }

    // mapping of qg.patternRelationship -> EIdx in our raw edge graph
    std::unordered_map<qg::VariableId, raw::EIdx> get_varid_to_eidx(const qg::QueryGraph &queryGraph)
    {
      std::unordered_map<qg::VariableId, raw::EIdx> varid_to_index;
      for (size_t qrel_idx = 0; qrel_idx < queryGraph.patternRelationships.size(); ++qrel_idx)
      {
        const auto [_, inserted] = varid_to_index.emplace(queryGraph.patternRelationships[qrel_idx].variable, raw::EIdx(qrel_idx));
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
        const raw::EdgeGraphRaw &edge_graph_raw,
        std::unordered_map<qg::BlockId, SharedBlockIR> &shared_block_of,
        std::unordered_map<qg::BlockId, BlockShape> &blockshape_of,
        const std::unordered_map<GlobalRelId, PredicateId> &signatures)
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

    std::unordered_map<raw::EIdx, node_classification::nc> classify_nodes(
        const std::unordered_map<raw::EIdx, std::vector<size_t>> &occs_of_node,
        const std::vector<BlockOccurrence> &block_occurrences)
    {
      std::unordered_map<raw::EIdx, node_classification::nc> classification_of_node;
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
          if (occ.boundary_relationship_index == node)
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
    }

    std::unordered_map<raw::EIdx, node_classification::nc> analyseBlocks(
        const qg::QueryGraph &queryGraph,
        const raw::EdgeGraphRaw &raw_edge_graph,
        std::unordered_map<qg::BlockId, SharedBlockIR> &shared_block_of,
        std::unordered_map<qg::BlockId, BlockShape> &blockshape_of,
        const std::unordered_map<GlobalRelId, PredicateId> &signatures)
    {
      // mapping of qg.patternRelationship -> EIdx in our raw edge graph
      std::unordered_map<qg::VariableId, raw::EIdx> varid_to_index = get_varid_to_eidx(queryGraph);
      std::unordered_map<raw::EIdx, std::vector<size_t>> occs_of_node;
      for (auto &node : raw_edge_graph.all_nodes())
      {
        occs_of_node.emplace(node, std::vector<size_t>{});
      }

      std::vector<BlockOccurrence> block_occurrences;
      size_t occurrence_id = 0;
      for (const qg::SharedBlock &block : queryGraph.sharedBlocks)
      {
        BlockOccurrence occ = process_block(raw_edge_graph, varid_to_index, occs_of_node, block, occurrence_id);
        block_occurrences.push_back(occ);
        occurrence_id++;
      }
      // register block occurrences with global registry
      for (const auto &occurrence : block_occurrences)
      {
        registerOrValidateBlock(occurrence, raw_edge_graph, shared_block_of, blockshape_of, signatures);
      }
      auto classification_of_node = classify_nodes(occs_of_node, block_occurrences);
      return classification_of_node;
    }
  } // namespace edge_graph_helpers
} // namespace ir_builder