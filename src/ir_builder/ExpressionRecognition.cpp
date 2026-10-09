#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>

namespace qg = query_graph;
namespace ir_builder
{
  namespace expression_recognition
  {


    std::optional<qg::VariableId> variableReference(const qg::QueryGraph &qg, qg::ExprId id)
    {
      const qg::Expression &expr = qg.expressions[id];
      if (const auto *variable = std::get_if<qg::VariableReference>(&expr.data))
      {
        return variable->varId;
      }
      return std::nullopt;
    }

    std::optional<int64_t> intConstant(const qg::QueryGraph &qg, qg::ExprId id)
    {
      const qg::Expression &expr = qg.expressions[id];
      if (const auto *intc = std::get_if<qg::IntegerLiteral>(&expr.data))
      {
        return intc->val;
      }
      return std::nullopt;
    }

    std::optional<ParsedPropertyAccess> propertyAccess(const qg::QueryGraph &qg, qg::ExprId id)
    {

      // expr has to be 'PropertyAccess'
      const qg::Expression &expr = qg.expressions[id];
      const auto *property = std::get_if<qg::PropertyAccess>(&expr.data);
      if (!property)
      {
        return std::nullopt;
      }
      // property.entity has to match directVariable
      auto variable = variableReference(qg, property->entity);
      if (!variable)
      {
        return std::nullopt;
      }
      return ParsedPropertyAccess{
          *variable,
          property->property};
    }

    std::optional<ParsedHasLabel> hasLabel(const qg::QueryGraph &qg, qg::ExprId id)
    {
      const qg::Expression &expr = qg.expressions[id];
      const auto *haslabel = std::get_if<qg::HasLabel>(&expr.data);
      if (!haslabel)
      {
        return std::nullopt;
      }

      auto variable = variableReference(qg, haslabel->node);
      if (!variable)
      {
        return std::nullopt;
      }

      return ParsedHasLabel{
          *variable,
          haslabel->label};
    }

    std::optional<ParsedHasRelType> hasRelType(const qg::QueryGraph &qg, qg::ExprId id)
    {
      const qg::Expression &expr = qg.expressions[id];
      const auto *hasreltype = std::get_if<qg::HasRelType>(&expr.data);
      if (!hasreltype)
      {
        return std::nullopt;
      }

      auto variable = variableReference(qg, hasreltype->relationship);
      if (!variable)
      {
        return std::nullopt;
      }

      return ParsedHasRelType{
          *variable,
          hasreltype->relType};
    }
  }
}
