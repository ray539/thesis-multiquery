#include <common/QueryGraph.hpp>

namespace query_graph
{
  // ===========================================================================
  // QueryGraphBuilder
  // ===========================================================================

  VariableId QueryGraphBuilder::createNode()
  {
    VariableId variable{nextVariableId_++};

    graph_.patternNodes.push_back(
        PatternNode{
            .variable = variable,
            .inlinePredicates = {}});

    nodeIndex_[variable] = graph_.patternNodes.size() - 1;
    variableTypes_[variable] = ValueType::Node;

    return variable;
  }

  VariableId QueryGraphBuilder::createRelationship(
      VariableId startNode,
      VariableId endNode,
      common::Direction direction)
  {
    if (variableType(startNode) != ValueType::Node)
    {
      throw QueryGraphBuilderError(
          "createRelationship: start variable " + query_graph::to_string(startNode) + " is not a Node");
    }

    if (variableType(endNode) != ValueType::Node)
    {
      throw QueryGraphBuilderError(
          "createRelationship: end variable " + query_graph::to_string(endNode) + " is not a Node");
    }

    VariableId variable{nextVariableId_++};

    graph_.patternRelationships.push_back(
        PatternRelationship{
            .variable = variable,
            .startNode = startNode,
            .endNode = endNode,
            .direction = direction,
            .inlinePredicates = {}});

    relationshipIndex_[variable] = graph_.patternRelationships.size() - 1;
    variableTypes_[variable] = ValueType::Relationship;

    return variable;
  }

  QueryGraphBuilder::ExpressionBuilder QueryGraphBuilder::expr()
  {
    return ExpressionBuilder{*this};
  }

  void QueryGraphBuilder::addPredicateTo(VariableId variable, ExprId predicate)
  {
    requireBooleanExpression(predicate, "addPredicateTo");
    auto typeIt = variableTypes_.find(variable);
    if (typeIt == variableTypes_.end())
    {
      throw QueryGraphBuilderError(
          "addPredicateTo: unknown variable " + query_graph::to_string(variable));
    }

    switch (typeIt->second)
    {
    case ValueType::Node:
      graph_.patternNodes.at(nodeIndex_.at(variable)).inlinePredicates.push_back(predicate);
      return;

    case ValueType::Relationship:
      graph_.patternRelationships.at(relationshipIndex_.at(variable)).inlinePredicates.push_back(predicate);
      return;

    case ValueType::Integer:
    case ValueType::Bool:
      break;
    }

    throw QueryGraphBuilderError(
        "addPredicateTo: variable " + query_graph::to_string(variable) + " is not a graph entity");
  }

  void QueryGraphBuilder::addGlobalPredicate(ExprId predicate)
  {
    requireBooleanExpression(predicate, "addGlobalPredicate");
    graph_.otherPredicates.push_back(predicate);
  }

  void QueryGraphBuilder::addSharedBlock(BlockId blockId, std::vector<VariableId> relationships)
  {
    if (relationships.empty())
    {
      throw QueryGraphBuilderError("addSharedBlock: shared block cannot be empty");
    }
    std::unordered_set<VariableId, IntStrongHash> seen;
    for (VariableId variable : relationships)
    {
      if (variableType(variable) != ValueType::Relationship)
      {
        throw QueryGraphBuilderError(
            "addSharedBlock: variable " + query_graph::to_string(variable) + " is not a Relationship");
      }
      if (!seen.insert(variable).second)
      {
        throw QueryGraphBuilderError(
            "addSharedBlock: relationship " + query_graph::to_string(variable) + " occurs twice in the block");
      }
    }
    graph_.sharedBlocks.push_back(
        SharedBlock{
            .block_id = blockId,
            .patternRelationshipIDs = std::move(relationships)});
  }

  QueryGraph QueryGraphBuilder::build() const
  {
    return graph_;
  }

  const QueryGraph &QueryGraphBuilder::graph() const
  {
    return graph_;
  }

