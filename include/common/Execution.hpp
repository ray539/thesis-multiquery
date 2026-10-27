#pragma once

#include <bits/stdc++.h>
#include <generator>

#include <common/IRBuilder.hpp>
#include <common/Kernel.hpp>

namespace execution
{
  namespace ib = ir_builder;
  namespace qg = query_graph;
  namespace k = kernel;

  // ---------------------------------------------------------------------------
  // Predicate evaluator -- keep your existing implementation
  // ---------------------------------------------------------------------------

  struct PredicateEvalKey
  {
    k::DRelId rel;
    ib::PredicateId predicate;

    bool operator==(const PredicateEvalKey &) const = default;
  };
}

template <>
struct std::hash<execution::PredicateEvalKey>
{
  size_t operator()(const execution::PredicateEvalKey &x) const noexcept
  {
    size_t h = IntStrongHash{}(x.rel);
    common::hashCombine(h, x.predicate.val);
    return h;
  }
};

namespace execution
{
  class PredicateEvaluator
  {
  public:
    PredicateEvaluator(
        ib::PredicateRegistry &registry,
        k::CachedDatabaseAccess &db,
        k::DatabaseMetrics &metrics)
        : registry_{registry},
          db_{db},
          metrics_{metrics}
    {
    }

    bool evaluate(
        ib::PredicateId predicate,
        k::DRelId drel);

  private:
    ib::PredicateRegistry &registry_;
    k::CachedDatabaseAccess &db_;
    k::DatabaseMetrics &metrics_;

    std::unordered_map<
        PredicateEvalKey,
        bool>
        cache_;

    bool evaluateBase(
        const ib::BaseRelPredicate &predicate,
        k::DRelId drel);

    std::vector<k::DRelId> matchingNeighbors(
        k::DRelId drel,
        const ib::AdjacencyRequirement &req);

    bool evaluateOther(
        const ib::OtherRelPredicate &predicate,
        k::DRelId drel);

    bool evaluateUncached(
        ib::PredicateId predicateId,
        k::DRelId drel);
  };

  // VariableId -> DRelId output for a query
  struct Embedding
  {
    std::unordered_map<qg::VariableId, k::DRelId, IntStrongHash> relationships;
  };

  // ---------------------------------------------------------------------------
  // Shared block rows
  //
  // Context-independent row using SharedBlockIR-local EIdx values.
  // ---------------------------------------------------------------------------

  /**
   * fields
   * - `relationships: map<SB::EIdx, DRelId`: the actual embedding data
   */
  struct SharedBlockRow
  {
    std::unordered_map<
        ib::SharedBlockIR::EIdx,
        k::DRelId,
        IntStrongHash>
        relationships;
  };

  struct SharedBlockCacheKey
  {
    qg::BlockId block_id;
    // nullopt for a context-free/root block.
    std::optional<k::DRelId> boundary;
    bool operator==(const SharedBlockCacheKey &) const = default;
  };

  struct SharedBlockCacheKeyHash
  {
    size_t operator()(const SharedBlockCacheKey &key) const noexcept
    {
      size_t h = IntStrongHash{}(key.block_id);
      common::hashCombine(h, key.boundary.has_value());
      if (key.boundary)
      {
        common::hashCombine(h, key.boundary->val);
      }
      return h;
    }
  };

  // ---------------------------------------------------------------------------
  // Metrics
  // ---------------------------------------------------------------------------

  struct SharedBlockExecutionMetrics
  {
    uint64_t iteratorCreations = 0;
    uint64_t cacheEntriesCreated = 0;

    uint64_t rowRequests = 0;
    uint64_t cachedRowsReturned = 0;
    uint64_t rowsProduced = 0;

    uint64_t producerResumptions = 0;
    uint64_t producerExhaustions = 0;

    uint64_t candidateRelationshipsVisited = 0;
    uint64_t rejectedByUniqueness = 0;
    uint64_t rejectedByAdjacency = 0;
    uint64_t rejectedByPredicate = 0;

    // Aggregated over all producers of the same BlockId.
    // eg. recursiveCallsAtDepth[BlockID(1)][level = 3]
    std::unordered_map<qg::BlockId, std::vector<uint64_t>, IntStrongHash> recursiveCallsAtDepth;
  };

  struct QueryExecutionMetrics
  {
    // Outer collapsed-edge-graph recursion.
    std::vector<uint64_t> recursiveCallsAtDepth;

    uint64_t completeMatches = 0;

    uint64_t regularCandidatesVisited = 0;
    uint64_t regularRejectedByUniqueness = 0;
    uint64_t regularRejectedByAdjacency = 0;
    uint64_t regularRejectedByPredicate = 0;

