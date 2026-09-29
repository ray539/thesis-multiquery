#pragma once
#include <common/IRBuilder.hpp>
#include <bits/stdc++.h>


namespace ir_builder {


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

  struct GlobalEntities {
    // queryStates[qi] gives the local to global mapping information for query qi
    std::vector<QueryLocalToGlobal> queryLocalToGlobals;

    // list of internal pattern relationships
    std::unordered_map<GlobalRelId, PatternRelationshipInternal> relationships;

    // global node id -> relationships incident to it
    std::unordered_map<GlobalNodeId, std::vector<Incidence>> adjListNode;
  };

  inline GlobalEntities buildGlobalEntities(const std::vector<query_graph::QueryGraph> &queryGraphs);

  inline void collectPredicates(
    const std::vector<query_graph::QueryGraph> queryGraphs,
    GlobalEntities& ge
  );


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


}
