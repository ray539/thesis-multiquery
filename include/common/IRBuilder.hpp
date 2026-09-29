#pragma once
#include <common/QueryGraph.hpp>
#include <common/Common.hpp>

namespace ir_builder
{

  struct CanonicalizationError : std::runtime_error
  {
    using std::runtime_error::runtime_error;
  };

  struct GlobalNodeId
  {
    int64_t val;
    explicit GlobalNodeId(int64_t val) : val{val} {}
    bool operator==(const GlobalNodeId &other) const
    {
      return val == other.val;
    }
  };

  struct GlobalRelId
  {
    int64_t val;
    explicit GlobalRelId(int64_t val) : val{val} {}
    bool operator==(const GlobalRelId &other) const
    {
      return val == other.val;
    }
  };

  struct PredicateId
  {
    int64_t val;
    PredicateId(int64_t val) : val{val} {}
  };

  // (r) -> r.prop == val
  struct RelPropertyEquals
  {
    std::string prop;
    int64_t val;
  };

  // (r) -> relType(r) == 'R'
  struct RelTypeEquals
  {
    std::string relType;
  };

  // (r) -> start|end(r).prop == val
  struct EndpointPropertyEquals
  {
    common::Endpoint endpoint;
    std::string prop;
    int64_t val;
  };

  struct EndpointHasLabel
  {
    common::Endpoint endpoint;
    std::string label;
  };

  using AtomicRelPredicate = std::variant<
      RelPropertyEquals,
      RelTypeEquals,
      EndpointPropertyEquals,
      EndpointHasLabel>;

  struct AdjacencyRequirement
  {
    common::Adjacency adjacency;
    PredicateId predId;
    int64_t multiplicity;
  };

  // (r) -> p1(r) && p2(r) && p3(r) ...
  struct BaseRelPredicate
  {
    std::vector<AtomicRelPredicate> atomicPredicates;
  };

  struct OtherRelPredicate
  {
    PredicateId previousId;
    std::vector<AdjacencyRequirement> adjacencyRequirements;
  };

  using RelPredicate =
      std::variant<BaseRelPredicate, OtherRelPredicate>;

  // -----------
  // FINAL QUERY GRAPH IR
  // ------------
  struct PatternRelationshipIR
  {
    query_graph::VariableId lv;
    GlobalRelId lv_global;

    query_graph::VariableId startNode;
    query_graph::VariableId endNode;
    PredicateId signatureId;
  };

  struct QueryGraphIR
  {
    std::vector<PatternRelationshipIR> patternRelationships;
  };

  // relId, endpoint
  struct Incidence
  {
    GlobalRelId relId;
    common::Endpoint endpoint;
  };

  class PredicateRegistry
  {
  public:
    PredicateId intern(RelPredicate predicate);

    /**
     * How many predicates are registered
     */
    size_t size() const;

    // get predicate object corresponding to given predicateId
    const RelPredicate &operator[](
        PredicateId id) const;

    const std::vector<RelPredicate> &entries() const;

  private:
    std::vector<RelPredicate> predicates;
    // canonical predicate -> PredicateId
    std::unordered_map<std::string, PredicateId> byKey;
  };

  /**
   * Final output of IRBuilder.run
   */
  struct BuildResult
  {
    /**
     * internal query graphs
     */
    std::vector<QueryGraphIR> queryGraphs;

    /**
     * in a nutshell,  PredicateId -> RelPredicate
     */
    PredicateRegistry predicateRegistry;

    /**
     * signatures[k][globalRelationshipId] = predicate id
     * - not completely necessary to the executable layer, but useful for debugging
     */
    std::vector<
        std::unordered_map<GlobalRelId, PredicateId>>
        signatures;
  };

  class IRBuilder
  {
  public:
    IRBuilder() {}
    BuildResult run(const std::vector<query_graph::QueryGraph> &queryGraphs, int maxLayer = 3);
  };

  namespace expression_recognition
  {
    namespace qg = query_graph;

    std::optional<qg::VariableId> variableReference(const qg::QueryGraph &qg, qg::ExprId id);
    std::optional<int64_t> intConstant(const qg::QueryGraph &qg, qg::ExprId id);

    struct ParsedPropertyAccess
    {
      qg::VariableId varId;
      std::string property;
    };
    std::optional<ParsedPropertyAccess> propertyAccess(const qg::QueryGraph &qg, qg::ExprId id);

    struct ParsedHasLabel
    {
      qg::VariableId nodeId;
      std::string label;
    };
    std::optional<ParsedHasLabel> hasLabel(const qg::QueryGraph &qg, qg::ExprId id);

    struct ParsedHasRelType
    {
      qg::VariableId relId;
      std::string relType;
    };

    std::optional<ParsedHasRelType> hasRelType(const qg::QueryGraph &qg, qg::ExprId id);

  }

}

template <>
struct std::hash<ir_builder::GlobalNodeId>
{
  std::size_t operator()(const ir_builder::GlobalNodeId &id) const noexcept
  {
    return std::hash<int64_t>{}(id.val);
  }
};

template <>
struct std::hash<ir_builder::GlobalRelId>
{
  std::size_t operator()(const ir_builder::GlobalRelId &id) const noexcept
  {
    return std::hash<int64_t>{}(id.val);
  }
};