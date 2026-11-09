#include <common/Execution.hpp>

namespace execution
{
  // ---------------------------------------------------------------------------
  // CandidateSpace
  // ---------------------------------------------------------------------------

  const std::vector<k::DRelId> &CandidateSpace::candidates(ib::PredicateId predicate) const
  {
    return candidatesBySignature_.at(predicate).candidates;
  }

  const std::vector<k::DRelId> &CandidateSpace::adj(
      ib::PredicateId sourcePredicate,
      k::DRelId source,
      ib::PredicateId targetPredicate,
      common::Adjacency adjacency) const
  {
    CandidateAdjacencyKey key{
        .sourcePredicate = sourcePredicate,
        .targetPredicate = targetPredicate,
        .adjacency = adjacency,
    };

    const CandidateAdjacency &candidateAdjacency = adjacency_.at(key);
    return candidateAdjacency.neighbors.at(source);
  }

  const QueryCandidateSpace &CandidateSpace::query(size_t queryIndex) const
  {
    return queries_.at(queryIndex);
  }

  // ---------------------------------------------------------------------------
  // CandidateSpaceBuilder
  // ---------------------------------------------------------------------------

  ib::PredicateId CandidateSpaceBuilder::signatureFor(
      ib::GlobalRelId relationship,
      size_t signatureLevel) const
  {
    if (signatureLevel >= buildResult_.signatures.size())
    {
      throw std::out_of_range("CandidateSpaceBuilder: requested signature level does not exist");
    }
    const auto &signatures = buildResult_.signatures.at(signatureLevel);
    if (!signatures.contains(relationship))
    {
      throw std::logic_error("CandidateSpaceBuilder: relationship has no signature at requested level");
    }
    return signatures.at(relationship);
  }

  CandidateSpace CandidateSpaceBuilder::build(size_t signatureLevel)
  {
    CandidateSpace result;
    auto [requiredPredicates, requiredAdjacencies] = collectRequirements(
        result,
        signatureLevel);

    buildCandidateSets(
        result,
        requiredPredicates);

    buildCandidateAdjacencies(
        result,
        requiredAdjacencies);

    return result;
  }

  // ---------------------------------------------------------------------------
  // Requirement collection
  // ---------------------------------------------------------------------------

  std::pair<
      CandidateSpaceBuilder::PredicateSet,
      CandidateSpaceBuilder::AdjacencyRequirementSet>
  CandidateSpaceBuilder::collectRequirements(
      CandidateSpace &space,
      size_t signatureLevel) const
  {
    space.queries_.resize(buildResult_.edgeGraphs.size());
    PredicateSet requiredPredicates;
    AdjacencyRequirementSet requiredAdjacencies;

    for (size_t queryIndex = 0; queryIndex < buildResult_.edgeGraphs.size(); ++queryIndex)
    {
      const ib::EdgeGraphCollapsed &edgeGraph = buildResult_.edgeGraphs.at(queryIndex);
      QueryCandidateSpace &querySpace = space.queries_.at(queryIndex);

      // -----------------------------------------------------------------------
      // build all query relationship -> signature maps in predicateOfRelationship
      // -----------------------------------------------------------------------
      for (size_t i = 0; i < edgeGraph.num_nodes(); ++i)
      {
        ib::EIdxCollapsed eidx{i};
        const ib::NodeType node = edgeGraph.node_at(eidx);
        const auto *relationship = std::get_if<ib::PatternRelationshipIR>(&node);

        if (relationship == nullptr)
        {
          throw std::logic_error("CandidateSpaceBuilder does not support SharedBlockReference nodes");
        }

        ib::PredicateId predicate = signatureFor(relationship->global_rel_id, signatureLevel);
        auto [_, inserted] = querySpace.predicateOfRelationship.emplace(relationship->global_rel_id, predicate);

        if (!inserted)
        {
          throw std::logic_error(
              "CandidateSpaceBuilder: duplicate relationship in query candidate space");
        }
        requiredPredicates.insert(predicate);
      }

      // -----------------------------------------------------------------------
      // transform each directed edge in the EdgeGraph into a CandidateAdjacency key
      // -----------------------------------------------------------------------
      for (size_t i = 0; i < edgeGraph.num_nodes(); ++i)
      {
        ib::EIdxCollapsed sourceIndex{i};
        const ib::NodeType sourceNode = edgeGraph.node_at(sourceIndex);
        const auto *sourceRelationship = std::get_if<ib::PatternRelationshipIR>(&sourceNode);

        if (sourceRelationship == nullptr)
        {
          throw std::logic_error(
              "CandidateSpaceBuilder currently does not support SharedBlockReference nodes");
        }

        ib::PredicateId sourcePredicate =
            querySpace.predicateOfRelationship.at(
                sourceRelationship->global_rel_id);

        for (const auto &edge : edgeGraph.adj(sourceIndex))
        {
          if (edge.adjType == common::Adjacency::BoundaryToSB)
          {
            throw std::logic_error(
                "CandidateSpaceBuilder encountered BoundaryToSB adjacency");
          }

          const ib::NodeType targetNode = edgeGraph.node_at(edge.other);

          const auto *targetRelationship = std::get_if<ib::PatternRelationshipIR>(&targetNode);

          if (targetRelationship == nullptr)
          {
            throw std::logic_error(
                "CandidateSpaceBuilder currently does not support SharedBlockReference nodes");
          }

          ib::PredicateId targetPredicate = querySpace.predicateOfRelationship.at(targetRelationship->global_rel_id);

          requiredAdjacencies.insert(
              CandidateAdjacencyKey{
                  .sourcePredicate = sourcePredicate,
                  .targetPredicate = targetPredicate,
                  .adjacency = edge.adjType,
              });
        }
      }
    }
    return {requiredPredicates, requiredAdjacencies};
  }

