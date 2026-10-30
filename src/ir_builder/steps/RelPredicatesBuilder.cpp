#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>

namespace ir_builder
{

  namespace qg = query_graph;
  namespace cmn = common;
  namespace er = expression_recognition;

  namespace attacher
  {

    void attachRelationshipPredicate(size_t qi, qg::VariableId relVariable, GlobalEntities &ge, AtomicRelPredicate predicate)
    {

      QueryLocalToGlobal &qLocalToGlobal = ge.queryLocalToGlobals.at(qi);
      if (!qLocalToGlobal.rel_to_globalID.contains(relVariable))
      {
        throw CanonicalizationError(
            "predicate references unknown "
            "relationship VariableId " +
            std::to_string(
                relVariable.val));
      }

      GlobalRelId globalRelId = qLocalToGlobal.rel_to_globalID.at(relVariable);
      ge.relationships.at(globalRelId).atomicPredicates.push_back(std::move(predicate));
    }

    void attachNodePredicate(
        size_t qi,
        qg::VariableId nodeVariable,
        GlobalEntities &ge,
        std::function<AtomicRelPredicate(cmn::Endpoint)> make_predicate)
    {

      QueryLocalToGlobal &qLocalToGlobal = ge.queryLocalToGlobals.at(qi);
      if (!qLocalToGlobal.node_to_globalID.contains(nodeVariable))
      {
        throw CanonicalizationError(
            "predicate references unknown "
            "node VariableId " +
            std::to_string(
                nodeVariable.val));
      }

      auto nodeGlobalId = qLocalToGlobal.node_to_globalID.at(nodeVariable);
      if (!ge.adjListNode.contains(nodeGlobalId))
      {
        throw CanonicalizationError("invalid node");
      }
      if (ge.adjListNode[nodeGlobalId].empty())
      {
        throw CanonicalizationError(
            "predicate on isolated node "
            "VariableId " +
            std::to_string(
                nodeVariable.val) +
            " cannot be represented by "
            "relationship-only DSL");
      }

      for (const Incidence &incidence : ge.adjListNode[nodeGlobalId])
      {
        ge.relationships
            .at(incidence.relId)
                .atomicPredicates.push_back(
                    make_predicate(incidence.endpoint));
      }
    }

    /**
     * given `ParsedProperty{varId, property} = value`
     * - find out whether varId is a relationship or a node
     * - attach it appropiately
     */
    void attachPropertyEquals(
        size_t qi,
        GlobalEntities &ge,
        const er::ParsedPropertyAccess &property,
        int64_t value)
    {

      QueryLocalToGlobal &qLocalToGlobal = ge.queryLocalToGlobals.at(qi);

      // Relationship property.
      if (qLocalToGlobal.rel_to_globalID.contains(property.varId))
      {
        GlobalRelId relIndex = qLocalToGlobal.rel_to_globalID.at(property.varId);
        ge.relationships.at(relIndex).atomicPredicates.push_back(
            RelPropertyEquals{
                property.property,
                value});
        return;
      }

      // Node property.
      if (qLocalToGlobal.node_to_globalID.contains(property.varId))
      {
        attachNodePredicate(
            qi,
            property.varId,
            ge,
            [&](cmn::Endpoint endpoint)
                -> AtomicRelPredicate
            {
              return EndpointPropertyEquals{
                  endpoint,
                  property.property,
                  value};
            });

        return;
      }

      throw CanonicalizationError(
          "property access references unknown "
          "VariableId " +
          std::to_string(
              property.varId.val));
    }
  }

  /**
   * Given a query graph and pointer to expression to flatten
   * - populate `output` with ExprIds of flattened expressions from traversing an 'And' Tree
   */
  inline void flattenAnd(const qg::QueryGraph &qg, qg::ExprId id, std::vector<qg::ExprId> &output)
  {
    const qg::Expression &expr = qg.expressions[id];
    if (const auto *boolean = std::get_if<qg::BooleanBinary>(&expr.data))
    {
      if (boolean->op != qg::BooleanOp::And)
      {
        throw CanonicalizationError(
            "OR (and non AND) is not supported by the "
            "relationship predicate DSL");
      }
      flattenAnd(
          qg,
          boolean->lhs,
          output);

      flattenAnd(
          qg,
          boolean->rhs,
          output);
      return;
    }

    // non-and predicate, just return the ID immediately
    output.push_back(id);
  }

