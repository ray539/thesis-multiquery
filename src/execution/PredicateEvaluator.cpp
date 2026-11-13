#include <bits/stdc++.h>
#include <common/Execution.hpp>

namespace execution
{
  namespace ir = ir_builder;

  bool PredicateEvaluator::evaluate(ir::PredicateId predicate, k::DRelId drel)
  {
    PredicateEvalKey key{
        .rel = drel,
        .predicate = predicate,
    };
    ++metrics_.predicateCache.lookups;
    if (auto it = cache_.find(key); it != cache_.end())
    {
      ++metrics_.predicateCache.hits;
      return it->second;
    }
    ++metrics_.predicateCache.misses;

    bool result = evaluateUncached(predicate, drel);
    cache_.emplace(key, result);
    return result;
  }

  // ---------------------------------------------------------------------------
  // Base predicates
  // ---------------------------------------------------------------------------

  bool PredicateEvaluator::evaluateBase(
      const ir::BaseRelPredicate &predicate,
      k::DRelId drel)
  {
    for (const ir::AtomicRelPredicate &atom : predicate.atomicPredicates)
    {
      auto predicateOk = [&]()
      {
        if (auto *p = std::get_if<ir::RelPropertyEquals>(&atom))
        {
          return db_.relationshipProperty(drel, p->prop) == std::optional<int64_t>{p->val};
        }

        if (auto *p = std::get_if<ir::RelTypeEquals>(&atom))
        {
          return db_.relationshipType(drel) == p->relType;
        }
        if (auto *p = std::get_if<ir::EndpointPropertyEquals>(&atom))
        {
          k::DNodeId node = db_.endpoint(drel, p->endpoint);
          return db_.nodeProperty(node, p->prop) == std::optional<int64_t>{p->val};
        }

        if (auto *p = std::get_if<ir::EndpointHasLabel>(&atom))
        {
          k::DNodeId node = db_.endpoint(drel, p->endpoint);
          return db_.nodeHasLabel(node, p->label);
        }

        if (std::holds_alternative<ir::EndpointsEqual>(atom))
        {
          k::DNodeId start = db_.endpoint(drel, common::Endpoint::Start);
          k::DNodeId end = db_.endpoint(drel, common::Endpoint::End);
          return start == end;
        }

        throw std::logic_error(
            "PredicateEvaluator: unknown atomic predicate");
      };

      if (!predicateOk())
        return false;
    }

    return true;
  }

  // ---------------------------------------------------------------------------
  // Neighbor helpers
  // ---------------------------------------------------------------------------

  bool PredicateEvaluator::hasMatchingNeighbor(
      k::DRelId drel,
      const ir::AdjacencyRequirement &req)
  {
    k::DNodeId sharedNode = db_.endpoint(drel, common::myEndpoint(req.adjacency));
    auto cursor = db_.adjacent(sharedNode, common::neighborDirection(req.adjacency));
    k::DRelId neighbor{0};
    while (cursor.next(neighbor))
    {
      // Relationship uniqueness.
      if (neighbor == drel)
        continue;

      if (evaluate(req.predId, neighbor))
        return true;
    }

    return false;
  }

  std::vector<k::DRelId> PredicateEvaluator::matchingNeighbors(
      k::DRelId drel,
      const ir::AdjacencyRequirement &req)
  {
    k::DNodeId sharedNode = db_.endpoint(drel, common::myEndpoint(req.adjacency));
    auto cursor = db_.adjacent(sharedNode, common::neighborDirection(req.adjacency));
    std::vector<k::DRelId> result;
    k::DRelId neighbor{0};

    while (cursor.next(neighbor))
    {
      // Relationship uniqueness.
      if (neighbor == drel)
        continue;

      if (evaluate(req.predId, neighbor))
        result.push_back(neighbor);
    }

    return result;
  }

