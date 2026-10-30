#include <bits/stdc++.h>
#include <common/Execution.hpp>

namespace execution
{

  namespace ir = ir_builder;
  namespace qg = query_graph;

  bool PredicateEvaluator::evaluate(ir_builder::PredicateId predicate, kernel::DRelId drel)
  {
    PredicateEvalKey key{
        drel,
        predicate};

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

  bool PredicateEvaluator::evaluateBase(const ir::BaseRelPredicate &predicate, kernel::DRelId drel)
  {
    for (const ir::AtomicRelPredicate &atom : predicate.atomicPredicates)
    {
      auto predicateOk = [&]()
      {
        if (auto *p = std::get_if<ir::RelPropertyEquals>(&atom))
        {
          return db_.relationshipProperty(drel, p->prop) == std::optional<int64_t>{p->val};
        }
        else if (auto *p = std::get_if<ir::RelTypeEquals>(&atom))
        {
          return db_.relationshipType(drel) == p->relType;
        }
        else if (auto *p = std::get_if<ir::EndpointPropertyEquals>(&atom))
        {
          kernel::DNodeId node = db_.endpoint(drel, p->endpoint);
          return db_.nodeProperty(node, p->prop) == std::optional<int64_t>{p->val};
        }
        else if (auto *p = std::get_if<ir::EndpointHasLabel>(&atom))
        {
          kernel::DNodeId node = db_.endpoint(drel, p->endpoint);
          return db_.nodeHasLabel(node, p->label);
        }
        else if (auto* p = std::get_if<ir::EndpointsEqual>(&atom))
        {
          kernel::DNodeId node1 = db_.endpoint(drel, common::Endpoint::Start);
          kernel::DNodeId node2 = db_.endpoint(drel, common::Endpoint::End);
          return node1 == node2;
        }

        assert(false);
        return false;
      };
      if (!predicateOk())
        return false;
    }
    return true;
  }

  std::vector<kernel::DRelId> PredicateEvaluator::matchingNeighbors(kernel::DRelId drel, const ir_builder::AdjacencyRequirement &req)
  {
    kernel::DNodeId sharedNode = db_.endpoint(drel, myEndpoint(req.adjacency));
    auto cursor = db_.adjacent(sharedNode, neighborDirection(req.adjacency));
    std::vector<kernel::DRelId> result;
    kernel::DRelId neighbor{0};

    while (cursor.next(neighbor))
    {
      // Relationship uniqueness.
      if (neighbor == drel)
        continue;
      if (evaluate(req.predId, neighbor))
      {
        result.push_back(neighbor);
      }
    }

    return result;
  }

  bool PredicateEvaluator::evaluateOther(const ir_builder::OtherRelPredicate &predicate, kernel::DRelId drel)
  {
    if (!evaluate(predicate.previousId, drel))
    {
      return false;
    }

    // LHS: expanded adjacency requirements, where each multiplicity becomes a node
    // - adjreq(adjacency, pred): req0, req1, req2 ..
    // RHS: data relationships adjacent to drel
    // - drels                    d0, d2, d3, d4 ...
    // options[ri]: drel[], list of drels which satisfy (adjacency, pred) of a req{ri}
    // - in bipartite graph, options[ri] containing dj means path from req{i} to d{j}

    // bipartite matching goal:
    // - each requirement must be matched at least once
    std::vector<std::vector<kernel::DRelId>> options;
    for (const auto &req : predicate.adjacencyRequirements)
    {
      // neighbors of drel which satisfy (adjacency, pred)
      auto neighbors = matchingNeighbors(drel, req);
      if (neighbors.size() < (size_t)req.multiplicity)
      {
        return false;
      }
      for (int64_t i = 0; i < req.multiplicity; ++i)
      {
        options.push_back(neighbors);
      }
    }

    // heuristic sort by number of options
    std::sort(options.begin(), options.end(), [](const auto &a, const auto &b)
              { return a.size() < b.size(); });

    // assignedSlot[ri] = index of rel idx assigned to ri
    std::unordered_map<kernel::DRelId, size_t, IntStrongHash> assignedSlot;

    // Kuhn's algorithm? idk tbh
    auto augment = [&](auto &&augment, size_t slot, std::unordered_set<kernel::DRelId, IntStrongHash> &seen) -> bool
    {
      for (kernel::DRelId candidate : options[slot])
      {
        if (seen.contains(candidate))
          continue;
        seen.insert(candidate);

        auto it = assignedSlot.find(candidate);
        if (it == assignedSlot.end() || augment(augment, it->second, seen))
        {
          assignedSlot[candidate] = slot;
          return true;
        }
      }
      return false;
    };

    for (size_t slot = 0; slot < options.size(); ++slot)
    {
      std::unordered_set<kernel::DRelId, IntStrongHash> seen;
      if (!augment(augment, slot, seen))
      {
        return false;
      }
    }
    return true;
  }

  bool PredicateEvaluator::evaluateUncached(
      ir::PredicateId predicateId,
      kernel::DRelId drel)
  {
    const ir::RelPredicate &predicate = registry_[predicateId];
    if (auto *p = std::get_if<ir::BaseRelPredicate>(&predicate))
    {
      return evaluateBase(*p, drel);
    }
    else if (auto *p = std::get_if<ir::OtherRelPredicate>(&predicate))
    {
      return evaluateOther(*p, drel);
    }
    assert(false);
    return false;
  }

}