  ValueType QueryGraphBuilder::variableType(VariableId variable) const
  {
    auto it = variableTypes_.find(variable);
    if (it == variableTypes_.end())
    {
      throw QueryGraphBuilderError(
          "unknown variable " + query_graph::to_string(variable));
    }
    return it->second;
  }

  const Expression &QueryGraphBuilder::expression(ExprId id) const
  {
    if (id.val < 0 || static_cast<size_t>(id.val) >= graph_.expressions.expressions.size())
    {
      throw QueryGraphBuilderError(
          "invalid expression id " + query_graph::to_string(id));
    }
    return graph_.expressions[id];
  }

  void QueryGraphBuilder::requireBooleanExpression(ExprId id, std::string_view operation) const
  {
    const Expression &expr = expression(id);
    if (expr.type != ValueType::Bool)
    {
      throw QueryGraphBuilderError(
          std::string(operation) + ": expression " + query_graph::to_string(id) +
          " must evaluate to Bool, got " + query_graph::to_string(expr.type));
    }
  }

  // ===========================================================================
  // ExpressionBuilder
  // ===========================================================================

  QueryGraphBuilder::ExpressionBuilder::ExpressionBuilder(QueryGraphBuilder &parent)
      : parent_{parent}
  {
  }

  ExprId QueryGraphBuilder::ExpressionBuilder::addExpression(ValueType type, ExpressionData data)
  {
    return parent_.graph_.expressions.add(
        Expression{
            .type = type,
            .data = std::move(data)});
  }

  const Expression &QueryGraphBuilder::ExpressionBuilder::expression(ExprId id) const
  {
    return parent_.expression(id);
  }

  void QueryGraphBuilder::ExpressionBuilder::requireType(
      ExprId id,
      ValueType expected,
      std::string_view operation) const
  {
    const Expression &expr = expression(id);

    if (expr.type != expected)
      throwTypeError(operation, id, expected, expr.type);
  }

  [[noreturn]] void QueryGraphBuilder::ExpressionBuilder::throwTypeError(
      std::string_view operation,
      ExprId id,
      ValueType expected,
      ValueType actual) const
  {
    throw QueryGraphBuilderError(
        std::string(operation) + ": expression " + query_graph::to_string(id) +
        " must have type " + query_graph::to_string(expected) +
        ", got " + query_graph::to_string(actual));
  }

  // ---------------------------------------------------------------------------
  // Literals / references
  // ---------------------------------------------------------------------------

  ExprId QueryGraphBuilder::ExpressionBuilder::variableReference(VariableId variable)
  {
    ValueType type = parent_.variableType(variable);

    return addExpression(
        type,
        VariableReference{variable});
  }

  ExprId QueryGraphBuilder::ExpressionBuilder::integerLiteral(int64_t value)
  {
    return addExpression(
        ValueType::Integer,
        IntegerLiteral{value});
  }

  ExprId QueryGraphBuilder::ExpressionBuilder::boolLiteral(bool value)
  {
    return addExpression(
        ValueType::Bool,
        BoolLiteral{value});
  }

  // ---------------------------------------------------------------------------
  // Properties
  // ---------------------------------------------------------------------------

  ExprId QueryGraphBuilder::ExpressionBuilder::property(ExprId entity, std::string propertyName)
  {
    const Expression &entityExpression = expression(entity);

    if (entityExpression.type != ValueType::Node && entityExpression.type != ValueType::Relationship)
    {
      throw QueryGraphBuilderError(
          "property: expression " + query_graph::to_string(entity) +
          " must evaluate to Node or Relationship, got " + query_graph::to_string(entityExpression.type));
    }

    return addExpression(
        ValueType::Integer,
        PropertyAccess{
            entity,
            std::move(propertyName)});
  }

  ExprId QueryGraphBuilder::ExpressionBuilder::propertyAccess(ExprId entity, std::string propertyName)
  {
    return property(entity, std::move(propertyName));
  }

  // ---------------------------------------------------------------------------
  // Arithmetic
  // ---------------------------------------------------------------------------

