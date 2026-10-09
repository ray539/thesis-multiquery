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
    bool rel_in_block(raw::EIdx rel, std::vector<raw::EIdx> &rels_in_block)
    {
      return std::find(rels_in_block.begin(), rels_in_block.end(), rel) != rels_in_block.end();
    };

    /**
     * get boundary of the shared block
     * - NOTE: in future, user specifies the boundary, so multiple blocks can share same boundary
     */
    raw::EIdx get_boundary(raw::EdgeGraphRaw &raw_adj, std::vector<raw::EIdx> &rels_in_block)
    {
      /**
       * find all boundary candidates, those which touch the outside graph
       */
      std::vector<raw::EIdx> boundary_candidates;
      for (raw::EIdx qrel_index : rels_in_block)
      {
        bool touches_outside = false;
        for (const auto &edge : raw_adj.adj(qrel_index))
        {
          if (!rel_in_block(edge.other, rels_in_block))
          {
            touches_outside = true;
            break;
          }
        }

        if (touches_outside)
        {
          boundary_candidates.push_back(qrel_index);
        }
      }

      if (boundary_candidates.size() != 1)
      {
        throw std::runtime_error(
            "shared block must have exactly one relationship "
            "which touches the rest of the query; found " +
            std::to_string(
                boundary_candidates.size()));
      }

      // only one boundary. Find the EIdx of this boundary
      raw::EIdx boundary = boundary_candidates[0];
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
    std::vector<raw::EIdx> check_and_get_rels_in_block(
        std::unordered_map<qg::VariableId, raw::EIdx> &varid_to_index,
        std::unordered_map<raw::EIdx, size_t> relIdx_to_occurrence_id,
        size_t occurence_id,
        const qg::SharedBlock &block)
    {
      std::vector<raw::EIdx> rels_in_block;
      for (qg::VariableId varId : block.patternRelationshipIDs)
      {
        if (!varid_to_index.contains(varId))
        {
          throw std::runtime_error("shared block references relationship not present in the query graph");
        }
        raw::EIdx qrel_index = varid_to_index.at(varId);
        if (rel_in_block(qrel_index, rels_in_block))
        {
          throw std::runtime_error("same relationship occurs twice inside one shared block annotation");
        }
        if (relIdx_to_occurrence_id.contains(qrel_index))
        {
          throw std::runtime_error("one query relationship belongs to more than one shared block");
        }
        relIdx_to_occurrence_id[qrel_index] = occurence_id;
        rels_in_block.push_back(qrel_index);
      }
      return rels_in_block;
    }

    /**
     * Check that the induced shared block is connected using dfs
     */
    void check_connected(raw::EdgeGraphRaw &raw_adj, std::vector<raw::EIdx> &rels_in_block, raw::EIdx boundary)
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
          if (rel_in_block(edge.other, rels_in_block))
          {
            stack.push_back(edge.other);
          }
        }
      }
      if (visited.size() != rels_in_block.size())
      {
        throw std::runtime_error("relationships inside a shared block do not form a connected edge graph");
      }
    }

    BlockOccurrence process_block(
        raw::EdgeGraphRaw &raw_adj,
        std::unordered_map<qg::VariableId, raw::EIdx> &varid_to_index,
        std::unordered_map<raw::EIdx, size_t> relIdx_to_occurrence_id,
        const qg::SharedBlock &block, size_t occurence_id)
    {

      if (block.patternRelationshipIDs.empty())
        throw std::runtime_error("shared block contains no relationships");
      std::vector<raw::EIdx> rels_in_block = check_and_get_rels_in_block(varid_to_index, relIdx_to_occurrence_id, occurence_id, block);
      raw::EIdx boundary = get_boundary(raw_adj, rels_in_block);
      check_connected(raw_adj, rels_in_block, boundary);

      size_t boundary_local = std::find(rels_in_block.begin(),
                                                     rels_in_block.end(),
                                                     boundary) -
                                           rels_in_block.begin();

      return BlockOccurrence{
          .block_id = block.block_id,
          .relationship_indices = std::move(rels_in_block),
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


    std::pair<std::vector<BlockOccurrence>, std::unordered_map<raw::EIdx, size_t>> analyseBlocks(
        const qg::QueryGraph &queryGraph,
        raw::EdgeGraphRaw &raw_adj)
    {
      const size_t N = queryGraph.patternRelationships.size();

      // mapping of qg.patternRelationship -> EIdx in our raw edge graph
      std::unordered_map<qg::VariableId, raw::EIdx> varid_to_index = get_varid_to_eidx(queryGraph);
      std::unordered_map<raw::EIdx, size_t> relIdx_to_occurrence_id;
      std::vector<BlockOccurrence> block_occurrences;

      size_t occurrence_id = 0;
      for (const qg::SharedBlock &block : queryGraph.sharedBlocks)
      {
        BlockOccurrence occ = process_block(raw_adj, varid_to_index, relIdx_to_occurrence_id, block, occurrence_id);
        occurrence_id++;
      }

      return {block_occurrences, relIdx_to_occurrence_id};
    }
  } // namespace edge_graph_helpers
} // namespace ir_builder