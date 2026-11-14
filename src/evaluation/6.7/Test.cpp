#include <common/Kernel.hpp>
#include <common/IRBuilder.hpp>
#include <common/Execution.hpp>
#include <common/Evaluation.hpp>

namespace evaluation
{
    void shared_block_eval()
    {
        namespace k = kernel;
        namespace ib = ir_builder;
        namespace qg = query_graph;
        namespace ex = execution;
        namespace e = evaluation;

        using Clock = std::chrono::steady_clock;

        // ---------------------------------------------------------------------------
        // Generate one common data graph and the two equivalent query graphs.
        // ---------------------------------------------------------------------------

        k::DatabaseMetrics metrics;
        k::InMemoryDatabase database(metrics);
        e::SharedBlockStressConfig config{
            .centralCount = 2,
            .blockCount = 4,
            .embeddingsPerBlock = 5,
            .noiseDegree = 0,
            .seed = 1,
        };

        auto workload = e::populateSharedBlockStressGraph(database, config);
        auto &withoutSharedBlocks = workload.queryGraphs[0];
        auto &withSharedBlocks = workload.queryGraphs[1];

        /**
         * Keep hierarchical filtering disabled for the shared-block ablation.
         *
         * This means both versions primarily differ in whether repeated internal
         * subgraph enumeration can be cached.
         */
        constexpr size_t maxLayer = 0;

        const ex::PredicateEvaluatorConfig predicateConfig{
            .neighborhoodCondition =
                ex::NeighborhoodCondition::ExistenceDegree,
        };

        // ---------------------------------------------------------------------------
        // Result summary for one run.
        // ---------------------------------------------------------------------------

        struct RunResult
        {
            double elapsedMs = 0.0;

            uint64_t matches = 0;

            uint64_t outerCandidatesVisited = 0;
            uint64_t sharedBlockCandidatesVisited = 0;
            uint64_t totalCandidatesVisited = 0;

            uint64_t outerRecursiveCalls = 0;
            uint64_t sharedBlockRecursiveCalls = 0;
            uint64_t totalRecursiveCalls = 0;

            uint64_t sharedBlockRowsVisited = 0;
            uint64_t sharedBlockRowsRejectedByOuterUniqueness = 0;

            ex::SharedBlockExecutionMetrics sharedBlockMetrics;
        };

        auto sumVector = [](const std::vector<uint64_t> &values)
        {
            return std::accumulate(values.begin(), values.end(), uint64_t{0});
        };

        auto sumSharedBlockRecursiveCalls = [&](const ex::SharedBlockExecutionMetrics &sbMetrics)
        {
            uint64_t total = 0;
            for (const auto &[blockId, calls] : sbMetrics.recursiveCallsAtDepth)
            {
                (void)blockId;
                total += sumVector(calls);
            }
            return total;
        };

        // ---------------------------------------------------------------------------
        // Execute one query.
        //
        // Important: use enumerateQuery(), not executeQuery().
        //
        // The stress workload can produce millions of embeddings, and there is no
        // reason to keep them all in memory just to compare execution behaviour.
        // ---------------------------------------------------------------------------

        auto run = [&](const qg::QueryGraph &query) -> RunResult
        {
            /**
             * Compile this query independently.
             *
             * This also makes each executor see exactly one query at index 0.
             */
            std::vector<qg::QueryGraph> queryGraphs{query};

            ib::IRBuilder builder;
            ib::BuildResult buildResult = builder.run(queryGraphs, maxLayer);
            /**
             * Discard metrics caused by graph generation / compilation.
             *
             * InMemoryDatabase stores a reference to this same metrics object.
             */
            metrics = {};

            ex::LazyExecutorWithSB executor(
                database,
                metrics,
                buildResult,
                predicateConfig,
                5000'000);

            uint64_t matchCount = 0;
            auto start = Clock::now();
            {
                for (const auto &embedding : executor.enumerateQuery(0))
                {
                    (void)embedding;
                    ++matchCount;
                }
            }

            auto end = Clock::now();
            const ex::QueryExecutionMetrics &queryMetrics = executor.queryMetrics(0);

            const ex::SharedBlockExecutionMetrics &sbMetrics = executor.sharedBlockMetrics();

            RunResult result;

            result.elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
            result.matches = matchCount;
            result.outerCandidatesVisited = queryMetrics.regularCandidatesVisited;
            result.sharedBlockCandidatesVisited = sbMetrics.candidateRelationshipsVisited;
            result.totalCandidatesVisited = result.outerCandidatesVisited + result.sharedBlockCandidatesVisited;

            result.outerRecursiveCalls = sumVector(queryMetrics.recursiveCallsAtDepth);

            result.sharedBlockRecursiveCalls = sumSharedBlockRecursiveCalls(sbMetrics);

            result.totalRecursiveCalls =
                result.outerRecursiveCalls +
                result.sharedBlockRecursiveCalls;

            result.sharedBlockRowsVisited = queryMetrics.sharedBlockRowsVisited;
            result.sharedBlockRowsRejectedByOuterUniqueness = queryMetrics.sharedBlockRowsRejectedByOuterUniqueness;
            result.sharedBlockMetrics = sbMetrics;
            return result;
        };

        // ---------------------------------------------------------------------------
        // Run independently.
        //
        // A new LazyExecutorWithSB is constructed for each run, so its
        // CachedDatabaseAccess begins with cold caches.
        // ---------------------------------------------------------------------------

        std::cout
            << "Running WITH shared blocks...\n";

        RunResult shared =
            run(withSharedBlocks);

        std::cout
            << "Running WITHOUT shared blocks...\n";

        RunResult normal =
            run(withoutSharedBlocks);

        // ---------------------------------------------------------------------------
        // Printing helpers.
        // ---------------------------------------------------------------------------

        auto printResult =
            [&](const std::string &name,
                const RunResult &result)
        {
            const auto &sb =
                result.sharedBlockMetrics;

            std::cout
                << "\n"
                << name
                << "\n"
                << "--------------------------------------------------\n";

            std::cout
                << "time:                         "
                << std::fixed
                << std::setprecision(3)
                << result.elapsedMs
                << " ms\n";

            std::cout
                << "matches:                      "
                << result.matches
                << "\n";

            std::cout
                << "\nCandidate visits\n";

            std::cout
                << "  outer candidates:            "
                << result.outerCandidatesVisited
                << "\n";

            std::cout
                << "  shared-block candidates:     "
                << result.sharedBlockCandidatesVisited
                << "\n";

            std::cout
                << "  total candidates:            "
                << result.totalCandidatesVisited
                << "\n";

            std::cout
                << "\nRecursive calls\n";

            std::cout
                << "  outer recursive calls:       "
                << result.outerRecursiveCalls
                << "\n";

            std::cout
                << "  shared-block recursive calls:"
                << " "
                << result.sharedBlockRecursiveCalls
                << "\n";

            std::cout
                << "  total recursive calls:       "
                << result.totalRecursiveCalls
                << "\n";

            std::cout
                << "\nShared-block outer usage\n";

            std::cout
                << "  rows visited:                "
                << result.sharedBlockRowsVisited
                << "\n";

            std::cout
                << "  rows rejected by uniqueness: "
                << result.sharedBlockRowsRejectedByOuterUniqueness
                << "\n";

            std::cout
                << "\nShared-block cache\n";

            std::cout
                << "  iterator creations:          "
                << sb.iteratorCreations
                << "\n";

            std::cout
                << "  cache entries created:       "
                << sb.cacheEntriesCreated
                << "\n";

            std::cout
                << "  row requests:                "
                << sb.rowRequests
                << "\n";

            std::cout
                << "  cached rows returned:        "
                << sb.cachedRowsReturned
                << "\n";

            std::cout
                << "  rows produced:               "
                << sb.rowsProduced
                << "\n";

            std::cout
                << "  producer resumptions:        "
                << sb.producerResumptions
                << "\n";

            std::cout
                << "  producer exhaustions:        "
                << sb.producerExhaustions
                << "\n";

            std::cout
                << "\nShared-block DFS rejection\n";

            std::cout
                << "  rejected by uniqueness:      "
                << sb.rejectedByUniqueness
                << "\n";

            std::cout
                << "  rejected by adjacency:       "
                << sb.rejectedByAdjacency
                << "\n";

            std::cout
                << "  rejected by predicate:       "
                << sb.rejectedByPredicate
                << "\n";
        };

        // ---------------------------------------------------------------------------
        // Results.
        // ---------------------------------------------------------------------------

        std::cout
            << "\n\n"
            << "================ SHARED BLOCK EVALUATION ================\n";

        std::cout
            << "centralCount:          "
            << config.centralCount
            << "\n";

        std::cout
            << "blockCount:            "
            << config.blockCount
            << "\n";

        std::cout
            << "embeddingsPerBlock:    "
            << config.embeddingsPerBlock
            << "\n";

        std::cout
            << "noiseDegree:           "
            << config.noiseDegree
            << "\n";

        std::cout
            << "hierarchical level:    "
            << maxLayer
            << "\n";

        printResult(
            "WITHOUT SHARED BLOCKS",
            normal);

        printResult(
            "WITH SHARED BLOCKS",
            shared);

        // ---------------------------------------------------------------------------
        // Comparison.
        // ---------------------------------------------------------------------------

        std::cout
            << "\n"
            << "COMPARISON\n"
            << "--------------------------------------------------\n";

        std::cout
            << "match counts equal:            "
            << std::boolalpha
            << (normal.matches == shared.matches)
            << "\n";

        if (shared.elapsedMs != 0.0)
        {
            std::cout
                << "runtime speedup:               "
                << normal.elapsedMs / shared.elapsedMs
                << "x\n";
        }

        if (normal.totalCandidatesVisited != 0)
        {
            double candidateRatio =
                static_cast<double>(
                    shared.totalCandidatesVisited) /
                static_cast<double>(
                    normal.totalCandidatesVisited);

            std::cout
                << "shared/normal candidate ratio: "
                << candidateRatio
                << "\n";

            std::cout
                << "candidate reduction:           "
                << (1.0 - candidateRatio) * 100.0
                << "%\n";
        }

        if (normal.totalRecursiveCalls != 0)
        {
            double recursiveCallRatio =
                static_cast<double>(
                    shared.totalRecursiveCalls) /
                static_cast<double>(
                    normal.totalRecursiveCalls);

            std::cout
                << "shared/normal DFS call ratio:  "
                << recursiveCallRatio
                << "\n";
        }

        const auto &sb =
            shared.sharedBlockMetrics;

        if (sb.rowRequests != 0)
        {
            double cacheReturnRatio =
                static_cast<double>(
                    sb.cachedRowsReturned) /
                static_cast<double>(
                    sb.rowRequests);

            std::cout
                << "cached-row return ratio:       "
                << cacheReturnRatio * 100.0
                << "%\n";
        }

        if (normal.matches != shared.matches)
        {
            std::cout
                << "\n"
                << "WARNING: shared-block and ordinary execution produced "
                   "different match counts.\n";
        }
    }
} // namespace evaluation