  ExprId QueryGraphBuilder::ExpressionBuilder::arithmetic(ExprId lhs, ArithmeticOp op, ExprId rhs)
  {
    requireType(lhs, ValueType::Integer, "arithmetic");
    requireType(rhs, ValueType::Integer, "arithmetic");

    return addExpression(
        ValueType::Integer,
        Arithmetic{
            op,
            lhs,
            rhs});
  }

  ExprId QueryGraphBuilder::ExpressionBuilder::add(ExprId lhs, ExprId rhs)
  {
    return arithmetic(lhs, ArithmeticOp::Add, rhs);
  }

  ExprId QueryGraphBuilder::ExpressionBuilder::subtract(ExprId lhs, ExprId rhs)
  {
    return arithmetic(lhs, ArithmeticOp::Subtract, rhs);
  }

  ExprId QueryGraphBuilder::ExpressionBuilder::multiply(ExprId lhs, ExprId rhs)
  {
    return arithmetic(lhs, ArithmeticOp::Multiply, rhs);
  }

  ExprId QueryGraphBuilder::ExpressionBuilder::divide(ExprId lhs, ExprId rhs)
  {
    return arithmetic(lhs, ArithmeticOp::Divide, rhs);
  }

  // ---------------------------------------------------------------------------
  // Comparison
  // ---------------------------------------------------------------------------

  ExprId QueryGraphBuilder::ExpressionBuilder::comparison(ExprId lhs, ComparisonOp op, ExprId rhs)
  {
    const Expression &left = expression(lhs);
    const Expression &right = expression(rhs);

    if (left.type != right.type)
    {
      throw QueryGraphBuilderError(
          "comparison: operands " + query_graph::to_string(lhs) + " and " + query_graph::to_string(rhs) +
          " must have the same type, got " + query_graph::to_string(left.type) +
          " and " + query_graph::to_string(right.type));
    }

    switch (op)
    {
    case ComparisonOp::Equal:
    case ComparisonOp::NotEqual:
      break;

    case ComparisonOp::Less:
    case ComparisonOp::LessEqual:
    case ComparisonOp::Greater:
    case ComparisonOp::GreaterEqual:
      if (left.type != ValueType::Integer)
      {
        throw QueryGraphBuilderError(
            "comparison: ordered comparisons require Integer operands, got " +
            query_graph::to_string(left.type));
      }
      break;
    }

    return addExpression(
        ValueType::Bool,
        Comparison{
            op,
            lhs,
            rhs});
  }

  // ---------------------------------------------------------------------------
  // Boolean
  // ---------------------------------------------------------------------------

  ExprId QueryGraphBuilder::ExpressionBuilder::booleanBinary(ExprId lhs, BooleanOp op, ExprId rhs)
  {
    requireType(lhs, ValueType::Bool, "booleanBinary");
    requireType(rhs, ValueType::Bool, "booleanBinary");

    return addExpression(
        ValueType::Bool,
        BooleanBinary{
            op,
            lhs,
            rhs});
  }

  ExprId QueryGraphBuilder::ExpressionBuilder::booleanNot(ExprId operand)
  {
    requireType(operand, ValueType::Bool, "booleanNot");

    return addExpression(
        ValueType::Bool,
        BooleanNot{operand});
  }

  // ---------------------------------------------------------------------------
  // Graph-specific predicates
  // ---------------------------------------------------------------------------

  ExprId QueryGraphBuilder::ExpressionBuilder::hasLabel(ExprId node, std::string label)
  {
    requireType(node, ValueType::Node, "hasLabel");

    return addExpression(
        ValueType::Bool,
        HasLabel{
            node,
            std::move(label)});
  }

  ExprId QueryGraphBuilder::ExpressionBuilder::hasRelType(ExprId relationship, std::string relType)
  {
    requireType(relationship, ValueType::Relationship, "hasRelType");

    return addExpression(
        ValueType::Bool,
        HasRelType{
            relationship,
            std::move(relType)});
  }
} // namespace query_graph