  void processAtomicPredicate(size_t qi, const qg::QueryGraph &qg, GlobalEntities &ge, qg::ExprId exprId)
  {
    const qg::Expression &predicate_expr = qg.expressions[exprId];
    if (predicate_expr.type != qg::ValueType::Bool)
    {
      throw CanonicalizationError(
          "top-level predicate must "
          "have Bool type");
    }

    // ---------------------------------------------------------
    // node:Label
    // ---------------------------------------------------------
    auto hasLabel = er::hasLabel(qg, exprId);
    if (hasLabel)
    {
      attacher::attachNodePredicate(qi, hasLabel->nodeId, ge, [&](cmn::Endpoint endpoint) -> AtomicRelPredicate
                                    { return EndpointHasLabel{
                                          endpoint,
                                          hasLabel->label}; });
      return;
    }

    // ---------------------------------------------------------
    // type(r) == R
    // ---------------------------------------------------------
    auto hasRelType = er::hasRelType(qg, exprId);
    if (hasRelType)
    {
      attacher::attachRelationshipPredicate(
          qi,
          hasRelType->relId,
          ge,
          RelTypeEquals{
              hasRelType->relType});

      return;
    }

    // ---------------------------------------------------------
    // property == integer
    // ---------------------------------------------------------
    const auto *comparison = std::get_if<qg::Comparison>(&predicate_expr.data);
    if (!comparison || comparison->op != qg::ComparisonOp::Equal)
    {
      throw CanonicalizationError(
          "only equality, HasLabel and "
          "HasRelType are supported");
    }
    auto lhsProperty = er::propertyAccess(qg, comparison->lhs);
    auto rhsProperty = er::propertyAccess(qg, comparison->rhs);
    auto lhsConstant = er::intConstant(qg, comparison->lhs);
    auto rhsConstant = er::intConstant(qg, comparison->rhs);

    // property = constant
    if (lhsProperty && rhsConstant)
    {
      attacher::attachPropertyEquals(qi, ge, *lhsProperty, *rhsConstant);
      return;
    }

    // constant = property
    if (rhsProperty && lhsConstant)
    {
      attacher::attachPropertyEquals(qi, ge, *rhsProperty, *lhsConstant);
      return;
    }

    throw CanonicalizationError("supported equality form is "
                                "<property> = <integer constant>");
  }

  std::vector<qg::ExprId> collectPredicatesFromQg(const qg::QueryGraph &qg)
  {
    std::vector<qg::ExprId> ans;
    for (const auto &node : qg.patternNodes)
    {
      for (qg::ExprId predicate : node.inlinePredicates)
      {
        ans.push_back(predicate);
      }
    }

    for (const auto &rel : qg.patternRelationships)
    {

      for (qg::ExprId predicate : rel.inlinePredicates)
      {
        ans.push_back(predicate);
      }
    }

    for (qg::ExprId predicate : qg.otherPredicates)
    {
      ans.push_back(predicate);
    }
    return ans;
  }

  void collectPredicates(const std::vector<qg::QueryGraph> &queryGraphs, GlobalEntities &ge)
  {
    for (size_t qi = 0; qi < queryGraphs.size(); ++qi)
    {
      const qg::QueryGraph &qg = queryGraphs[qi];

      // attach relevant predicate equals predicates
      for (const auto &rel : qg.patternRelationships)
      {
        if (rel.startNode == rel.endNode)
        {
          GlobalRelId rel_global = ge.queryLocalToGlobals[qi].rel_to_globalID.at(rel.variable);
          ge.relationships.at(rel_global).atomicPredicates.push_back(EndpointsEqual{});
        }
      }

      for (qg::ExprId predId : collectPredicatesFromQg(qg))
      {
        std::vector<qg::ExprId> atoms;
        flattenAnd(qg, predId, atoms);
        for (qg::ExprId atom : atoms)
        {
          processAtomicPredicate(qi, qg, ge, atom);
        }
      }
    }
  }
}