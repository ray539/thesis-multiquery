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

  enum class NeighborhoodCondition
  {
    Existence,
    ExistenceDegree,
    BipartiteMatching,
  };

  struct PredicateEvaluatorConfig
  {
    NeighborhoodCondition neighborhoodCondition =
        NeighborhoodCondition::BipartiteMatching;
  };

  // ---------------------------------------------------------------------------
  // Predicate evaluator
  // ---------------------------------------------------------------------------

  struct PredicateEvalKey
  {
    k::DRelId rel;
    ib::PredicateId predicate;

    bool operator==(const PredicateEvalKey &) const = default;
  };
} // namespace execution

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
        k::DatabaseMetrics &metrics,
        PredicateEvaluatorConfig config = {})
        : registry_{registry},
          db_{db},
          metrics_{metrics},
          config_{config}
    {
    }

    bool evaluate(
        ib::PredicateId predicate,
        k::DRelId drel);

    const PredicateEvaluatorConfig &config() const
    {
      return config_;
    }

  private:
    ib::PredicateRegistry &registry_;
    k::CachedDatabaseAccess &db_;
    k::DatabaseMetrics &metrics_;

    // Config is deliberately immutable after construction.
    //
    // Therefore the existing cache key:
    //
    //   (PredicateId, DRelId)
    //
    // remains valid.
    const PredicateEvaluatorConfig config_;

    std::unordered_map<
        PredicateEvalKey,
        bool>
        cache_;

    bool evaluateBase(
        const ib::BaseRelPredicate &predicate,
        k::DRelId drel);

    bool evaluateOther(
        const ib::OtherRelPredicate &predicate,
        k::DRelId drel);

    bool evaluateExistence(
        const ib::OtherRelPredicate &predicate,
        k::DRelId drel);

    bool evaluateExistenceDegree(
        const ib::OtherRelPredicate &predicate,
        k::DRelId drel);

    bool evaluateBipartiteMatching(
        const ib::OtherRelPredicate &predicate,
        k::DRelId drel);

    bool hasMatchingNeighbor(
        k::DRelId drel,
        const ib::AdjacencyRequirement &req);

    std::vector<k::DRelId> matchingNeighbors(
        k::DRelId drel,
        const ib::AdjacencyRequirement &req);

    /**
     * Wrapper over db_.nodeDegree to handle cases like
     * - `<-[me]-()->`: here, when you do `nodeDegree(my start node)`, the degree includes myself as well
     */
    size_t availableNeighborDegree(
        k::DRelId drel,
        common::Adjacency adjacency);

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

    std::optional<size_t> indexWhichArgmins(
        const std::vector<size_t> &indices,
        std::function<bool(size_t)> &&filter,
        std::function<long double(size_t)> &&scorer);

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
  // candidate sets
  // ---------------------------------------------------------------------------

  /**
   * fields
   * - `candidates: vector<DRelId>`
   * - `membership: set<DRelId>` for membership testing
   */
  struct CandidateSet
  {
    std::vector<kernel::DRelId> candidates;
    std::unordered_set<
        kernel::DRelId,
        IntStrongHash>
        membership;

    void addDrel(k::DRelId drel)
    {
      if (membership.contains(drel))
        return;
      candidates.push_back(drel);
      membership.insert(drel);
    }
  };

  /**
   * fields:
   * - `sourcePredicate:PredId`
   * - `targetPredicate:PredId`
   * - `adjacency:Adjacency`
   */
  struct CandidateAdjacencyKey
  {
    ir_builder::PredicateId sourcePredicate;
    ir_builder::PredicateId targetPredicate;
    common::Adjacency adjacency;
    bool operator==(const CandidateAdjacencyKey &) const = default;
  };

  struct CandidateAdjacencyKeyHash
  {
    size_t operator()(const CandidateAdjacencyKey &key) const noexcept
    {
      size_t h = IntStrongHash{}(key.sourcePredicate);
      common::hashCombine(h, key.targetPredicate.val);
      common::hashCombine(h, static_cast<int>(key.adjacency));
      return h;
    }
  };

  /**
   * strong type for `RelId -> vector<RelId>`
   */
  struct CandidateAdjacency
  {
    std::unordered_map<
        k::DRelId,
        std::vector<k::DRelId>,
        IntStrongHash>
        neighbors;
  };

  /**
   * fields
   * - `predicateOfRelationship: GlobalRelId -> PredicateId`
   */
  struct QueryCandidateSpace
  {
    std::unordered_map<
        ib::GlobalRelId,
        ib::PredicateId,
        IntStrongHash>
        predicateOfRelationship;
  };

  class CandidateSpace
  {
  public:
    /**
     * candidates for a given predicate
     */
    const std::vector<k::DRelId> &candidates(ib::PredicateId predicate) const;

    /**
     * Return candidates in targetPredicate adjacent to source according to
     * the specified adjacency type.
     *
     * source is assumed to belong to C(sourcePredicate).
     */
    const std::vector<k::DRelId> &adj(
        ib::PredicateId sourcePredicate,
        k::DRelId source,
        ib::PredicateId targetPredicate,
        common::Adjacency adjacency) const;

    const QueryCandidateSpace &query(size_t queryIndex) const;

  private:
    friend class CandidateSpaceBuilder;

    /**
     * candidate set nodes
     * - `C(pi)`
     */
    std::unordered_map<
        ib::PredicateId,
        CandidateSet,
        IntStrongHash>
        candidatesBySignature_;

    /**
     * data used to answer `adj` function calls
     * (p0:PredId, p1:PredId, StartEnd) -> DRelId -> vector<DRelId>
     */
    std::unordered_map<
        CandidateAdjacencyKey,
        CandidateAdjacency,
        CandidateAdjacencyKeyHash>
        adjacency_;

    std::vector<QueryCandidateSpace> queries_;
  };

  class CandidateSpaceBuilder
  {
  public:
    CandidateSpaceBuilder(
        const ib::BuildResult &buildResult,
        k::CachedDatabaseAccess &database,
        PredicateEvaluator &predicateEvaluator)
        : buildResult_{buildResult},
          database_{database},
          predicateEvaluator_{predicateEvaluator}
    {
    }

    /**
     * Build a CSS using signatures from the requested hierarchical level.
     *
     * Example:
     *   build(0) -> base predicates only
     *   build(1) -> level-1 hierarchical signatures
     *   ...
     */
    CandidateSpace build(size_t signatureLevel);

  private:
    const ib::BuildResult &buildResult_;
    k::CachedDatabaseAccess &database_;
    PredicateEvaluator &predicateEvaluator_;

    using PredicateSet =
        std::unordered_set<
            ib::PredicateId,
            IntStrongHash>;

    using AdjacencyRequirementSet =
        std::unordered_set<
            CandidateAdjacencyKey,
            CandidateAdjacencyKeyHash>;

    ib::PredicateId signatureFor(ib::GlobalRelId relationship, size_t signatureLevel) const;

    /**
     * - modify each QueryCandidateSpace (relId -> predId) mapping
     * return
     * - `requiredPredicates`: set of predicate ids found in at least one edge graph node
     * - `requiredAdjacencies`: list of CandidiateAdjacency pairs
     */
    std::pair<PredicateSet, AdjacencyRequirementSet> collectRequirements(
        CandidateSpace &space,
        size_t signatureLevel) const;

    /**
     * essentially performs
     * ```
     * for drel in db.allRelationships
     *   for pred in requiredPredicates
     *     if evaluate(drel, pred)
     *       C[pred].add(drel)
     * ```
     *
     */
    void buildCandidateSets(
        CandidateSpace &space,
        const PredicateSet &requiredPredicates);

    /**
     * see `buildCandidateAdjacency`
     */
    void buildCandidateAdjacencies(
        CandidateSpace &space,
        const AdjacencyRequirementSet &requiredAdjacencies);

    /**
     * for each candidate adjacency `(p0:PredId, p1:PredId, type)`,
     * - look through all drels in `C[p0]`, and create it's
     *   adjacency list in `C[p1]`
     */
    void buildCandidateAdjacency(
        CandidateSpace &space,
        const CandidateAdjacencyKey &key);

    /**
     * Build Adj(p0, p1, adjacency, source) by traversing the database adjacency
     * cursor and checking membership in C(p1).
     * - `source:Drel, adjacency`: describes the node to get the adjacent relationships from
     * - `targetCandidates:CandidateSet`: where each target neighbor has to belong to
     */
    void buildAdjacencyUsingCursor(
        k::DRelId source,
        const CandidateSet &targetCandidates,
        common::Adjacency adjacency,
        std::vector<k::DRelId> &output);

    /**
     * Build Adj(p0, p1, adjacency, source) by traversing C(p1) and checking
     * endpoint equality.
     * - `source:Drel, adjacency`: describes the node to get the adjacent relationships from
     * - `targetCandidates:CandidateSet`: where each target neighbor has to belong to
     */
    void buildAdjacencyUsingCandidateSet(
        k::DRelId source,
        const CandidateSet &targetCandidates,
        common::Adjacency adjacency,
        std::vector<k::DRelId> &output);
  };

  // ---------------------------------------------------------------------------
  // eager executor
  // ---------------------------------------------------------------------------

  enum class EagerOrderingStrategy
  {
    SmallestSetFirst,
    ConnectedGreedy,
    LazyGreedy,
  };

  struct EagerExecutorOptions
  {
    size_t signatureLevel = 3;

    EagerOrderingStrategy ordering =
        EagerOrderingStrategy::ConnectedGreedy;
  };

  struct EagerExecutionMetrics
  {
    std::vector<uint64_t> recursiveCallsAtDepth;

    uint64_t candidatesVisited = 0;
    uint64_t rejectedByUniqueness = 0;
    uint64_t rejectedByAdjacency = 0;

    uint64_t completeMatches = 0;

    /**
     * Query relationship matching order.
     *
     * The ordering strategies implemented here depend only on the query graph,
     * candidate set sizes and the processed query nodes, so all branches use
     * the same order.
     */
    std::vector<ib::EIdxCollapsed> matchingOrder;
  };

  class EagerExecutor
  {
  public:
    /**
     * Builds the CandidateSpace immediately using options.signatureLevel.
     *
     * PredicateEvaluator is supplied externally so its filtering semantics can
     * be configured independently of the eager executor.
     */
    EagerExecutor(
        const ib::BuildResult &buildResult,
        k::CachedDatabaseAccess &database,
        PredicateEvaluator &predicateEvaluator,
        EagerExecutorOptions options = {});

    /**
     * Materialize all embeddings for one query.
     */
    std::vector<Embedding> executeQuery(size_t queryIndex);

    const CandidateSpace &candidateSpace() const
    {
      return candidateSpace_;
    }

    const EagerExecutionMetrics &metrics() const
    {
      return metrics_;
    }

  private:
    /**
     * fields
     * - `ib::EIdxCollapsed boundNode`: query node of this bound neighbor
     * - `ib::PredicateId boundNodePredicate`: predicate this bound neighbor satisfies
     * - `k::DrelId boundNodeDrel`: drel of this bound neighbor
     * - `common::Adjacency qnodeToBoundNode`: adjacency from NEWLY MATCHED qnode to this bound neighbor
     */
    struct BoundNeighbor
    {
      ib::EIdxCollapsed boundNode;
      ib::PredicateId boundNodePredicate;
      k::DRelId boundNodeDrel;
      /**
       * Adjacency from the current qnode being matched to this bound neighbor
       */
      common::Adjacency qnodeToBoundNode;
    };

    const ib::BuildResult &buildResult_;
    k::CachedDatabaseAccess &database_;
    PredicateEvaluator &predicateEvaluator_;

    EagerExecutorOptions options_;

    CandidateSpace candidateSpace_;

    EagerExecutionMetrics metrics_;

    void backtrack(
        size_t queryIndex,
        const ib::EdgeGraphCollapsed &edgeGraph,
        Embedding &embedding,
        std::unordered_set<k::DRelId, IntStrongHash> &usedRelationships,
        std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &processed,
        std::vector<Embedding> &output);

    std::optional<ib::EIdxCollapsed> chooseNextNode(
        size_t queryIndex,
        const ib::EdgeGraphCollapsed &edgeGraph,
        const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &processed) const;

    std::optional<ib::EIdxCollapsed> chooseSmallestSetFirst(
        size_t queryIndex,
        const ib::EdgeGraphCollapsed &edgeGraph,
        const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &processed) const;

    std::optional<ib::EIdxCollapsed> chooseConnectedGreedy(
        size_t queryIndex,
        const ib::EdgeGraphCollapsed &edgeGraph,
        const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &processed) const;

    std::optional<ib::EIdxCollapsed> chooseLazyGreedy(
        const ib::EdgeGraphCollapsed &edgeGraph,
        const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &processed) const;

    /**
     * Query relationship degree: number of distinct adjacent query relationship
     * nodes.
     */
    size_t degree(
        const ib::EdgeGraphCollapsed &edgeGraph,
        ib::EIdxCollapsed node) const;

    /**
     * Number of distinct adjacent query relationship nodes already bound.
     */
    size_t boundDegree(
        const ib::EdgeGraphCollapsed &edgeGraph,
        ib::EIdxCollapsed node,
        const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &processed) const;

    size_t candidateSetSize(
        size_t queryIndex,
        const ib::EdgeGraphCollapsed &edgeGraph,
        ib::EIdxCollapsed node) const;

    ib::PatternRelationshipIR relationshipAt(
        const ib::EdgeGraphCollapsed &edgeGraph,
        ib::EIdxCollapsed node) const;

    ib::PredicateId predicateOf(
        size_t queryIndex,
        const ib::PatternRelationshipIR &relationship) const;

    /**
     * Bound query neighbors of current.
     */
    std::vector<BoundNeighbor> boundNeighbors(
        size_t queryIndex,
        const ib::EdgeGraphCollapsed &edgeGraph,
        ib::EIdxCollapsed current,
        const Embedding &embedding,
        const std::unordered_set<ib::EIdxCollapsed, IntStrongHash> &processed) const;

    /**
     * Check all already-bound query adjacency constraints.
     */
    bool satisfiesBoundNeighbors(
        k::DRelId candidate,
        const std::vector<BoundNeighbor> &neighbors);

    /**
     * Pick the smallest CSS adjacency list among all already-bound neighbors.
     *
     * Returns nullptr if current has no bound neighbors, meaning the executor
     * should iterate over the whole C(predicate).
     */
    const std::vector<k::DRelId> *chooseExpansionCandidates(
        ib::PredicateId currentPredicate,
        const std::vector<BoundNeighbor> &neighbors) const;
  };

  // ---------------------------------------------------------------------------
  // lazy executor
  // ---------------------------------------------------------------------------

  class LazyExecutorWithSB
  {
  public:
    LazyExecutorWithSB(k::InMemoryDatabase &database, k::DatabaseMetrics &databaseMetrics, ib::BuildResult &buildResult)
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

    /**
     * main enumerator function
     */
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