  size_t PredicateEvaluator::availableNeighborDegree(k::DRelId drel, common::Adjacency adjacency)
  {
    common::Endpoint myEndpoint = common::myEndpoint(adjacency);
    common::Endpoint neighborEndpoint = common::neighborEndpoint(adjacency);

    k::DNodeId sharedNode = db_.endpoint(drel, myEndpoint);

    size_t result = db_.nodeDegree(sharedNode, common::neighborDirection(adjacency));

    /**
     * nodeDegree() includes drel itself exactly when drel's endpoint on the
     * neighbor side is also the shared node.
     *
     * Examples:
     *
     * StartStart:
     *   drel is always outgoing from start(drel), so subtract one.
     *
     * EndEnd:
     *   drel is always incoming to end(drel), so subtract one.
     *
     * StartEnd / EndStart:
     *   drel is only returned itself if it is a self-loop.
     */
    k::DNodeId drelNeighborEndpoint = db_.endpoint(drel, neighborEndpoint);

    if (drelNeighborEndpoint == sharedNode)
    {
      if (result == 0)
      {
        throw std::logic_error(
            "PredicateEvaluator: inconsistent node degree");
      }
      --result;
    }

    return result;
  }

  // ---------------------------------------------------------------------------
  // Condition 1: existence
  // ---------------------------------------------------------------------------

  bool PredicateEvaluator::evaluateExistence(
      const ir::OtherRelPredicate &predicate,
      k::DRelId drel)
  {
    for (const ir::AdjacencyRequirement &req : predicate.adjacencyRequirements)
    {
      if (req.multiplicity <= 0)
      {
        throw std::logic_error(
            "PredicateEvaluator: adjacency requirement has invalid multiplicity");
      }

      /**
       * Multiplicity is deliberately ignored here.
       *
       * A requirement such as:
       *
       *   (StartEnd, p3, multiplicity = 4)
       *
       * only requires:
       *
       *   exists one StartEnd neighbor satisfying p3.
       */
      if (!hasMatchingNeighbor(drel, req))
        return false;
    }

    return true;
  }

  // ---------------------------------------------------------------------------
  // Condition 2: existence + degree
  // ---------------------------------------------------------------------------

  bool PredicateEvaluator::evaluateExistenceDegree(
      const ir::OtherRelPredicate &predicate,
      k::DRelId drel)
  {
    /**
     * Required number of query neighbors for each adjacency orientation.
     *
     * Example:
     *
     *   StartStart p1 x2
     *   StartStart p2 x1
     *   EndStart   p3 x2
     *
     * gives:
     *
     *   requiredDegree[StartStart] = 3
     *   requiredDegree[EndStart]   = 2
     */
    std::map<common::Adjacency, size_t> requiredDegree;
    for (const ir::AdjacencyRequirement &req : predicate.adjacencyRequirements)
    {
      if (req.multiplicity <= 0)
      {
        throw std::logic_error(
            "PredicateEvaluator: adjacency requirement has invalid multiplicity");
      }

      // First part: every required predicate must exist at least once.
      if (!hasMatchingNeighbor(drel, req)) return false;
      requiredDegree[req.adjacency] += static_cast<size_t>(req.multiplicity);
    }

    // Second part: raw data degree for each adjacency orientation must be large
    // enough to support the number of required query neighbors.
    for (const auto &[adjacency, required] : requiredDegree)
    {
      size_t available = availableNeighborDegree(drel, adjacency);
      if (available < required)
        return false;
    }
    return true;
  }

  // ---------------------------------------------------------------------------
  // Condition 3: bipartite matching
  // ---------------------------------------------------------------------------

