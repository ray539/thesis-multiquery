#pragma once

#include <bits/stdc++.h>

#include <common/Common.hpp>

namespace query_graph
{
  enum class ValueType
  {
    Node,
    Relationship,
    Integer,
    Bool
  };

  std::string to_string(ValueType type);

  // ---------------------------------------------------------------------------
  // Literals / references
  // ---------------------------------------------------------------------------

  struct IntegerLiteral
  {
    int64_t val;

    IntegerLiteral(int64_t val)
        : val{val}
    {
    }
  };

  std::string to_string(
      const IntegerLiteral &literal);

  struct BoolLiteral
  {
    bool value;

    BoolLiteral(bool value)
        : value{value}
    {
    }
  };

  std::string to_string(
      const BoolLiteral &literal);

  struct VariableReference
  {
    VariableId varId;

    VariableReference(
        VariableId varId)
        : varId{varId}
    {
    }
  };

  std::string to_string(
      const VariableReference &reference);

  // ---------------------------------------------------------------------------
  // Property access
  // ---------------------------------------------------------------------------

  struct PropertyAccess
  {
    ExprId entity;
    std::string property;

    PropertyAccess(
        ExprId entity,
        std::string property)
        : entity{entity},
          property{std::move(property)}
    {
    }
  };

  /**
   * Shallow representation.
   *
   * Example:
   *   PropertyAccess{entity=e2, property="x"}
   *
   * Use expression_to_string() when recursive expression resolution is wanted.
   */
  std::string to_string(
      const PropertyAccess &propertyAccess);

  // ---------------------------------------------------------------------------
  // Arithmetic
  // ---------------------------------------------------------------------------

  enum class ArithmeticOp
  {
    Add,
    Subtract,
    Multiply,
    Divide
  };

  std::string to_string(
      ArithmeticOp op);

  struct Arithmetic
  {
    ArithmeticOp op;
    ExprId lhs;
    ExprId rhs;

    Arithmetic(
        ArithmeticOp op,
        ExprId lhs,
        ExprId rhs)
        : op{op},
          lhs{lhs},
          rhs{rhs}
    {
    }
  };

  std::string to_string(
      const Arithmetic &arithmetic);

  // ---------------------------------------------------------------------------
  // Comparison
  // ---------------------------------------------------------------------------

  enum class ComparisonOp
  {
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual
  };

  std::string to_string(
      ComparisonOp op);

  struct Comparison
  {
    ComparisonOp op;
    ExprId lhs;
    ExprId rhs;

    Comparison(
        ComparisonOp op,
        ExprId lhs,
        ExprId rhs)
        : op{op},
          lhs{lhs},
          rhs{rhs}
    {
    }
  };

  std::string to_string(
      const Comparison &comparison);

  // ---------------------------------------------------------------------------
  // Boolean
  // ---------------------------------------------------------------------------

  enum class BooleanOp
  {
    And,
    Or
  };

  std::string to_string(
      BooleanOp op);

  struct BooleanBinary
  {
    BooleanOp op;
    ExprId lhs;
    ExprId rhs;

    BooleanBinary(
        BooleanOp op,
        ExprId lhs,
        ExprId rhs)
        : op{op},
          lhs{lhs},
          rhs{rhs}
    {
    }
  };

  std::string to_string(
      const BooleanBinary &booleanBinary);

  struct BooleanNot
  {
    ExprId operand;

    BooleanNot(
        ExprId operand)
        : operand{operand}
    {
    }
  };

  std::string to_string(
      const BooleanNot &booleanNot);

  // ---------------------------------------------------------------------------
  // Graph-specific predicates
  // ---------------------------------------------------------------------------

  struct HasLabel
  {
    ExprId node;
    std::string label;

    HasLabel(
        ExprId node,
        std::string label)
        : node{node},
          label{std::move(label)}
    {
    }
  };

  std::string to_string(
      const HasLabel &hasLabel);

  struct HasRelType
  {
    ExprId relationship;
    std::string relType;

    HasRelType(
        ExprId relationship,
        std::string relType)
        : relationship{relationship},
          relType{std::move(relType)}
    {
    }
  };

  std::string to_string(
      const HasRelType &hasRelType);

