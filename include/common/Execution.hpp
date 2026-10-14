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

  // ---------------------------------------------------------------------------
  // Output embedding
  //
  // This is now query VariableId -> data relationship ID.
  // ---------------------------------------------------------------------------

  struct Embedding
  {
    std::unordered_map<
        qg::VariableId,
        k::DRelId,
        IntStrongHash>
        relationships;
  };

  // ---------------------------------------------------------------------------
  // Shared block rows
  //
  // Context-independent row using SharedBlockIR-local EIdx values.
  // ---------------------------------------------------------------------------

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

    bool operator==(
        const SharedBlockCacheKey &) const = default;
  };

  struct SharedBlockCacheKeyHash
  {
    size_t operator()(
        const SharedBlockCacheKey &key) const noexcept
    {
      size_t h =
          IntStrongHash{}(key.block_id);

      common::hashCombine(
          h,
          key.boundary.has_value());

      if (key.boundary)
      {
        common::hashCombine(
            h,
            key.boundary->val);
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
    std::unordered_map<
        qg::BlockId,
        std::vector<uint64_t>,
        IntStrongHash>
        recursiveCallsAtDepth;
  };

  struct QueryExecutionMetrics
  {
    // Outer collapsed-edge-graph recursion.
    std::vector<uint64_t>
        recursiveCallsAtDepth;

    uint64_t completeMatches = 0;

    uint64_t regularCandidatesVisited = 0;
    uint64_t regularRejectedByUniqueness = 0;
    uint64_t regularRejectedByAdjacency = 0;
    uint64_t regularRejectedByPredicate = 0;

    uint64_t sharedBlockRowsVisited = 0;
    uint64_t sharedBlockRowsRejectedByOuterUniqueness = 0;

    // Structural order chosen on the first search branch.
    std::vector<ib::collapsed::EIdx>
        matchingOrder;
  };

  struct QueryExecutionResult
  {
    std::vector<Embedding> matches;
    QueryExecutionMetrics metrics;
  };

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

    SharedBlockIterator(
        SharedBlockIteratorManager *manager,
        SharedBlockCacheKey key)
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

    SharedBlockIterator newIterator(
        qg::BlockId block_id,
        std::optional<k::DRelId> boundary);

  private:
    friend class SharedBlockIterator;

    using LocalAssignment =
        std::unordered_map<
            ib::SharedBlockIR::EIdx,
            k::DRelId,
            IntStrongHash>;

    using Producer = std::generator<SharedBlockRow>;
    using ProducerIterator = Producer::iterator;

    struct Entry
    {
      std::vector<SharedBlockRow> rows;

      std::optional<Producer> producer;
      std::optional<ProducerIterator> iterator;

      bool started = false;
      bool exhausted = false;
    };

    struct BoundConstraint
    {
      common::Adjacency adjacency;

      // DRel assigned to the other query relationship.
      k::DRelId otherDRel;
    };

    const std::unordered_map<
        qg::BlockId,
        ib::SharedBlockIR,
        IntStrongHash> &blocks_;

    PredicateEvaluator &predicateEvaluator_;
    k::CachedDatabaseAccess &db_;

    SharedBlockExecutionMetrics &metrics_;

    std::unordered_map<
        SharedBlockCacheKey,
        std::unique_ptr<Entry>,
        SharedBlockCacheKeyHash>
        entries_;

    std::optional<SharedBlockRow> rowAt(
        const SharedBlockCacheKey &key,
        size_t index);

    Entry &getOrCreateEntry(
        const SharedBlockCacheKey &key);

    bool produceOne(
        Entry &entry);

    Producer makeProducer(
        qg::BlockId block_id,
        std::optional<k::DRelId> boundary);

    Producer enumerateBlockDFS(
        const ib::SharedBlockIR &block,
        LocalAssignment &assignment,
        std::unordered_set<
            k::DRelId,
            IntStrongHash> &usedRelationships);

    std::optional<ib::SharedBlockIR::EIdx>
    chooseNextNode(
        const ib::SharedBlockIR &block,
        const LocalAssignment &assignment) const;

    std::vector<BoundConstraint>
    boundConstraints(
        const ib::SharedBlockIR &block,
        ib::SharedBlockIR::EIdx current,
        const LocalAssignment &assignment);

    k::RelationshipCursor candidateCursor(
        const std::vector<BoundConstraint> &constraints);

    bool satisfiesConstraints(
        k::DRelId candidate,
        const std::vector<BoundConstraint> &constraints);
  };

  // ---------------------------------------------------------------------------
  // Top-level lazy executor
  // ---------------------------------------------------------------------------

  class Executor
  {
  public:
    Executor(
        k::InMemoryDatabase &database,
        k::DatabaseMetrics &databaseMetrics,
        ib::BuildResult &buildResult)
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
    std::generator<Embedding> cenumerateQuery(size_t queryIndex);

    /**
     * Convenience function which fully materializes one query.
     */
    QueryExecutionResult
    executeQuery(size_t queryIndex);

    const QueryExecutionMetrics &
    queryMetrics(size_t queryIndex) const
    {
      return queryMetrics_.at(queryIndex);
    }

    const SharedBlockExecutionMetrics &
    sharedBlockMetrics() const
    {
      return sharedBlockMetrics_;
    }

  private:
    struct BoundConstraint
    {
      common::Adjacency adjacency;
      k::DRelId otherDRel;
    };

    ib::BuildResult &buildResult_;

    k::InMemoryDatabase &database_;
    k::DatabaseMetrics &databaseMetrics_;

    k::CachedDatabaseAccess cachedDatabase_;
    PredicateEvaluator predicateEvaluator_;

    SharedBlockExecutionMetrics
        sharedBlockMetrics_;

    SharedBlockIteratorManager
        sharedBlockManager_;

    std::vector<QueryExecutionMetrics>
        queryMetrics_;

    std::generator<Embedding>
    enumerateOuterDFS(
        const ib::collapsed::EdgeGraphCollapsed &edgeGraph,
        QueryExecutionMetrics &metrics,
        Embedding &embedding,
        std::unordered_set<
            k::DRelId,
            IntStrongHash> &usedRelationships,
        std::unordered_set<
            ib::collapsed::EIdx,
            IntStrongHash> &processed);

    std::optional<ib::collapsed::EIdx>
    chooseNextOuterNode(
        const ib::collapsed::EdgeGraphCollapsed &edgeGraph,
        const Embedding &embedding,
        const std::unordered_set<
            ib::collapsed::EIdx,
            IntStrongHash> &processed) const;

    bool sharedBlockEligible(
        const ib::collapsed::SharedBlockReference &reference,
        const Embedding &embedding) const;

    std::vector<BoundConstraint>
    regularBoundConstraints(
        const ib::collapsed::EdgeGraphCollapsed &edgeGraph,
        ib::collapsed::EIdx current,
        const Embedding &embedding,
        const std::unordered_set<
            ib::collapsed::EIdx,
            IntStrongHash> &processed);

    k::RelationshipCursor regularCandidateCursor(
        const std::vector<BoundConstraint> &constraints);

    bool satisfiesConstraints(
        k::DRelId candidate,
        const std::vector<BoundConstraint> &constraints);

    static bool isOrdinaryAdjacency(
        common::Adjacency adjacency)
    {
      return adjacency !=
             common::Adjacency::BoundaryToSB;
    }

    static common::Direction
    directionForEndpoint(
        common::Endpoint endpoint)
    {
      return endpoint == common::Endpoint::Start
                 ? common::Direction::Outgoing
                 : common::Direction::Incoming;
    }
  };

} // namespace execution