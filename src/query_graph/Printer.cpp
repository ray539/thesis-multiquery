#include <common/QueryGraph.hpp>

namespace query_graph
{
  // ===========================================================================
  // Enums
  // ===========================================================================

  std::string to_string(
      ValueType type)
  {
    switch (type)
    {
    case ValueType::Node:
      return "Node";

    case ValueType::Relationship:
      return "Relationship";

    case ValueType::Integer:
      return "Integer";

    case ValueType::Bool:
      return "Bool";
    }

    std::unreachable();
  }

  std::string to_string(
      ArithmeticOp op)
  {
    switch (op)
    {
    case ArithmeticOp::Add:
      return "+";

    case ArithmeticOp::Subtract:
      return "-";

    case ArithmeticOp::Multiply:
      return "*";

    case ArithmeticOp::Divide:
      return "//";
    }

    std::unreachable();
  }

  std::string to_string(
      ComparisonOp op)
  {
    switch (op)
    {
    case ComparisonOp::Equal:
      return "==";

    case ComparisonOp::NotEqual:
      return "!=";

    case ComparisonOp::Less:
      return "<";

    case ComparisonOp::LessEqual:
      return "<=";

    case ComparisonOp::Greater:
      return ">";

    case ComparisonOp::GreaterEqual:
      return ">=";
    }

    std::unreachable();
  }

  std::string to_string(
      BooleanOp op)
  {
    switch (op)
    {
    case BooleanOp::And:
      return "AND";

    case BooleanOp::Or:
      return "OR";
    }

    std::unreachable();
  }

  // ===========================================================================
  // Leaf expressions
  // ===========================================================================

  std::string to_string(
      const IntegerLiteral &literal)
  {
    return "IntegerLiteral{" +
           std::to_string(literal.val) +
           "}";
  }

  std::string to_string(
      const BoolLiteral &literal)
  {
    return std::string{
               "BoolLiteral{"} +
           (literal.value
                ? "true"
                : "false") +
           "}";
  }

  std::string to_string(
      const VariableReference &reference)
  {
    return "VariableReference{" +
           query_graph::to_string(
               reference.varId) +
           "}";
  }

  std::string to_string(
      const PropertyAccess &propertyAccess)
  {
    return "PropertyAccess{entity=" +
           query_graph::to_string(
               propertyAccess.entity) +
           ", property=" +
           common::quoted(propertyAccess.property) +
           "}";
  }

  std::string to_string(
      const Arithmetic &arithmetic)
  {
    return "Arithmetic{op=" +
           query_graph::to_string(
               arithmetic.op) +
           ", lhs=" +
           query_graph::to_string(
               arithmetic.lhs) +
           ", rhs=" +
           query_graph::to_string(
               arithmetic.rhs) +
           "}";
  }

  std::string to_string(
      const Comparison &comparison)
  {
    return "Comparison{op=" +
           query_graph::to_string(
               comparison.op) +
           ", lhs=" +
           query_graph::to_string(
               comparison.lhs) +
           ", rhs=" +
           query_graph::to_string(
               comparison.rhs) +
           "}";
  }

  std::string to_string(
      const BooleanBinary &booleanBinary)
  {
    return "BooleanBinary{op=" +
           query_graph::to_string(
               booleanBinary.op) +
           ", lhs=" +
           query_graph::to_string(
               booleanBinary.lhs) +
           ", rhs=" +
           query_graph::to_string(
               booleanBinary.rhs) +
           "}";
  }

  std::string to_string(
      const BooleanNot &booleanNot)
  {
    return "BooleanNot{operand=" +
           query_graph::to_string(
               booleanNot.operand) +
           "}";
  }

  std::string to_string(
      const HasLabel &hasLabel)
  {
    return "HasLabel{node=" +
           query_graph::to_string(
               hasLabel.node) +
           ", label=" +
           common::quoted(hasLabel.label) +
           "}";
  }

  std::string to_string(
      const HasRelType &hasRelType)
  {
    return "HasRelType{relationship=" +
           query_graph::to_string(
               hasRelType.relationship) +
           ", relType=" +
           common::quoted(hasRelType.relType) +
           "}";
  }

  // ===========================================================================
  // Expression
  // ===========================================================================