  // ---------------------------------------------------------------------------
  // Expression
  // ---------------------------------------------------------------------------

  using ExpressionData =
      std::variant<
          IntegerLiteral,
          BoolLiteral,
          VariableReference,
          PropertyAccess,
          Arithmetic,
          Comparison,
          BooleanBinary,
          BooleanNot,
          HasLabel,
          HasRelType>;

  /**
   * Typed expression after static analysis.
   * - ValueType: the type the expression EVALUATES TO 
   */
  struct Expression
  {
    ValueType type;
    ExpressionData data;
  };

  /**
   * Shallow representation.
   *
   * Child expressions are printed as ExprIds rather than recursively expanded.
   */
  std::string to_string(
      const Expression &expression);

  // ---------------------------------------------------------------------------
  // Expression arena
  // ---------------------------------------------------------------------------

  struct ExpressionArena
  {
    std::vector<Expression> expressions;

    ExprId add(
        Expression expr)
    {
      ExprId id{
          static_cast<int64_t>(
              expressions.size())};

      expressions.push_back(
          std::move(expr));

      return id;
    }

    const Expression &operator[](
        ExprId id) const
    {
      return expressions.at(
          static_cast<size_t>(id.val));
    }
  };

  /**
   * Recursively render an expression.
   *
   * Example:
   *
   *   ((v0.x + 3) == v1.weight)
   */
  std::string expression_to_string(
      ExprId id,
      const ExpressionArena &arena);

  /**
   * Debug dump of every expression in the arena.
   */
  std::string to_string(
      const ExpressionArena &arena);

  // ---------------------------------------------------------------------------
  // Query pattern
  // ---------------------------------------------------------------------------

  struct PatternNode
  {
    VariableId variable;

    std::vector<ExprId>
        inlinePredicates;
  };

  /**
   * Shallow representation using ExprIds.
   */
  std::string to_string(
      const PatternNode &node);

  /**
   * Recursively expands inline predicates using arena.
   */
  std::string to_string(
      const PatternNode &node,
      const ExpressionArena &arena);

  struct PatternRelationship
  {
    VariableId variable;

    VariableId startNode;
    VariableId endNode;

    common::Direction direction;

    std::vector<ExprId>
        inlinePredicates;
  };

  /**
   * Shallow representation using ExprIds.
   */
  std::string to_string(
      const PatternRelationship &relationship);

  /**
   * Recursively expands inline predicates using arena.
   */
  std::string to_string(
      const PatternRelationship &relationship,
      const ExpressionArena &arena);

  // ---------------------------------------------------------------------------
  // Shared blocks
  // ---------------------------------------------------------------------------

  /**
   * Localized shared block on one query graph.
   */
  struct SharedBlock
  {
    BlockId block_id;

    std::vector<VariableId>
        patternRelationshipIDs;
  };

  std::string to_string(
      const SharedBlock &sharedBlock);

  // ---------------------------------------------------------------------------
  // Query graph
  // ---------------------------------------------------------------------------

  /**
   * Each QueryGraph owns its ExpressionArena.
   */
  struct QueryGraph
  {
    std::vector<PatternNode>
        patternNodes;

    std::vector<PatternRelationship>
        patternRelationships;

    std::vector<ExprId>
        otherPredicates;

    ExpressionArena expressions;

    std::vector<SharedBlock>
        sharedBlocks;
  };

  /**
   * Full human-readable dump.
   *
   * Predicates are recursively expanded rather than shown only as ExprIds.
   */
  std::string to_string(
      const QueryGraph &queryGraph);



  // query graph builder
  struct QueryGraphBuilderError : std::runtime_error
  {
    using std::runtime_error::runtime_error;
  };
  class QueryGraphBuilder
  {
  public:
    class ExpressionBuilder;
    QueryGraphBuilder() = default;

    /**
     * Create a new pattern node.
     *
     * Returns its query-local VariableId.
     */
    VariableId createNode();

    /**
     * Create a new pattern relationship between two existing node variables.
     */
    VariableId createRelationship(
        VariableId startNode,
        VariableId endNode,
        common::Direction direction = common::Direction::Outgoing);

