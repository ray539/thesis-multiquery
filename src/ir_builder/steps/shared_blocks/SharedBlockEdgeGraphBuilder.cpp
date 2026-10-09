#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>
namespace qg = query_graph;
namespace cmn = common;

namespace ir_builder
{
  namespace egh = edge_graph_helpers;
  class SharedBlockEdgeGraphBuilder
  {
  private:


    using SignatureLayers = std::vector<std::unordered_map<GlobalRelId, PredicateId>>;

    const SignatureLayers &signatures_;

    std::unordered_map<qg::BlockId, SharedBlockIR>
        shared_block_registry_;
    /**
     * BlockID -> BlockShape. Used internally to check whether 2 blocks are equal in shape
     */
    std::unordered_map<qg::BlockId, egh::BlockShape>
        registered_shapes_;

  public:
    explicit SharedBlockEdgeGraphBuilder(
        const SignatureLayers &signatures)
        : signatures_{signatures}
    {
      if (signatures_.empty())
      {
        throw std::runtime_error(
            "SharedBlockEdgeGraphBuilder requires layer-0 signatures");
      }
    }

  private:
    [[noreturn]]
    static void fail(const std::string &message)
    {
      throw std::runtime_error(
          "SharedBlockEdgeGraphBuilder: " + message);
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
     * Also, we don't check graph isomorphism properly. User also has to give the correct matching orders.
     */
    void registerOrValidateBlock(
        const egh::BlockOccurrence &occurrence,
        const raw::EdgeGraphRaw& edge_graph_raw
    )
    {
      auto [block_ir, shape] = egh::makeSharedBlockIR(occurrence, edge_graph_raw, signatures_.at(0));
      if (!registered_shapes_.contains(occurrence.block_id))
      {
        registered_shapes_.emplace(occurrence.block_id, shape);
        shared_block_registry_.emplace(occurrence.block_id, std::move(block_ir));
        return;
      }
      if ((registered_shapes_.at(occurrence.block_id) != shape))
      {
        fail(
            "two SharedBlock occurrences have the same BlockId "
            "but different predicate/adjacency structure");
      }
    }

    collapsed::EdgeGraphCollapsed buildEdgeGraph(
        const qg::QueryGraph &queryGraph,
        size_t qi,
        GlobalEntities &ge)
    {

      // 1. Build full, uncollapsed relationship edge graph.
      auto raw_edge_graph = egh::buildRawEdgeGraph(queryGraph, ge.queryLocalToGlobals.at(qi), ge.relationships);
      // 2. Resolve and validate shared-block occurrences.
      // block_of_relationship[i] = occurrence index
      auto [block_occurrences, block_of_relationship] = egh::analyseBlocks(queryGraph, raw_edge_graph);
      for (const auto &occurrence : block_occurrences)
      {
        registerOrValidateBlock(occurrence, raw_edge_graph);
      }
      // 3. Collapse each shared block occurrence into one outer EdgeGraph node.


      
      std::vector<collapsed::NodeType> nodes;
      // Maps original qrel index -> collapsed outer EIdx.
      std::vector<size_t> outer_index_of_relationship(qrels.size());
      std::unordered_set<qg::BlockId> created_blocks;
      for (size_t qrel_index = 0; qrel_index < qrels.size(); ++qrel_index)
      {
        // Ordinary relationship.
        if (!block_of_relationship.contains(qrel_index))
        {
          size_t outer_index = nodes.size();
          nodes.push_back(qrels[qrel_index]);
          outer_index_of_relationship[qrel_index] = outer_index;
          continue;
        }

        // from now on, we are dealing with a Block. We need to create the SharedBlockreference
        qg::BlockId block_id = block_of_relationship[qrel_index];
        if (created_blocks.contains(block_id))
          continue;
        created_blocks.insert(block_id);

        const egh::BlockOccurrence &block_occurrence = block_occurrences.at(block_id);

        SharedBlockReference shared_block_reference{
            .boundary_node = block_occurrence.boundary_relationship_index,
            .block_id = block_occurrence.block_id,
            .back_mapping = {}
        };
        // back_mapping[k] corresponds exactly to
        // SharedBlockIR::EIdx{k}.
        size_t block_eidx = 0;
        for (EIdx block_qrel_index : block_occurrence.relationship_indices)
        {
          shared_block_reference.back_mapping[SharedBlockIR::EIdx(block_eidx)] = qrels[block_qrel_index.val];
          block_eidx++;
        }

        // Every relationship inside this block occurrence collapses
        // to the same outer EdgeGraph node.
        size_t outer_index = nodes.size();
        nodes.push_back(std::move(shared_block_reference));
        for (EIdx block_qrel_index : block_occurrence.relationship_indices)
        {
          outer_index_of_relationship[block_qrel_index.val] = outer_index;
        }
      }

      // ---------------------------------------------------------
      // 4. Build adjacency of the collapsed quotient edge graph.
      //
      // Internal block edges disappear.
      //
      // Since analyseBlocks() guarantees that only the boundary
      // relationship touches the outside, all surviving edges on a
      // block reference are exactly the edges of its boundary
      // relationship.
      // ---------------------------------------------------------

      std::vector<std::vector<EGraphAdjacency>> adj(nodes.size());
      for (size_t qrel_index = 0; qrel_index < qrels.size(); ++qrel_index)
      {
        size_t outer_from = outer_index_of_relationship[qrel_index];
        for (const RawAdjacency &edge : raw_adj[qrel_index])
        {
          size_t outer_to = outer_index_of_relationship[edge.other.val];
          // Internal to the same collapsed shared block.
          if (outer_from == outer_to)
            continue;
          adj[outer_from].push_back(EGraphAdjacency{
              edge.type,
              outer_to,
          });
        }
      }

      return EdgeGraph{
          std::move(nodes),
          std::move(adj)};
    }

  public:
    /**
     * Builds:
     *
     * 1. one collapsed EdgeGraph per query;
     * 2. the global context-independent SharedBlockIR registry.
     */
    std::pair<
        std::vector<EdgeGraph>,
        std::unordered_map<qg::BlockId, SharedBlockIR>>
    build(
        const std::vector<qg::QueryGraph> &queryGraphs,
        GlobalEntities &ge)
    {
      shared_block_registry_.clear();
      registered_shapes_.clear();

      std::vector<EdgeGraph> edge_graphs;

      edge_graphs.reserve(
          queryGraphs.size());

      for (size_t qi = 0;
           qi < queryGraphs.size();
           ++qi)
      {
        edge_graphs.push_back(
            buildEdgeGraph(
                queryGraphs[qi],
                qi,
                ge));
      }

      return {
          std::move(edge_graphs),
          std::move(shared_block_registry_)};
    }
  };
}