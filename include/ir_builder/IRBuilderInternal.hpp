#pragma once
#include <common/IRBuilder.hpp>
#include <bits/stdc++.h>
/**
 * IRBuilder classes only used in the builder itself
 */

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
    std::unordered_map<query_graph::VariableId, GlobalNodeId, IntStrongHash> node_to_globalID;
    // local relationship VariableId -> index of relationship in
    std::unordered_map<query_graph::VariableId, GlobalRelId, IntStrongHash> rel_to_globalID;
  };

  // relId, endpoint
  struct Incidence
  {
    GlobalRelId relId;
    common::Endpoint endpoint;
  };

  struct GlobalEntities
  {
    // queryStates[qi] gives the local to global mapping information for query qi
    std::vector<QueryLocalToGlobal> queryLocalToGlobals;

    // list of internal pattern relationships
    std::unordered_map<GlobalRelId, PatternRelationshipInternal, IntStrongHash> relationships;

    // global node id -> relationships incident to it
    std::unordered_map<GlobalNodeId, std::vector<Incidence>, IntStrongHash> adjListNode;
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
    std::vector<std::unordered_map<GlobalRelId, PredicateId, IntStrongHash>> signatures;
    // map of predId -> RelPredicate
    PredicateRegistry pRegistry;
  };

  inline SignatureBuilderResult buildAndInternSignatures(
      const std::vector<query_graph::QueryGraph> &queryGraphs,
      GlobalEntities &ge,
      int maxLayers);

  // raw query graph, before collapsing shared blocks
  struct EIdxRaw_tag;
  using EIdxRaw = IntStrong<EIdxRaw_tag, size_t>;
  typedef EdgeGraph<PatternRelationshipIR, EIdxRaw> EdgeGraphRaw;

  namespace edge_graph_helpers
  {

    /**
     * Think of this class as holding a bunch of pointers to nodes in the outer raw edge graph.
     * These will then be used for further analysis to be turned into compressed shared blocks.
     */
    struct BlockOccurrence
    {
      struct Boundary
      {
        // local index. Can be used to index outer_node_indices
        size_t boundary_local_index;
        // global index
        EIdxRaw boundary_outer_index;
      };

      query_graph::BlockId block_id;
      // outer edge graph indices
      std::vector<EIdxRaw> outer_node_indices;
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
    EdgeGraphRaw buildRawEdgeGraph(
        const query_graph::QueryGraph &queryGraph,
        const QueryLocalToGlobal &local_to_global,
        const std::unordered_map<GlobalRelId, PatternRelationshipInternal, IntStrongHash> &global_relationships);

    /**
     * This will
     * - turn user given shared block annotations into `BlockOccurrence` objects
     * - register the new blocks in `shared_block_of` and `blockshape_of`
     *
     * return
     * - block_occurrences: `vector<BlockOccurrence>`: list of block occurrence objects
     * - classification_of_node: classifies each node on whether it belongs to the inner, outer or boundary of a shared block
     */
    std::pair<std::vector<BlockOccurrence>, std::unordered_map<EIdxRaw, node_classification::nc, IntStrongHash>> analyseBlocks(
        const qg::QueryGraph &queryGraph,
        const EdgeGraphRaw &raw_edge_graph,
        std::unordered_map<qg::BlockId, SharedBlockIR, IntStrongHash> &shared_block_of,
        std::unordered_map<qg::BlockId, BlockShape, IntStrongHash> &blockshape_of,
        const std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> &signatures);

    /**
     * turn an `BlockOccurrence` to an `SharedBlockIR`.
     * return
     * - `SharedBlockIR`: final IR form of a shared block
     * - `BlockShape`: shape of the block. Can be used to determine whether it is equal to another SharedBlockIR
     */
    std::pair<SharedBlockIR, BlockShape> makeSharedBlockIR(
        const BlockOccurrence &occurrence,
        const EdgeGraphRaw &edge_graph_raw,
        const std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> &signatures);

    /**
     * step 3 (final) step of the collapsed edge graph building process
     */
    EdgeGraphCollapsed buildCollapsedGraph(
        const EdgeGraphRaw &raw_edge_graph,
        const std::vector<BlockOccurrence> &block_occurrences,
        const std::unordered_map<EIdxRaw, node_classification::nc, IntStrongHash> &classification_of_node);

  } // namespace edge_graph_helpers

  /**
   * Builds:
   *
   * 1. one collapsed EdgeGraph per query;
   * 2. the global context-independent SharedBlockIR registry.
   */
  std::pair<
      std::vector<EdgeGraphCollapsed>,
      std::unordered_map<qg::BlockId, SharedBlockIR, IntStrongHash>>
  buildCollapsedEdgeGraphs(
      const std::vector<qg::QueryGraph> &queryGraphs,
      const GlobalEntities &ge,
      const std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> &signatures);


  // ---------------------------------------------------------------------------
  // Internal builder debug printing
  // ---------------------------------------------------------------------------
  std::string to_string(const PatternRelationshipInternal &relationship);
  std::string to_string(const QueryLocalToGlobal &mapping);
  std::string to_string(const Incidence &incidence);
  std::string to_string(const GlobalEntities &entities);
  std::string to_string(const SignatureBuilderResult &result);

  std::string to_string(EIdxRaw eidx);
  std::string to_string(const EdgeGraphRaw &edgeGraph);

  namespace edge_graph_helpers
  {
    std::string to_string(const BlockOccurrence::Boundary &boundary);
    std::string to_string(const BlockOccurrence &occurrence);
    std::string to_string(const BlockShape &shape);

    namespace node_classification
    {
      std::string to_string(const Boundary &classification);
      std::string to_string(const Inner &classification);
      std::string to_string(const Outer &classification);
      std::string to_string(const nc &classification);
    }
  }

}