  /**
   * shallow to string
   * for example, prints BooleanNot(e12), and doesn't expand e12
   */
  std::string to_string(
      const Expression &expression)
  {
    std::string data = std::visit([](const auto &node) -> std::string
                                  { return query_graph::to_string(node); },
                                  expression.data);

    return "Expression{type=" +
           query_graph::to_string(
               expression.type) +
           ", data=" +
           data +
           "}";
  }

  /**
   * recursive to_string, which will recurse over the expression in order to
   * pretty print the whole thing
   */
  std::string expression_to_string(
      ExprId id,
      const ExpressionArena &arena)
  {
    // get expression object
    const Expression &expression = arena[id];
    return std::visit([&](const auto &node) -> std::string
                      {
          using T = std::decay_t<decltype(node)>;
          if constexpr (std::is_same_v<T, IntegerLiteral>)
          {
            return std::to_string(node.val);
          }

          else if constexpr (std::is_same_v<T, BoolLiteral>)
          {
            return node.value
                       ? "true"
                       : "false";
          }

          else if constexpr (std::is_same_v<T, VariableReference>)
          {
            return query_graph::to_string(node.varId);
          }

          else if constexpr (std::is_same_v<T, PropertyAccess>)
          {
            return expression_to_string(
                       node.entity,
                       arena) +
                   "." +
                   node.property;
          }

          else if constexpr (std::is_same_v<T, Arithmetic>)
          {
            return "(" +
                   expression_to_string(
                       node.lhs,
                       arena) +
                   " " +
                   query_graph::to_string(
                       node.op) +
                   " " +
                   expression_to_string(
                       node.rhs,
                       arena) +
                   ")";
          }

          else if constexpr (std::is_same_v<T, Comparison>)
          {
            return "(" +
                   expression_to_string(
                       node.lhs,
                       arena) +
                   " " +
                   query_graph::to_string(
                       node.op) +
                   " " +
                   expression_to_string(
                       node.rhs,
                       arena) +
                   ")";
          }

          else if constexpr (std::is_same_v<T, BooleanBinary>)
          {
            return "(" +
                   expression_to_string(
                       node.lhs,
                       arena) +
                   " " +
                   query_graph::to_string(
                       node.op) +
                   " " +
                   expression_to_string(
                       node.rhs,
                       arena) +
                   ")";
          }

          else if constexpr (std::is_same_v<T, BooleanNot>)
          {
            return "NOT (" +
                   expression_to_string(
                       node.operand,
                       arena) +
                   ")";
          }

          else if constexpr (std::is_same_v<T, HasLabel>)
          {
            return "HasLabel(" +
                   expression_to_string(
                       node.node,
                       arena) +
                   ", " +
                   common::quoted(node.label) +
                   ")";
          }

          else if constexpr (std::is_same_v<T, HasRelType>)
          {
            return "HasRelType(" +
                   expression_to_string(
                       node.relationship,
                       arena) +
                   ", " +
                   common::quoted(node.relType) +
                   ")";
          }
          else
          {
            static_assert(std::false_type::value, "Unhandled expression variant");
          } },
                      expression.data);
  }

  std::string to_string(const ExpressionArena &arena)
  {
    std::stringstream out;
    out << "ExpressionArena{\n";
    for (size_t i = 0; i < arena.expressions.size(); ++i)
    {
      ExprId id{static_cast<int64_t>(i)};
      out
          << "  "
          << query_graph::to_string(id)
          << ": "
          << query_graph::to_string(
                 arena.expressions[i])
          << "\n";
    }

    out << "}";

    return out.str();
  }

  // ===========================================================================
  // Pattern node
  // ===========================================================================

  std::string to_string(
      const PatternNode &node)
  {
    std::stringstream out;

    out
        << "PatternNode{variable="
        << query_graph::to_string(
               node.variable)
        << ", inlinePredicates=[";

    for (size_t i = 0;
         i < node.inlinePredicates.size();
         ++i)
    {
      if (i != 0)
      {
        out << ", ";
      }

      out << query_graph::to_string(
          node.inlinePredicates[i]);
    }

    out << "]}";

    return out.str();
  }