    /**
     * Create an expression builder backed by this query graph's ExpressionArena.
     *
     * The returned object is lightweight and simply holds a reference to this builder.
     */
    ExpressionBuilder expr();

    /**
     * Attach a boolean predicate to either a PatternNode or PatternRelationship.
     *
     * `variable` must be a variable created by this builder.
     * `predicate` must evaluate to Bool.
     */
    void addPredicateTo(VariableId variable, ExprId predicate);

    /**
     * Add a query-level predicate.
     *
     * `predicate` must evaluate to Bool.
     */
    void addGlobalPredicate(ExprId predicate);

    /**
     * Add one occurrence of a shared block.
     *
     * blockId is supplied explicitly because the same BlockId may be used by occurrences
     * in other query graphs.
     */
    void addSharedBlock(BlockId blockId, std::vector<VariableId> relationships);

    /**
     * Return a copy of the constructed QueryGraph.
     */
    QueryGraph build() const;

    /**
     * Read-only access to the graph while constructing tests.
     */
    const QueryGraph &graph() const;

    /**
     * Return the type associated with a variable created by this builder.
     */
    ValueType variableType(VariableId variable) const;

  private:
    friend class ExpressionBuilder;

    QueryGraph graph_;

    int64_t nextVariableId_ = 0;

    // VariableId -> Node / Relationship.
    std::unordered_map<VariableId, ValueType, IntStrongHash> variableTypes_;

    // VariableId -> index in QueryGraph::patternNodes.
    std::unordered_map<VariableId, size_t, IntStrongHash> nodeIndex_;

    // VariableId -> index in QueryGraph::patternRelationships.
    std::unordered_map<VariableId, size_t, IntStrongHash> relationshipIndex_;

    const Expression &expression(ExprId id) const;
    void requireBooleanExpression(ExprId id, std::string_view operation) const;
  };

  /**
   * Thin facade over QueryGraph::ExpressionArena.
   *
   * Every method:
   * - validates its input expression types;
   * - inserts a correctly typed Expression into the parent's arena;
   * - returns the new ExprId.
   */
  class QueryGraphBuilder::ExpressionBuilder
  {
  public:
    explicit ExpressionBuilder(QueryGraphBuilder &parent);

    ExprId variableReference(VariableId variable);

    ExprId integerLiteral(int64_t value);
    ExprId boolLiteral(bool value);

    /**
     * Property accesses currently evaluate to Integer because the current data model
     * only supports integer-valued properties.
     */
    ExprId property(ExprId entity, std::string property);
    ExprId propertyAccess(ExprId entity, std::string property);

    /**
     * General arithmetic builder.
     *
     * lhs and rhs must both evaluate to Integer.
     */
    ExprId arithmetic(ExprId lhs, ArithmeticOp op, ExprId rhs);

    ExprId add(ExprId lhs, ExprId rhs);
    ExprId subtract(ExprId lhs, ExprId rhs);
    ExprId multiply(ExprId lhs, ExprId rhs);
    ExprId divide(ExprId lhs, ExprId rhs);

    /**
     * Equal / NotEqual require operands of the same type.
     *
     * Less / LessEqual / Greater / GreaterEqual additionally require Integer operands.
     */
    ExprId comparison(ExprId lhs, ComparisonOp op, ExprId rhs);

    /**
     * lhs and rhs must both evaluate to Bool.
     */
    ExprId booleanBinary(ExprId lhs, BooleanOp op, ExprId rhs);

    /**
     * operand must evaluate to Bool.
     */
    ExprId booleanNot(ExprId operand);

    /**
     * node must evaluate to Node.
     */
    ExprId hasLabel(ExprId node, std::string label);

    /**
     * relationship must evaluate to Relationship.
     */
    ExprId hasRelType(ExprId relationship, std::string relType);

  private:
    QueryGraphBuilder &parent_;

    ExprId addExpression(ValueType type, ExpressionData data);

    const Expression &expression(ExprId id) const;

    void requireType(ExprId id, ValueType expected, std::string_view operation) const;

    [[noreturn]] void throwTypeError(
        std::string_view operation,
        ExprId id,
        ValueType expected,
        ValueType actual) const;
  };

} // namespace query_graph