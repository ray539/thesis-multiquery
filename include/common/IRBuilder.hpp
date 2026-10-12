#pragma once
#include <common/QueryGraph.hpp>
#include <common/Common.hpp>

namespace ir_builder
{

  struct CanonicalizationError : std::runtime_error
  {
    using std::runtime_error::runtime_error;
  };

  class GlobalNodeId
  {
  public:
    int64_t val;
    GlobalNodeId() = default;
    explicit GlobalNodeId(int64_t val) : val{val} {}
    bool operator==(const GlobalNodeId &other) const
    {
      return val == other.val;
    }
  };

  class GlobalRelId
  {
  public:
    int64_t val;
    GlobalRelId() = default;
    explicit GlobalRelId(int64_t val) : val{val} {}
    bool operator==(const GlobalRelId &other) const
    {
      return val == other.val;
    }
  };

  class PredicateId
  {
  public:
    int64_t val;
    PredicateId() = default;
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

  struct EndpointsEqual
  {
  };

  using AtomicRelPredicate = std::variant<
      RelPropertyEquals,
      RelTypeEquals,
      EndpointPropertyEquals,
      EndpointHasLabel,
      EndpointsEqual>;

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
    GlobalRelId global_rel_id; // for reference, but likely not useful
    query_graph::VariableId lv;
    query_graph::VariableId start;
    query_graph::VariableId end;
    PredicateId signatureId;
  };

  /**
   * edge graph template. There are 3 edge graphs our code deals with
   * - SharedBlockIR::EdgeGraph, NodeType: PredicateID
   * - RawEdgeGraph, NodeType: PatternRelationshipIR
   * - EdgeGraph, NodeType: std::variant<...>
   */
  template <typename NodeType, typename EIdx>
  class EdgeGraph
  {
  public:
    struct EGraphAdjacency
    {
      common::Adjacency adjType;
      EIdx other;
    };

  private:
    std::vector<NodeType> nodes_;
    std::vector<std::vector<EGraphAdjacency>> adj_;

  public:
    EdgeGraph(std::vector<NodeType> nodes, std::vector<std::vector<EGraphAdjacency>> adj)
        : nodes_(nodes),
          adj_(adj)
    {
    }

    size_t num_nodes() const
    {
      return nodes_.size();
    }

    std::vector<NodeType> &all_nodes() const
    {
      return nodes_;
    }

    NodeType node_at(EIdx eidx) const
    {
      return nodes_[eidx.val];
    }

    std::vector<EGraphAdjacency> adj(EIdx eidx) const
    {
      return adj_[eidx.val];
    }
  };

  /**
   * shared block representation. Has it's own edge graph, which is indifferent to
   * where it is.
   *
   * Needs a 'back_context' to localize back into an edge graph
   */
  struct SharedBlockIR
  {

    struct EIdx
    {
      size_t val;
      EIdx() = default;
      EIdx(size_t val) : val(val)
      {
      }
      bool operator==(const EIdx &) const = default;
    };
    query_graph::BlockId block_id;

    typedef EdgeGraph<PredicateId, EIdx> EdgeGraphSB;

    EdgeGraph<PredicateId, EIdx> edge_graph;
    EIdx boundary_node; // backtracking MUST start here
  };

  namespace raw
  {
    // raw query graph, before collapsing shared blocks
    struct EIdx
    {
      size_t val;
      EIdx(size_t val) : val(val)
      {
      }
      bool operator==(const EIdx &) const = default;
    };
    typedef EdgeGraph<PatternRelationshipIR, EIdx> EdgeGraphRaw;
  };

  namespace collapsed
  {
    // collapsed query graph
    struct EIdx
    {
      size_t val;
      EIdx() = default;
      EIdx(size_t val) : val(val)
      {
      }
      bool operator==(const EIdx &) const = default;
    };
    

  }

  /**
   * localized shared block for a specific EdgeGraph
   */
  struct SharedBlockReference
  {
    collapsed::EIdx index;
    collapsed::EIdx boundary_node;
    query_graph::BlockId block_id;
    std::unordered_map<SharedBlockIR::EIdx, PatternRelationshipIR> back_mapping;
  };

  namespace collapsed
  {
    typedef EdgeGraph<std::variant<PatternRelationshipIR, SharedBlockReference>, EIdx> EdgeGraphCollapsed;
    typedef std::variant<PatternRelationshipIR, SharedBlockReference> NodeType;
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
     * edges graphs ready for matching
     */
    std::vector<collapsed::EdgeGraphCollapsed> edgeGraphs;

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

    /**
     * a shared block is an context indifferent subgraph which can occur in
     * multiple times in one query graph / multiple query graphs
     */
    std::unordered_map<query_graph::BlockId, SharedBlockIR> shared_block_registry;
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

    /**
     * match `VariableReference(v:VariableId)`, returning `v`
     */
    std::optional<qg::VariableId> variableReference(const qg::QueryGraph &qg, qg::ExprId id);

    /**
     * match `IntegerLiteral(val:int64_t)`, returning `val`
     */
    std::optional<int64_t> intConstant(const qg::QueryGraph &qg, qg::ExprId id);

    struct ParsedPropertyAccess
    {
      qg::VariableId varId;
      std::string property;
    };

    /**
     * match `PropertyAccess(varId:VariableId, property:string)`, returning `varId, property`
     */
    std::optional<ParsedPropertyAccess> propertyAccess(const qg::QueryGraph &qg, qg::ExprId id);

    struct ParsedHasLabel
    {
      qg::VariableId nodeId;
      std::string label;
    };

    /**
     * match `HasLabel(VariableReference(nodeId:VariableId), label:string)`, returning `nodeId, label`
     */
    std::optional<ParsedHasLabel> hasLabel(const qg::QueryGraph &qg, qg::ExprId id);

    struct ParsedHasRelType
    {
      qg::VariableId relId;
      std::string relType;
    };

    /**
     * match `HasRelType(VariableReference(relId:VariableId), relType:string)`, returning `relId, relType`
     */
    std::optional<ParsedHasRelType> hasRelType(const qg::QueryGraph &qg, qg::ExprId id);
  }
} // namespace ir_builder

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

template <>
struct std::hash<ir_builder::SharedBlockIR::EIdx>
{
  std::size_t operator()(const ir_builder::SharedBlockIR::EIdx &id) const noexcept
  {
    return std::hash<int64_t>{}(id.val);
  }
};

template <>
struct std::hash<ir_builder::raw::EIdx>
{
  std::size_t operator()(const ir_builder::raw::EIdx &id) const noexcept
  {
    return std::hash<int64_t>{}(id.val);
  }
};

template <>
struct std::hash<ir_builder::collapsed::EIdx>
{
  std::size_t operator()(const ir_builder::collapsed::EIdx &id) const noexcept
  {
    return std::hash<int64_t>{}(id.val);
  }
};