  std::string to_string(
      const PatternNode &node,
      const ExpressionArena &arena)
  {
    std::stringstream out;
    out
        << "PatternNode{variable="
        << query_graph::to_string(
               node.variable)
        << ", inlinePredicates=[";

    for (size_t i = 0; i < node.inlinePredicates.size(); ++i)
    {
      if (i != 0)
      {
        out << ", ";
      }

      out << expression_to_string(
          node.inlinePredicates[i],
          arena);
    }
    out << "]}";
    return out.str();
  }

  // ===========================================================================
  // Pattern relationship
  // ===========================================================================

  std::string to_string(const PatternRelationship &relationship)
  {
    std::stringstream out;
    out
        << "PatternRelationship{variable="
        << query_graph::to_string(
               relationship.variable)
        << ", start="
        << query_graph::to_string(
               relationship.startNode)
        << ", end="
        << query_graph::to_string(
               relationship.endNode)
        << ", direction="
        << common::to_string(
               relationship.direction)
        << ", inlinePredicates=[";

    for (size_t i = 0; i < relationship.inlinePredicates.size(); ++i)
    {
      if (i != 0)
      {
        out << ", ";
      }
      out << query_graph::to_string(
          relationship.inlinePredicates[i]);
    }
    out << "]}";
    return out.str();
  }

  std::string to_string(
      const PatternRelationship &relationship,
      const ExpressionArena &arena)
  {
    std::stringstream out;

    out
        << "PatternRelationship{variable="
        << query_graph::to_string(
               relationship.variable)
        << ", start="
        << query_graph::to_string(
               relationship.startNode)
        << ", end="
        << query_graph::to_string(
               relationship.endNode)
        << ", direction="
        << common::to_string(
               relationship.direction)
        << ", inlinePredicates=[";

    for (size_t i = 0; i < relationship.inlinePredicates.size(); ++i)
    {
      if (i != 0)
      {
        out << ", ";
      }

      out << expression_to_string(
          relationship.inlinePredicates[i],
          arena);
    }

    out << "]}";

    return out.str();
  }

  // ===========================================================================
  // Shared block
  // ===========================================================================

  std::string to_string(const SharedBlock &sharedBlock)
  {
    std::stringstream out;
    out
        << "SharedBlock{block_id="
        << query_graph::to_string(
               sharedBlock.block_id)
        << ", relationships=[";

    for (size_t i = 0; i < sharedBlock.patternRelationshipIDs.size(); ++i)
    {
      if (i != 0)
      {
        out << ", ";
      }
      out << query_graph::to_string(sharedBlock.patternRelationshipIDs[i]);
    }
    out << "]}";
    return out.str();
  }

  // ===========================================================================
  // Full query graph
  // ===========================================================================

  std::string to_string(
      const QueryGraph &queryGraph)
  {
    std::stringstream out;

    out << "QueryGraph {\n";

    // -----------------------------------------------------------------------
    // Nodes
    // -----------------------------------------------------------------------
    out << "  patternNodes: [\n";
    for (const PatternNode &node : queryGraph.patternNodes)
    {
      out
          << "    "
          << query_graph::to_string(
                 node,
                 queryGraph.expressions)
          << "\n";
    }

    out << "  ],\n";

    // -----------------------------------------------------------------------
    // Relationships
    // -----------------------------------------------------------------------

    out << "  patternRelationships: [\n";
    for (const PatternRelationship &relationship : queryGraph.patternRelationships)
    {
      out
          << "    "
          << query_graph::to_string(
                 relationship,
                 queryGraph.expressions)
          << "\n";
    }
    out << "  ],\n";

    // -----------------------------------------------------------------------
    // Other predicates
    // -----------------------------------------------------------------------

    out
        << "  otherPredicates: [\n";

    for (ExprId predicate : queryGraph.otherPredicates)
    {
      out
          << "    "
          << expression_to_string(
                 predicate,
                 queryGraph.expressions)
          << "\n";
    }

    out << "  ],\n";

    // -----------------------------------------------------------------------
    // Shared blocks
    // -----------------------------------------------------------------------

    out
        << "  sharedBlocks: [\n";

    for (const SharedBlock &block : queryGraph.sharedBlocks)
    {
      out
          << "    "
          << query_graph::to_string(block)
          << "\n";
    }
    out << "  ]\n";
    out << "}";
    return out.str();
  }

} // namespace query_graph