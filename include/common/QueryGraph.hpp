#pragma once
#include <bits/stdc++.h>

namespace query_graph
{
  struct ExprId
  {
    int64_t val;
    ExprId(int64_t val) : val{val} {}
  };
  class VariableId
  {
  public:
    int64_t val;
    VariableId() = default;
    VariableId(int64_t val) : val{val} {}
    bool operator==(const VariableId &other) const
    {
      return val == other.val;
    }
  };

  class BlockId
  {
  public:
    int64_t val;
    BlockId() = default;
    BlockId(int64_t val) : val{val} {}
  };


  enum class ValueType
  {
    Node,
    Relationship,
    Integer,
    Bool
  };

  struct IntegerLiteral
  {
    int64_t val;
    IntegerLiteral(int64_t val) : val{val} {}
  };

  struct BoolLiteral
  {
    bool value;
    BoolLiteral(bool value) : value{value} {}
  };

  struct VariableReference
  {
    VariableId varId;
    VariableReference(VariableId varId) : varId{varId} {}
  };

  struct PropertyAccess
  {
    ExprId entity;
    std::string property;
    PropertyAccess(ExprId entity, std::string property)
        : entity{entity}, property{property} {}
  };

  enum class ArithmeticOp
  {
    Add,
    Subtract,
    Multiply,
    Divide
  };

  struct Arithmetic
  {
    ArithmeticOp op;
    ExprId lhs;
    ExprId rhs;
    Arithmetic(ArithmeticOp op, ExprId lhs, ExprId rhs)
        : op{op}, lhs{lhs}, rhs{rhs} {}
  };

  enum class ComparisonOp
  {
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual
  };

  struct Comparison
  {
    ComparisonOp op;
    ExprId lhs;
    ExprId rhs;
    Comparison(ComparisonOp op, ExprId lhs, ExprId rhs)
        : op{op}, lhs{lhs}, rhs{rhs} {}
  };

  enum class BooleanOp
  {
    And,
    Or
  };

  struct BooleanBinary
  {
    BooleanOp op;
    ExprId lhs;
    ExprId rhs;
    BooleanBinary(BooleanOp op, ExprId lhs, ExprId rhs)
        : op{op}, lhs{lhs}, rhs{rhs} {}
  };

  struct BooleanNot
  {
    ExprId operand;
    BooleanNot(ExprId operand) : operand{operand} {}
  };

  struct HasLabel
  {
    ExprId node;
    std::string label;
    HasLabel(ExprId node, std::string label) : node{node}, label{label} {}
  };

  struct HasRelType
  {
    ExprId relationship;
    std::string relType;
    HasRelType(ExprId relationship, std::string relType)
        : relationship{relationship}, relType{relType} {}
  };

  using ExpressionData = std::variant<
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

  // typed expression after static analysis
  struct Expression
  {
    ValueType type;
    ExpressionData data;
  };

  // memory pool for expressions
  struct ExpressionArena
  {
    std::vector<Expression> expressions;
    ExprId add(Expression expr)
    {
      ExprId id = expressions.size();
      expressions.push_back(std::move(expr));
      return id;
    }
    const Expression &operator[](ExprId id) const
    {
      return expressions[id.val];
    }
  };

  struct PatternNode
  {
    VariableId variable;
    std::vector<ExprId> inlinePredicates;
  };

  struct PatternRelationship
  {
    VariableId variable;

    VariableId startNode;
    VariableId endNode;
    common::Direction direction;

    std::vector<ExprId> inlinePredicates;
  };


  /**
   * localized shared block on one query graph
   */
  struct SharedBlock {
    BlockId block_id;
    std::vector<VariableId> patternRelationshipIDs;
  };


  // each query graph has it's own ExpressionArena
  class QueryGraph
  {
  public:
    std::vector<PatternNode> patternNodes;
    std::vector<PatternRelationship> patternRelationships;
    std::vector<ExprId> otherPredicates;
    ExpressionArena expressions;

    std::vector<SharedBlock> sharedBlocks;
  };
}

template <>
struct std::hash<query_graph::VariableId>
{
  std::size_t operator()(const query_graph::VariableId &id) const noexcept
  {
    return std::hash<int64_t>{}(id.val);
  }
};

template <>
struct std::hash<query_graph::BlockId>
{
  std::size_t operator()(const query_graph::BlockId &id) const noexcept
  {
    return std::hash<int64_t>{}(id.val);
  }
};
