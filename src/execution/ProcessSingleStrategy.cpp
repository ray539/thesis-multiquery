#include <bits/stdc++.h>
#include <common/Execution.hpp>

namespace execution
{
  namespace ir = ir_builder;
  namespace qg = query_graph;
  namespace cmn = common;

  QueryExecutionResult ProcessSingleStrategy::processSingleQG(const ir::QueryGraphIR &qg)
  {
    const auto &qrels = qg.patternRelationships;
    const size_t N = qrels.size();
    auto qadj = helpers::buildQueryAdjacency(qg);
    std::vector<size_t> order = helpers::chooseRelationshipOrder(qg, candidateSets_, qadj);

    QueryExecutionResult result;
    for (size_t i : order)
    {
      result.matchingOrder.push_back(qrels[i].lv_global);
    }

    result.metrics
        .recursiveCallsAtDepth
        .assign(N + 1, 0);

    // calculate crude theoretical maximum number of returned embeddings
    // product(|C(qrel)|)
    for (const auto &qrel : qrels)
    {
      result.metrics.candidateProduct *=
          static_cast<long double>(
              candidateSets_[qrel.signatureId.val].size());
    }

    // assignment[qRelIndex] = data relationship
    std::vector<std::optional<kernel::DRelId>> assignment(N);
    // Cypher-style relationship uniqueness.
    std::unordered_set<kernel::DRelId> usedDataRelationships;

    /**
     * perform the actual backtracking
     * - lambda because it just depends on too much stuff
     */
    auto dfs = [&](auto &&self, size_t depth) -> void
    {
      ++result.metrics.recursiveCallsAtDepth[depth];
      if (depth == N)
      {
        Embedding embedding;
        for (size_t i = 0; i < N; ++i)
        {
          embedding.relationships.emplace(qrels[i].lv_global, *assignment[i]);
        }
        result.matches.push_back(std::move(embedding));
        ++result.metrics.completeMatches;
        return;
      }

      size_t qrelIndex = order[depth];
      const auto &qrel = qrels[qrelIndex];
      const std::vector<kernel::DRelId> &candidates = candidateSets_[qrel.signatureId.val];
      for (kernel::DRelId drel : candidates)
      {
        //
        // Relationships cannot be reused.
        //
        if (usedDataRelationships.contains(drel))
        {
          continue;
        }

        bool valid = true;

        //
        // Only constraints to already-assigned
        // q relationships matter.
        //
        for (const helpers::QAdjacency &constraint : qadj[qrelIndex])
        {
          size_t other = constraint.other;
          if (!assignment[other])
            continue;

          if (!helpers::areAdjacent(
                  database_,
                  drel,
                  *assignment[other],
                  constraint.type))
          {

            valid = false;
            break;
          }
        }

        if (!valid)
          continue;

        assignment[qrelIndex] = drel;
        usedDataRelationships.insert(drel);

        self(
            self,
            depth + 1);

        usedDataRelationships.erase(drel);
        assignment[qrelIndex].reset();
      }
    };

    dfs(dfs, 0);
    return result;
  }

  std::vector<QueryExecutionResult> ProcessSingleStrategy::run()
  {
    candidateSets_ = helpers::buildCandidateSets(database_, buildResult_.predicateRegistry, evaluator_);
    std::vector<QueryExecutionResult> results;
    for (const ir::QueryGraphIR &qg : buildResult_.queryGraphs)
    {
      results.push_back(processSingleQG(qg));
    }
    return results;
  }
}