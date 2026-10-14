#include <bits/stdc++.h>
#include <ir_builder/IRBuilderInternal.hpp>

namespace ir_builder
{

  namespace qg = query_graph;
  namespace cmn = common;
  namespace er = expression_recognition;

  std::vector<AdjacencyRequirement> buildAdjacencyRequirements(
      GlobalRelId relId,
      GlobalEntities &ge,
      const std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> &levelPrev)
  {
    const auto &rel = ge.relationships.at(relId);
    // counts[{adj_type, predId}]
    std::map<std::pair<cmn::Adjacency, PredicateId>, int64_t> counts;

    auto processEndpoint =
        [&](GlobalNodeId node,
            cmn::Endpoint myEndpoint)
    {
      if (!ge.adjListNode.contains(node))
        return;
      for (const Incidence &neighbor : ge.adjListNode[node])
      {
        // A relationship is not its own neighbor.
        if (neighbor.relId == relId)
        {
          continue;
        }
        cmn::Adjacency adjacency = adjacencyOf(myEndpoint, neighbor.endpoint);
        PredicateId neighborSignature = levelPrev.at(neighbor.relId);
        ++counts[{adjacency, neighborSignature}];
      }
    };

    processEndpoint(
        rel.globalStartNode,
        cmn::Endpoint::Start);

    processEndpoint(
        rel.globalEndNode,
        cmn::Endpoint::End);

    std::vector<AdjacencyRequirement>
        result;

    result.reserve(counts.size());

    for (const auto &[key, multiplicity] : counts)
    {
      result.push_back(AdjacencyRequirement{key.first, key.second, multiplicity});
    }

    return result;
  }

  SignatureBuilderResult buildAndInternSignatures(
      const std::vector<qg::QueryGraph> &queryGraphs,
      GlobalEntities &ge,
      int maxLayers)
  {
    std::vector<std::unordered_map<GlobalRelId, PredicateId, IntStrongHash>> signatures;
    PredicateRegistry pRegistry;

    // build level 0
    // previous[relIdx] = predicate for relationship at index relId
    {
      std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> layer0;
      for (auto &[_, rel] : ge.relationships)
      {
        BaseRelPredicate predicate{.atomicPredicates = rel.atomicPredicates};
        PredicateId id = pRegistry.intern(predicate);
        rel.signatureId = id;
        layer0.emplace(rel.globalId, id);
      }
      signatures.push_back(layer0);
    }

    // layer 1 ... 3
    for (int layer = 1; layer <= maxLayers; layer++)
    {
      // current[relIdx] = predicate for relationship at index relId
      std::vector<PredicateId> current;
      std::unordered_map<GlobalRelId, PredicateId, IntStrongHash> levelk;

      for (auto &[relId, rel] : ge.relationships)
      {
        auto &levelk1 = signatures.back();

        auto requirements =
            buildAdjacencyRequirements(
                relId,
                ge,
                levelk1);

        OtherRelPredicate predicate{
            .previousId = levelk1.at(relId),
            .adjacencyRequirements = std::move(requirements)};

        PredicateId predId = pRegistry.intern(std::move(predicate));
        current.push_back(predId);
        ge.relationships[relId].signatureId = predId;
        levelk[relId] = predId;

        levelk1 = levelk;
      }
      signatures.push_back(levelk);
    }

    return SignatureBuilderResult{
        .signatures = signatures,
        .pRegistry = pRegistry};
  }
}