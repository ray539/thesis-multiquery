#include <bits/stdc++.h>
#include <common/Execution.hpp>

namespace execution
{

  namespace helpers
  {
    namespace ir = ir_builder;
    namespace qg = query_graph;
    namespace cmn = common;

    std::vector<std::vector<kernel::DRelId>> buildCandidateSets(
        kernel::CachedDatabaseAccess &database,
        const ir::PredicateRegistry &registry,
        PredicateEvaluator &evaluator)
    {
      std::vector<std::vector<kernel::DRelId>> candidateSets(registry.size());

      // scan all relationships
      // - for each relationship, evaluate against each pi. If true add it to candidate set pi
      auto cursor = database.getRelationshipCursor();
      kernel::DRelId drel{0};
      while (cursor.next(drel))
      {
        for (size_t pi = 0; pi < registry.size(); ++pi)
        {
          ir::PredicateId predId{pi};
          if (evaluator.evaluate(predId, drel))
          {
            candidateSets[pi].push_back(drel);
          }
        }
      }
      return candidateSets;
    }



    /**
     *  execution step 3: choose order we should traverse relationships in
     *  - the algorithm briefly works as follows
     *  - while (not all rels processed)
     *    - choose root rel with minimum |C[u]|/deg(u)
     *    - extend this component as much as you can, choosing neighbors based on |C[u]| / (1 + number of bound neighbors of u)
     *
     */
    std::vector<size_t> chooseRelationshipOrder(
        const ir::QueryGraphIR &qg,
        const std::vector<std::vector<kernel::DRelId>> &candidateSets,
        const std::vector<std::vector<QAdjacency>> &adj)
    {

      const size_t N = qg.patternRelationships.size();
      std::vector<size_t> order;
      order.reserve(N);
      std::vector<bool> processed(N, false);

      auto candidateCount = [&](size_t i) -> size_t
      {
        ir::PredicateId id = qg.patternRelationships[i].signatureId;
        return candidateSets[id.val].size();
      };

      auto degree = [&](size_t i) -> size_t
      {
        std::unordered_set<size_t> neighbors;
        for (const auto &x : adj[i])
          neighbors.insert(x.other);
        return neighbors.size();
      };

      auto matchedNeighborCount = [&](size_t i) -> size_t
      {
        std::unordered_set<size_t> neighbors;
        for (const auto &x : adj[i])
        {
          if (processed[x.other])
            neighbors.insert(x.other);
        }
        return neighbors.size();
      };

      //
      // Compare:
      //     |C(a)| / denominatorA
      //       vs
      //     |C(b)| / denominatorB
      // without floating point.
      //
      auto smallerRatio = [&](size_t a, size_t denominatorA, size_t b, size_t denominatorB)
      {
        __int128 lhs = static_cast<__int128>(candidateCount(a)) * denominatorB;
        __int128 rhs = static_cast<__int128>(candidateCount(b)) * denominatorA;
        return lhs < rhs;
      };

      auto chooseNewComponentRoot = [&]()
      {
        std::optional<size_t> best;
        for (size_t i = 0; i < N; ++i)
        {

          if (processed[i])
            continue;

          size_t denom = std::max<size_t>(degree(i), 1);

          if (!best)
          {
            best = i;
            continue;
          }
          size_t bestDenom = std::max<size_t>(degree(*best), 1);
          if (smallerRatio(
                  i,
                  denom,
                  *best,
                  bestDenom))
          {

            best = i;
          }
        }

        return *best;
      };

      // - First try to extend the current component
      // - on failure, choose a new root (and component)
      // - On the first pass, this will always fail and choose a new component
      //
      while (order.size() < N)
      {

        std::optional<size_t> best;
        for (size_t i = 0; i < N; ++i)
        {
          if (processed[i])
            continue;
          size_t boundNeighbors = matchedNeighborCount(i);
          if (boundNeighbors == 0)
            continue;
          if (!best)
          {
            best = i;
            continue;
          }

          /**
           * TODO: refactor this into a better function..
           * considering using a fraction class for all these type of ratio comparisons
           */
          size_t bestBoundNeighbors = matchedNeighborCount(*best);
          if (smallerRatio(i, 1 + boundNeighbors, *best, 1 + bestBoundNeighbors))
          {
            best = i;
          }
          else if (candidateCount(i) * (1 + bestBoundNeighbors) == candidateCount(*best) * (1 + boundNeighbors))
          {
            // Tie: prefer more already-bound neighbors.
            if (boundNeighbors > bestBoundNeighbors)
            {
              best = i;
            }
            // Then prefer higher future connectivity.
            else if (boundNeighbors == bestBoundNeighbors && degree(i) > degree(*best))
            {
              best = i;
            }
          }
        }

        if (!best)
          best = chooseNewComponentRoot();
        processed[*best] = true;
        order.push_back(*best);
      }

      return order;
    }

    /**
     * are two data graph relationships a,b adjacent with the specified adjacency?
     */
    bool areAdjacent(
        kernel::CachedDatabaseAccess &db,
        kernel::DRelId a,
        kernel::DRelId b,
        cmn::Adjacency adjacency)
    {
      switch (adjacency)
      {
      case cmn::Adjacency::StartStart:
        return db.endpoint(a, cmn::Endpoint::Start) == db.endpoint(b, cmn::Endpoint::Start);
      case cmn::Adjacency::StartEnd:
        return db.endpoint(a, cmn::Endpoint::Start) == db.endpoint(b, cmn::Endpoint::End);
      case cmn::Adjacency::EndStart:
        return db.endpoint(a, cmn::Endpoint::End) == db.endpoint(b, cmn::Endpoint::Start);
      case cmn::Adjacency::EndEnd:
        return db.endpoint(a, cmn::Endpoint::End) == db.endpoint(b, cmn::Endpoint::End);
      }
      assert(false);
      return false;
    }

  }

}