    uint64_t sharedBlockRowsVisited = 0;
    uint64_t sharedBlockRowsRejectedByOuterUniqueness = 0;

    /**
     * edge graph order chosen on FIRST search branch.
     * Later branches are NOT reflected here
     */
    std::vector<ib::EIdxCollapsed> matchingOrder;
  };

  struct QueryExecutionResult
  {
    std::vector<Embedding> matches;
    QueryExecutionMetrics metrics;
  };

  // -------------------
  // DFSHelper
  // -------------------
  namespace dfs_helper
  {

    /**
     * describes datagraph neighbor requirements of `me:DRelID`
     * fields:
     * - `adjacency: common::Adjacency`: StartStart ... EndEnd
     * - `otherDRel: DRelId`: required neighbor
     *
     * datagraph neighbor requirements can be checked by inspecting endpoint nodes
     */
    struct BoundConstraint
    {
      common::Adjacency adjacency;
      k::DRelId otherDRel;
    };

    /**
     * inspect edge graph for nodes adjacent to `current`
     * return
     * - list of `(adjacency, other:DRel)` pairs describing requirements of a candidate data relationship `me:DRel`
     * - the list dictates that `me` has to be datagraph adjacent to all the relationships in
     *   `other` in the with the given adjacency type
     */
    std::vector<BoundConstraint> regularBoundConstraints(
        const ib::EdgeGraphCollapsed &edgeGraph,
        ib::EIdxCollapsed current,
        const Embedding &embedding,
        const std::unordered_set<
            ib::EIdxCollapsed,
            IntStrongHash> &processed);

    /**
     * inspect edge graph for nodes adjacent to `current`
     * same as `regularBoundConstraints`, but functionality specialized for shared block assignments
     */
    std::vector<BoundConstraint> SBBoundConstraints(
        const ib::SharedBlockIR::EdgeGraphSB &edgeGraph,
        ib::SharedBlockIR::EIdx current,

        const std::unordered_map<
            ib::SharedBlockIR::EIdx,
            k::DRelId,
            IntStrongHash> &assignment);

    /**
     * returns cursor which yields a superset of relationships which will
     * satisfy constraints.
     */
    k::RelationshipCursor regularCandidateCursor(
        const std::vector<BoundConstraint> &constraints,
        k::CachedDatabaseAccess &cached_database);

    /**
     * whether a candidate DRel satisfies the constraints
     */
    bool satisfiesConstraints(
        k::DRelId candidate,
        const std::vector<BoundConstraint> &constraints,
        k::CachedDatabaseAccess &cached_database);

    std::optional<size_t> indexWhichArgmaxes(
        std::vector<size_t> indices,
        std::function<bool(size_t)> &&filter,
        std::function<int64_t(size_t)> &&scorer);

  } // namespace dfs_helper

  // ---------------------------------------------------------------------------
  // SharedBlockIterator
  // ---------------------------------------------------------------------------

  class SharedBlockIteratorManager;
  class SharedBlockIterator
  {
  public:
    std::optional<SharedBlockRow> next();

  private:
    friend class SharedBlockIteratorManager;

    SharedBlockIterator(SharedBlockIteratorManager *manager, SharedBlockCacheKey key)
        : manager_{manager},
          key_{std::move(key)}
    {
    }

    SharedBlockIteratorManager *manager_;
    SharedBlockCacheKey key_;
    size_t next_index_ = 0;
  };

  // ---------------------------------------------------------------------------
  // SharedBlockIteratorManager
  // ---------------------------------------------------------------------------

  class SharedBlockIteratorManager
  {
  public:
    SharedBlockIteratorManager(
        const std::unordered_map<
            qg::BlockId,
            ib::SharedBlockIR,
            IntStrongHash> &blocks,
        PredicateEvaluator &predicateEvaluator,
        k::CachedDatabaseAccess &db,
        SharedBlockExecutionMetrics &metrics)
        : blocks_{blocks},
          predicateEvaluator_{predicateEvaluator},
          db_{db},
          metrics_{metrics}
    {
    }

    /**
     * Main accessor. Create (lazy) iterator into shared blocks.
     */
    SharedBlockIterator newIterator(qg::BlockId block_id, std::optional<k::DRelId> boundary);

  private:
    friend class SharedBlockIterator;
    using Producer = std::generator<SharedBlockRow>;

    /**
     * (BlockId, DRel) -> entry containing shared state between iterators
     * fields:
     * - `std::vector<SharedBlockRow> rows`
     * - `std::optional<Producer> producer`
     */
    struct Entry
    {
      std::vector<SharedBlockRow> rows;
      std::optional<Producer> producer;
      std::optional<std::ranges::iterator_t<Producer>> iterator;
      bool exhausted = false;
    };