  bool PredicateEvaluator::evaluateBipartiteMatching(
      const ir::OtherRelPredicate &predicate,
      k::DRelId drel)
  {
    /**
     * LHS:
     *
     * Expand each adjacency requirement according to multiplicity.
     *
     * Example:
     *
     *   (StartEnd, p1, multiplicity=2)
     *   (EndStart, p2, multiplicity=1)
     *
     * becomes slots:
     *
     *   q0 : StartEnd / p1
     *   q1 : StartEnd / p1
     *   q2 : EndStart / p2
     *
     * RHS:
     *
     * Actual data relationships adjacent to drel.
     *
     * options[i] contains all DRelIds which are valid assignments for slot i.
     */
    std::vector<std::vector<k::DRelId>> options;

    for (const ir::AdjacencyRequirement &req : predicate.adjacencyRequirements)
    {
      if (req.multiplicity <= 0)
      {
        throw std::logic_error(
            "PredicateEvaluator: adjacency requirement has invalid multiplicity");
      }

      std::vector<k::DRelId> neighbors = matchingNeighbors(drel, req);

      /**
       * Cheap failure check before bipartite matching.
       *
       * A requirement with multiplicity m needs at least m distinct matching
       * data relationships.
       */
      if (neighbors.size() < static_cast<size_t>(req.multiplicity))
        return false;

      for (int64_t i = 0; i < req.multiplicity; ++i)
        options.push_back(neighbors);
    }

    // Try the most constrained query-neighbor slots first.
    std::sort(
        options.begin(),
        options.end(),
        [](const auto &lhs, const auto &rhs)
        {
          return lhs.size() < rhs.size();
        });

    /**
     * assignedSlot[drel] = query-neighbor slot currently assigned to drel.
     */
    std::unordered_map<
        k::DRelId,
        size_t,
        IntStrongHash>
        assignedSlot;

    /**
     * Standard augmenting-path bipartite matching.
     */
    auto augment = [&](auto &&augment, size_t slot, std::unordered_set<k::DRelId, IntStrongHash> &seen) -> bool
    {
      for (k::DRelId candidate : options[slot])
      {
        if (seen.contains(candidate))
          continue;

        seen.insert(candidate);
        auto it = assignedSlot.find(candidate);
        if (it == assignedSlot.end() || augment(augment, it->second,seen))
        {
          assignedSlot[candidate] = slot;
          return true;
        }
      }
      return false;
    };

    for (size_t slot = 0; slot < options.size(); ++slot)
    {
      std::unordered_set<k::DRelId, IntStrongHash> seen;
      if (!augment(augment, slot, seen))
      {
        return false;
      }
    }
    return true;
  }

  // ---------------------------------------------------------------------------
  // Hierarchical predicate
  // ---------------------------------------------------------------------------

  bool PredicateEvaluator::evaluateOther(
      const ir::OtherRelPredicate &predicate,
      k::DRelId drel)
  {
    /**
     * Every hierarchical signature still implies the previous level.
     */
    if (!evaluate(predicate.previousId, drel))
    {
      return false;
    }

    switch (config_.neighborhoodCondition)
    {
    case NeighborhoodCondition::Existence:
      return evaluateExistence(predicate, drel);

    case NeighborhoodCondition::ExistenceDegree:
      return evaluateExistenceDegree(predicate, drel);

    case NeighborhoodCondition::BipartiteMatching:
      return evaluateBipartiteMatching(predicate, drel);
    }

    throw std::logic_error(
        "PredicateEvaluator: unknown neighborhood condition");
  }

  // ---------------------------------------------------------------------------
  // Predicate dispatch
  // ---------------------------------------------------------------------------

  bool PredicateEvaluator::evaluateUncached(
      ir::PredicateId predicateId,
      k::DRelId drel)
  {
    const ir::RelPredicate &predicate = registry_[predicateId];
    if (auto *p = std::get_if<ir::BaseRelPredicate>(&predicate))
    {
      return evaluateBase(*p, drel);
    }

    if (auto *p = std::get_if<ir::OtherRelPredicate>(&predicate))
    {
      return evaluateOther(*p, drel);
    }
    throw std::logic_error(
        "PredicateEvaluator: unknown predicate type");
  }

} // namespace execution