  // ---------------------------------------------------------------------------
  // Candidate sets
  // ---------------------------------------------------------------------------

  void CandidateSpaceBuilder::buildCandidateSets(
      CandidateSpace &space,
      const PredicateSet &requiredPredicates)
  {
    // Create all candidate sets first, including empty ones.
    for (ib::PredicateId predicate : requiredPredicates)
    {
      space.candidatesBySignature_.emplace(predicate, CandidateSet{});
    }

    k::RelationshipCursor cursor = database_.getRelationshipCursor();

    k::DRelId drel{0};
    while (cursor.next(drel))
    {
      for (ib::PredicateId predicate : requiredPredicates)
      {
        if (!predicateEvaluator_.evaluate(predicate, drel)) continue;
        space.candidatesBySignature_.at(predicate).addDrel(drel);
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Candidate adjacency
  // ---------------------------------------------------------------------------

  void CandidateSpaceBuilder::buildCandidateAdjacencies(
      CandidateSpace &space,
      const AdjacencyRequirementSet &requiredAdjacencies)
  {
    for (const CandidateAdjacencyKey &key : requiredAdjacencies)
    {
      buildCandidateAdjacency(
          space,
          key);
    }
  }

  void CandidateSpaceBuilder::buildCandidateAdjacency(
      CandidateSpace &space,
      const CandidateAdjacencyKey &key)
  {
    const CandidateSet &sourceCandidates = space.candidatesBySignature_.at(key.sourcePredicate);
    const CandidateSet &targetCandidates = space.candidatesBySignature_.at(key.targetPredicate);

    if (space.adjacency_.contains(key)) {
      throw std::logic_error(
          "CandidateSpaceBuilder: candidate adjacency constructed twice");
    }

    space.adjacency_.emplace(key, CandidateAdjacency{});
    CandidateAdjacency &candidateAdjacency = space.adjacency_.at(key);
    for (k::DRelId source : sourceCandidates.candidates)
    {
      // construct neighbors of this (p0,p1,type) -> drel
      auto [neighborIt, _] = candidateAdjacency.neighbors.emplace(source, std::vector<k::DRelId>{});
      std::vector<k::DRelId> &neighbors = neighborIt->second;
    
      if (targetCandidates.candidates.empty())
        continue;

      k::DNodeId anchorNode = database_.endpoint(source, common::myEndpoint(key.adjacency));
      common::Direction neighborDirection = common::neighborDirection(key.adjacency);
      size_t degree = database_.nodeDegree(anchorNode, neighborDirection);

      // -----------------------------------------------------------------------
      // Compute:
      //
      //   Neighbor(source) intersect C(targetPredicate)
      //
      // by traversing whichever side is smaller.
      // -----------------------------------------------------------------------
      if (degree <= targetCandidates.candidates.size())
      {
        buildAdjacencyUsingCursor(
            source,
            targetCandidates,
            key.adjacency,
            neighbors);
      }
      else
      {
        buildAdjacencyUsingCandidateSet(
            source,
            targetCandidates,
            key.adjacency,
            neighbors);
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Cursor-based adjacency construction
  // ---------------------------------------------------------------------------

  void CandidateSpaceBuilder::buildAdjacencyUsingCursor(
      k::DRelId source,
      const CandidateSet &targetCandidates,
      common::Adjacency adjacency,
      std::vector<k::DRelId> &output)
  {
    k::DNodeId anchorNode = database_.endpoint(source, common::myEndpoint(adjacency));
    common::Direction direction = common::neighborDirection(adjacency);
    k::RelationshipCursor cursor = database_.adjacent(anchorNode, direction);
    k::DRelId candidate{0};
    while (cursor.next(candidate))
    {
      if (targetCandidates.membership.contains(candidate))
        output.push_back(candidate);
    }
  }

  // ---------------------------------------------------------------------------
  // Candidate-set-based adjacency construction
  // ---------------------------------------------------------------------------

  void CandidateSpaceBuilder::buildAdjacencyUsingCandidateSet(
      k::DRelId source,
      const CandidateSet &targetCandidates,
      common::Adjacency adjacency,
      std::vector<k::DRelId> &output)
  {
    k::DNodeId sourceEndpoint = database_.endpoint(source, common::myEndpoint(adjacency));
    common::Endpoint targetEndpointType = common::neighborEndpoint(adjacency);

    for (k::DRelId target : targetCandidates.candidates)
    {
      k::DNodeId targetEndpoint = database_.endpoint(target, targetEndpointType);
      if (sourceEndpoint == targetEndpoint)
        output.push_back(target);
    }
  }

} // namespace execution