    const std::unordered_map<qg::BlockId, ib::SharedBlockIR, IntStrongHash> &blocks_;
    PredicateEvaluator &predicateEvaluator_;
    k::CachedDatabaseAccess &db_;
    SharedBlockExecutionMetrics &metrics_;

    // TODO: why unique_ptr??
    std::unordered_map<SharedBlockCacheKey, std::unique_ptr<Entry>, SharedBlockCacheKeyHash> entries_;

    std::optional<SharedBlockRow> rowAt(const SharedBlockCacheKey &key, size_t index);

    Entry &getOrCreateEntry(const SharedBlockCacheKey &key);

    /**
     * produce an `SharedBlockRow` in the entry by invoking the `producer` coroutine
     * until the next co_yield
     */
    bool produceOne(Entry &entry);

    Producer makeProducer(qg::BlockId block_id, std::optional<k::DRelId> boundary);

    Producer enumerateBlockDFS(
        const ib::SharedBlockIR &block,

        std::unordered_map<
            ib::SharedBlockIR::EIdx,
            k::DRelId,
            IntStrongHash> &assignment,

        std::unordered_set<
            k::DRelId,
            IntStrongHash> &usedRelationships);

    /**
     * choose next node to maximize
     * - (boundNeighbors, degree)
     * return `nullopt` if none found (which means the assignment is fully matched)
     * 
     * NOTE
     * - the first node will be chosen to minimize degree only
     */
    std::optional<ib::SharedBlockIR::EIdx> chooseNextNode(
        const ib::SharedBlockIR &block,

        const std::unordered_map<
            ib::SharedBlockIR::EIdx,
            k::DRelId,
            IntStrongHash> &assignment) const;
  };

  // ---------------------------------------------------------------------------
  // Top-level lazy executor
  // ---------------------------------------------------------------------------

  class Executor
  {
  public:
    Executor(k::InMemoryDatabase &database, k::DatabaseMetrics &databaseMetrics, ib::BuildResult &buildResult)
        : buildResult_{buildResult},
          database_{database},
          databaseMetrics_{databaseMetrics},
          cachedDatabase_{database, databaseMetrics},
          predicateEvaluator_{
              buildResult.predicateRegistry,
              cachedDatabase_,
              databaseMetrics},
          sharedBlockManager_{
              buildResult.shared_block_registry,
              predicateEvaluator_,
              cachedDatabase_,
              sharedBlockMetrics_}
    {
      queryMetrics_.resize(
          buildResult_.edgeGraphs.size());
    }

    /**
     * Main lazy API.
     *
     * Executor must outlive the returned generator.
     */
    std::generator<Embedding> enumerateQuery(size_t queryIndex);

    /**
     * Convenience function which fully materializes one query.
     */
    QueryExecutionResult executeQuery(size_t queryIndex);

    const QueryExecutionMetrics &queryMetrics(size_t queryIndex) const
    {
      return queryMetrics_.at(queryIndex);
    }

    const SharedBlockExecutionMetrics &sharedBlockMetrics() const
    {
      return sharedBlockMetrics_;
    }

  private:
    ib::BuildResult &buildResult_;

    k::InMemoryDatabase &database_;
    k::DatabaseMetrics &databaseMetrics_;

    k::CachedDatabaseAccess cachedDatabase_;
    PredicateEvaluator predicateEvaluator_;

    SharedBlockExecutionMetrics sharedBlockMetrics_;

    SharedBlockIteratorManager sharedBlockManager_;

    std::vector<QueryExecutionMetrics> queryMetrics_;

    std::generator<Embedding> enumerateOuterDFS(
        const ib::EdgeGraphCollapsed &edgeGraph,
        QueryExecutionMetrics &metrics,
        Embedding &embedding,
        std::unordered_set<
            k::DRelId,
            IntStrongHash> &usedRelationships,
        std::unordered_set<
            ib::EIdxCollapsed,
            IntStrongHash> &processed);

    /**
     * Choose next collapsed edge graph node to match.
     * - if none have been already matched, chooses node with `max(deg(u))`
     * - else, chooses node with `max((deg(u), num bound neighbors(u))`
     *
     * return
     * - `optional<EIdx`: next edge graph node to match
     */
    std::optional<ib::EIdxCollapsed> chooseNextOuterNode(
        const ib::EdgeGraphCollapsed &edgeGraph,
        const Embedding &embedding,
        const std::unordered_set<
            ib::EIdxCollapsed,
            IntStrongHash> &processed) const;

    /**
     * whether `embedding` contains the boundary node required by the shared block
     */
    bool sharedBlockEligible(
        const ib::SharedBlockReference &reference,
        const Embedding &embedding) const;
  };

} // namespace execution