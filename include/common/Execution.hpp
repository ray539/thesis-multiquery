#pragma once
#include <bits/stdc++.h>
#include <common/IRBuilder.hpp>
#include <common/Kernel.hpp>

namespace execution
{
  struct PredicateEvalKey
  {
    kernel::DRelId rel;
    ir_builder::PredicateId predicate;
    bool operator==(const PredicateEvalKey &) const = default;
  };
}

template <>
struct std::hash<execution::PredicateEvalKey>
{
  size_t operator()(const execution::PredicateEvalKey &x) const noexcept
  {
    size_t h = std::hash<kernel::DRelId>{}(x.rel);
    common::hashCombine(h, x.predicate.val);
    return h;
  }
};

namespace execution
{

  /**
   * predicate evaluator which automatically caches accesses
   * after the first access
   */
  class PredicateEvaluator
  {
  public:
    PredicateEvaluator(
        ir_builder::PredicateRegistry &registry,
        kernel::CachedDatabaseAccess &db,
        kernel::DatabaseMetrics &metrics)
        : registry_{registry},
          db_{db},
          metrics_{metrics}
    {
    }

    /**
     * cached evaluation
     */
    bool evaluate(ir_builder::PredicateId predicate, kernel::DRelId drel);

  private:
    ir_builder::PredicateRegistry &registry_;
    kernel::CachedDatabaseAccess &db_;
    kernel::DatabaseMetrics &metrics_;
    std::unordered_map<PredicateEvalKey, bool> cache_;

    bool evaluateBase(const ir_builder::BaseRelPredicate &predicate, kernel::DRelId drel);

    std::vector<kernel::DRelId> matchingNeighbors(kernel::DRelId drel, const ir_builder::AdjacencyRequirement &req);

    // main thing we are trying to solve here
    // - the predicate requirements form a tiny BIPARTITE MATCHING problem
    bool evaluateOther(const ir_builder::OtherRelPredicate &predicate, kernel::DRelId drel);
    bool evaluateUncached(ir_builder::PredicateId predicateId, kernel::DRelId drel);
  };

  struct Embedding
  {
    std::unordered_map<
        ir_builder::GlobalRelId,
        kernel::DRelId>
        relationships;
  };

  namespace helpers
  {

    /**
     * execution step 1:
     * - go over all datagraph relationships using a database cursor
     * - evaluate each predicate pi in `registry`. Add to `candidateSets[pi]` if true
     */
    std::vector<std::vector<kernel::DRelId>> buildCandidateSets(
        kernel::CachedDatabaseAccess &database,
        const ir_builder::PredicateRegistry &registry,
        PredicateEvaluator &evaluator);

    struct QAdjacency
    {
      size_t other;
      common::Adjacency type;
    };

    /**
     * execution step2: build adjacency list based on QueryGraphIR
     * - we treat each relationship as a 'node'
     * - the edges can then be annotated with 4 types of adjacency, as given by `Adjacency`
     */
    std::vector<std::vector<QAdjacency>> buildQueryAdjacency(const ir_builder::QueryGraphIR &qg);

    /**
     *  execution step 3: choose order we should traverse relationships in
     *  - the algorithm briefly works as follows
     *  - while (not all rels processed)
     *    - choose root rel with minimum |C[u]|/deg(u)
     *    - extend this component as much as you can, choosing neighbors based on |C[u]| / (1 + number of bound neighbors of u)
     *
     */
    std::vector<size_t> chooseRelationshipOrder(
        const ir_builder::QueryGraphIR &qg,
        const std::vector<std::vector<kernel::DRelId>> &candidateSets,
        const std::vector<std::vector<QAdjacency>> &adj);

    /**
     * are two data graph relationships a,b adjacent with the specified adjacency?
     */
    bool areAdjacent(
        kernel::CachedDatabaseAccess &db,
        kernel::DRelId a,
        kernel::DRelId b,
        common::Adjacency adjacency);
  }

  // metrics relating to the backtracking executor
  struct QueryExecutionMetrics
  {
    // recursiveCallsAtDepth[k] = number of calls to dfs(k)
    // Size is |Q_E| + 1 because the terminal call is also recorded.
    std::vector<uint64_t> recursiveCallsAtDepth;

    // complete matches returned
    uint64_t completeMatches = 0;

    // product(|C(r)|) over query relationships.
    uint64_t candidateProduct = 1;
    /**
     * no. matched embeddings / product(|C(r)|).
     * - indicates the effectiveness of the candidate sets
     */
    long double matchFraction() const
    {
      if (candidateProduct == 0)
      {
        std::cout << "matchFraction::warning: candidateProduct == 0 so returning 0";
        return 0.0;
      }

      return static_cast<long double>(completeMatches) / static_cast<long double>(candidateProduct);
    }
  };

  /**
   * Result for ONE query graph
   */
  struct QueryExecutionResult
  {
    std::vector<Embedding> matches;
    QueryExecutionMetrics metrics;
    std::vector<ir_builder::GlobalRelId> matchingOrder;
  };

  /**
   * This stragey will
   * - build shared candidate sets
   * - use the shared candidate sets to backtrack match ONE QUERY at a time
   */
  class ProcessSingleStrategy
  {
  public:
    ProcessSingleStrategy(
        ir_builder::BuildResult &&buildResult,
        kernel::CachedDatabaseAccess &database
      )
        : buildResult_(std::move(buildResult)),
          database_(database),
          evaluator_(buildResult.predicateRegistry, database, database.metrics())
    {
    }
    std::vector<QueryExecutionResult> run();

  private:
    // initialized by constructor
    ir_builder::BuildResult buildResult_;
    kernel::CachedDatabaseAccess database_;
    execution::PredicateEvaluator evaluator_;

    // built by the class
    std::vector<std::vector<kernel::DRelId>> candidateSets_;
    QueryExecutionResult processSingleQG(const ir_builder::QueryGraphIR &qg);
  };
}