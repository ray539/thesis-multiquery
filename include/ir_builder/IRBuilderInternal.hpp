#pragma once
#include <common/IRBuilder.hpp>
#include <bits/stdc++.h>

namespace ir_builder
{

  struct PatternRelationshipInternal
  {
    size_t queryIndex;

    // Query-local IDs.
    query_graph::VariableId localId;
    query_graph::VariableId localStartNode;
    query_graph::VariableId localEndNode;

    // Globally unique IDs used during canonicalization.
    GlobalRelId globalId;
    GlobalNodeId globalStartNode;
    GlobalNodeId globalEndNode;

    // Layer-0 local predicates.
    std::vector<AtomicRelPredicate>
        atomicPredicates;

    // Final layer's signature.
    PredicateId signatureId{-1};
  };

  struct QueryLocalToGlobal
  {
    // local node VariableId -> GlobalNodeId
    std::unordered_map<query_graph::VariableId, GlobalNodeId> node_to_globalID;
    // local relationship VariableId -> index of relationship in
    std::unordered_map<query_graph::VariableId, GlobalRelId> rel_to_globalID;
  };

  struct GlobalEntities
  {
    // queryStates[qi] gives the local to global mapping information for query qi
    std::vector<QueryLocalToGlobal> queryLocalToGlobals;

    // list of internal pattern relationships
    std::unordered_map<GlobalRelId, PatternRelationshipInternal> relationships;

    // global node id -> relationships incident to it
    std::unordered_map<GlobalNodeId, std::vector<Incidence>> adjListNode;
  };

  inline GlobalEntities buildGlobalEntities(const std::vector<query_graph::QueryGraph> &queryGraphs);

  /**
   * collect all predicates in all the query graphs given and place them in `ge.relationships`
   */
  inline void collectPredicates(
      const std::vector<query_graph::QueryGraph> queryGraphs,
      GlobalEntities &ge);

  struct SignatureBuilderResult
  {
    // signatures[level][global id of relationship] = signature for that relationship
    std::vector<std::unordered_map<GlobalRelId, PredicateId>> signatures;
    // map of predId -> RelPredicate
    PredicateRegistry pRegistry;
  };

  inline SignatureBuilderResult buildAndInternSignatures(
      const std::vector<query_graph::QueryGraph> &queryGraphs,
      GlobalEntities &ge,
      int maxLayers);

  namespace edge_graph_helpers
  {

    /**
     * Think of this class as holding a bunch of pointers to nodes in the outer raw edge graph.
     * These will then be used for further analysis to be turned into compressed shared blocks.
     */
    struct BlockOccurrence
    {
      struct Boundary {
        // local index. Can be used to index outer_node_indices
        size_t boundary_local_index;
        // global index
        raw::EIdx boundary_outer_index;
      };

      query_graph::BlockId block_id;
      // outer edge graph indices
      std::vector<raw::EIdx> outer_node_indices;
      std::optional<Boundary> boundary;
    };

    /**
     * Canonical description used to validate that two occurrences with
     * the same BlockId actually describe the same shared block.
     */
    struct BlockShape
    {
      // Indexed by block-local EIdx.
      std::vector<PredicateId> predicate_ids;
      // Directed edges:
      //   (from local EIdx, to local EIdx, adjacency type)
      // Sorted canonically.
      std::vector<std::tuple<SharedBlockIR::EIdx, SharedBlockIR::EIdx, int>> edges;
      std::optional<SharedBlockIR::EIdx> boundary_local_index;
      bool operator==(const BlockShape &) const = default;
    };

    namespace node_classification
    {
      struct Boundary
      {
        // the node is a boundary, and belongs to the following shared blocks
        std::vector<size_t> occurrence_ids;
      };
      struct Inner
      {
        size_t occurrence_id;
      };
      struct Outer
      {
      };
      typedef std::variant<Boundary, Inner, Outer> nc;
    }

    /**
     * build the edge graph before collapsing shared blocks
     */
    raw::EdgeGraphRaw buildRawEdgeGraph(
        const query_graph::QueryGraph &queryGraph,
        QueryLocalToGlobal &local_to_global,
        std::unordered_map<GlobalRelId, PatternRelationshipInternal> &global_relationships);

    /**
     * This will
     * - turn user given shared block annotations into `BlockOccurrence` objects
     * - register the new blocks in `shared_block_of` and `blockshape_of`
     *
     * return
     * - block_occurrences: `vector<BlockOccurrence>`: list of block occurrence objects
     * - classification_of_node: classifies each node on whether it belongs to the inner, outer or boundary of a shared block
     */
    std::pair<std::vector<BlockOccurrence>, std::unordered_map<raw::EIdx, node_classification::nc>> analyseBlocks(
        const qg::QueryGraph &queryGraph,
        const raw::EdgeGraphRaw &raw_edge_graph,
        std::unordered_map<qg::BlockId, SharedBlockIR> &shared_block_of,
        std::unordered_map<qg::BlockId, BlockShape> &blockshape_of,
        const std::unordered_map<GlobalRelId, PredicateId> &signatures);

    /**
     * turn an `BlockOccurrence` to an `SharedBlockIR`.
     * return
     * - `SharedBlockIR`: final IR form of a shared block
     * - `BlockShape`: shape of the block. Can be used to determine whether it is equal to another SharedBlockIR
     */
    std::pair<SharedBlockIR, BlockShape> makeSharedBlockIR(
        const BlockOccurrence &occurrence,
        const raw::EdgeGraphRaw &edge_graph_raw,
        const std::unordered_map<GlobalRelId, PredicateId> &signatures);


    /**
     * step 3 (final) step of the collapsed edge graph building process
     */
    collapsed::EdgeGraphCollapsed buildCollapsedGraph(
        const raw::EdgeGraphRaw &raw_edge_graph,
        const std::vector<BlockOccurrence> &block_occurrences,
        const std::unordered_map<ir_builder::raw::EIdx, node_classification::nc> &classification_of_node);

  } // namespace edge_graph_helpers

}
