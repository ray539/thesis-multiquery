#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>

namespace ir_builder
{
  namespace qg = query_graph;
  namespace cmn = common;

  GlobalEntities buildGlobalEntities(const std::vector<qg::QueryGraph> &queryGraphs)
  {
    int64_t nextGlobalNodeId = 0;
    int64_t nextGlobalRelId = 0;

    GlobalEntities ge;
    ge.queryLocalToGlobals.resize(queryGraphs.size());

    for (size_t qi = 0; qi < queryGraphs.size(); qi++)
    {
      const qg::QueryGraph &qg = queryGraphs[qi];
      QueryLocalToGlobal &localToGlobals = ge.queryLocalToGlobals[qi];

      // Nodes
      // - within a given query graph, if 2 nodes have same variable they are
      //   the same node and given same ID
      for (const auto &node : qg.patternNodes)
      {
        if (!localToGlobals.node_to_globalID.contains(node.variable))
        {
          localToGlobals.node_to_globalID.emplace(
              node.variable,
              GlobalNodeId{nextGlobalNodeId++});
        }
      }

      // relationships
      for (const auto &rel : qg.patternRelationships)
      {
        // basic check to see its not a nodeId
        if (localToGlobals.node_to_globalID.contains(rel.variable))
        {
          throw CanonicalizationError(
              "VariableId " +
              std::to_string(rel.variable.val) +
              " is used as both a node and "
              "relationship");
        }

        if (localToGlobals.rel_to_globalID.contains(rel.variable))
        {
          throw CanonicalizationError(
              "relationship VariableId " +
              std::to_string(rel.variable.val) +
              " occurs more than once");
        }

        qg::VariableId localStart = rel.startNode;
        qg::VariableId localEnd = rel.endNode;
        // start = a, end = b
        // outgoing,  then a->b
        // incoming, then a<-b
        // therefore need to normalize
        if (rel.direction == cmn::Direction::Incoming)
        {
          std::swap(localStart, localEnd);
        }
        // auto startIt = state.node_to_globalID.find(localStart);
        // auto endIt = state.node_to_globalID.find(localEnd);
        if (!(localToGlobals.node_to_globalID.contains(localStart) &&
              localToGlobals.node_to_globalID.contains(localEnd)))
        {
          throw CanonicalizationError(
              "relationship " +
              std::to_string(rel.variable.val) +
              " references a missing node");
        }

        GlobalNodeId startNodeGlobalId = localToGlobals.node_to_globalID.at(localStart);
        GlobalNodeId endNodeGlobalId = localToGlobals.node_to_globalID.at(localEnd);

        GlobalRelId globalRelId{nextGlobalRelId++};
        ge.relationships.emplace(
            globalRelId,
            PatternRelationshipInternal{
                qi,

                rel.variable,
                localStart,
                localEnd,

                globalRelId,
                startNodeGlobalId,
                endNodeGlobalId,
                {},
                PredicateId{-1}});
            
        localToGlobals.rel_to_globalID.emplace(rel.variable, globalRelId);
        ge.adjListNode[startNodeGlobalId].push_back(Incidence{
            globalRelId,
            cmn::Endpoint::Start});

        ge.adjListNode[endNodeGlobalId].push_back(Incidence{
            globalRelId,
            cmn::Endpoint::End});
      }
    }
    return ge;
  }
}