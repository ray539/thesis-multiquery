#include <common/Kernel.hpp>
#include <common/IRBuilder.hpp>
#include <common/Execution.hpp>
#include <common/Evaluation.hpp>

namespace evaluation
{

    void cdg_lazy()
    {
        namespace k = kernel;
        namespace ib = ir_builder;
        namespace ex = execution;
        namespace e = evaluation;

        const size_t max_layer = 3;
        auto runExperiment = [&](const std::string &name, size_t LIMIT)
        {
            // ------------- KERNEL LAYER AND SAMPLING -----------
            k::DatabaseMetrics metrics;
            k::InMemoryDatabase db_(metrics);
            k::CachedDatabaseAccess db(db_, metrics);

            e::ChaoticDataGraphGenConfig genConfig{
                .nodeCount = 100'000,
                .relationshipCount = 400'000,
                .relationshipTypes = {"A"},
                .seed = 1};
            e::populateChaoticDataGraph(db_, genConfig);

            e::QueryGraphSamplerConfig qconfig{
                .relationshipCount = 10,
                .maxAttempts = 100,
                .seed = 1,
            };

            auto query_graphs =
                e::getQueryGraphsFrom(
                    db_,
                    qconfig,
                    1);

            std::cout << name << ":" << std::endl;
            ib::IRBuilder builder;
            ib::BuildResult buildResult = builder.run(query_graphs, max_layer);
            ex::LazyExecutorWithSB executor(
                db_, metrics, buildResult,
                ex::PredicateEvaluatorConfig{.neighborhoodCondition = ex::NeighborhoodCondition::ExistenceDegree},
                LIMIT);

            auto start = std::chrono::steady_clock::now();
            auto exResult = executor.executeQuery(0);
            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            std::cout
                << "enumeration time: "
                << elapsed.count()
                << " ms"
                << std::endl;

            std::cout << ex::to_string(exResult.metrics) << std::endl;
            std::cout << "database metrics" << std::endl;
            std::cout << k::to_string(metrics) << std::endl;
        };
        runExperiment("lazy executor, limit = 8000000", 8000'000);
    }

    void cdg_precomp()
    {
        namespace k = kernel;
        namespace ib = ir_builder;
        namespace ex = execution;
        namespace e = evaluation;

        const size_t max_layer = 3;
        auto runExperiment = [&](const std::string &name, ex::EagerOrderingStrategy eos, size_t LIMIT)
        {
            // ------------- KERNEL LAYER AND SAMPLING -----------
            k::DatabaseMetrics metrics;
            k::InMemoryDatabase db_(metrics);
            k::CachedDatabaseAccess db(db_, metrics);

            e::ChaoticDataGraphGenConfig genConfig{
                .nodeCount = 100'000,
                .relationshipCount = 400'000,
                .relationshipTypes = {"A"},
                .seed = 1};
            e::populateChaoticDataGraph(db_, genConfig);

            e::QueryGraphSamplerConfig qconfig{
                .relationshipCount = 10,
                .maxAttempts = 100,
                .seed = 1,
            };

            auto query_graphs =
                e::getQueryGraphsFrom(
                    db_,
                    qconfig,
                    1);

            std::cout << name << ":" << std::endl;
            ib::IRBuilder builder;
            ib::BuildResult buildResult = builder.run(query_graphs, max_layer);

            ex::PredicateEvaluator predEvaluator(
                buildResult.predicateRegistry,
                db,
                metrics,
                ex::PredicateEvaluatorConfig{
                    .neighborhoodCondition = ex::NeighborhoodCondition::ExistenceDegree});

            auto start = std::chrono::steady_clock::now();
            ex::EagerExecutor executor(buildResult, db, predEvaluator, ex::EagerExecutorOptions{.signatureLevel = max_layer, .ordering = eos},
                                       LIMIT);

            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "done building scs, time: " << elapsed.count() << " ms " << std::endl;

            start = std::chrono::steady_clock::now();
            auto embeddings = executor.executeQuery(0);
            end = std::chrono::steady_clock::now();
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            std::cout
                << "enumeration time: "
                << elapsed.count()
                << " ms"
                << std::endl;

            auto execution_metrics = executor.metrics();
            std::cout << "execution metrics: " << std::endl;
            std::cout << ex::to_string(execution_metrics) << std::endl;
            std::cout << "database metrics" << std::endl;
            std::cout << k::to_string(metrics) << std::endl;
        };
        runExperiment("precomp, limit = 8000000", ex::EagerOrderingStrategy::ConnectedGreedy, 8000000);
    }

    void noise_ddg_eager_lazy()
    {
        namespace k = kernel;
        namespace ib = ir_builder;
        namespace ex = execution;
        namespace e = evaluation;

        using Clock = std::chrono::steady_clock;

        constexpr size_t maxLayer = 3;

        const ex::PredicateEvaluatorConfig predicateConfig{
            .neighborhoodCondition =
                ex::NeighborhoodCondition::ExistenceDegree,
        };

        // ---------------------------------------------------------------------------
        // Generate workload
        // ---------------------------------------------------------------------------

        k::DatabaseMetrics metrics;
        k::InMemoryDatabase database(metrics);

        e::NoisyDatagraphGenConfig config{
            .nodeCount = 100'000,

            .centralCandidateCount = 10'000,

            .level1Count = 1'000,
            .level2Count = 100,
            .level3Count = 2,

            .noiseOutgoingPerEndpoint = 32,
            .backgroundNoiseRelationshipCount = 100'000,

            .relationshipTypes = {
                "A",
                "B",
                "C",
                "D",
                "E",
                "F",
                "G",
                "H",
                "NOISE",
            },

            .queryCount = 100,

            .seed = 1,
        };

        auto workload = e::populatePlantedMotifGraph(database, config);

        auto &queryGraphs = workload.queryGraphs;

        std::cout
            << "queries: "
            << queryGraphs.size()
            << "\n";

        std::cout
            << "planted level-3 motifs: "
            << workload.fullMotifCenters.size()
            << "\n";

        // ---------------------------------------------------------------------------
        // Compile once.
        //
        // Both execution strategies operate on the exact same IR.
        // ---------------------------------------------------------------------------

        ib::IRBuilder builder;
        ib::BuildResult buildResult =
            builder.run(
                queryGraphs,
                maxLayer);

        // Ignore graph generation / compilation metrics.
        metrics = {};

        // ===========================================================================
        // EAGER
        // ===========================================================================

        k::CachedDatabaseAccess eagerDatabase(
            database,
            metrics);

        ex::PredicateEvaluator eagerPredicateEvaluator(
            buildResult.predicateRegistry,
            eagerDatabase,
            metrics,
            predicateConfig);

        /**
         * EagerExecutor constructs the entire shared candidate space in its
         * constructor, so time construction separately.
         */
        auto eagerPrecomputeStart = Clock::now();
        ex::EagerExecutor eagerExecutor(
            buildResult,
            eagerDatabase,
            eagerPredicateEvaluator,
            ex::EagerExecutorOptions{
                .signatureLevel = maxLayer,
                .ordering =
                    ex::EagerOrderingStrategy::LazyGreedy,
            });

        auto eagerPrecomputeEnd = Clock::now();

        uint64_t eagerMatches = 0;
        uint64_t eagerCandidatesVisited = 0;
        uint64_t eagerRecursiveCalls = 0;

        auto eagerEnumerationStart = Clock::now();

        for (size_t queryIndex = 0; queryIndex < queryGraphs.size();++queryIndex)
        {
            auto matches = eagerExecutor.executeQuery(  queryIndex);
            eagerMatches += matches.size();
            const auto &queryMetrics = eagerExecutor.metrics();
            eagerCandidatesVisited += queryMetrics.candidatesVisited;
            eagerRecursiveCalls +=
                std::accumulate(
                    queryMetrics.recursiveCallsAtDepth.begin(),
                    queryMetrics.recursiveCallsAtDepth.end(),
                    uint64_t{0});
        }

        auto eagerEnumerationEnd = Clock::now();

        // ===========================================================================
        // LAZY
        // ===========================================================================

        /**
         * Reset counters.
         *
         * LazyExecutorWithSB creates its own CachedDatabaseAccess, so its caches
         * begin empty even though the underlying InMemoryDatabase is shared.
         */
        metrics = {};
        ex::LazyExecutorWithSB lazyExecutor(
            database,
            metrics,
            buildResult,
            predicateConfig,
            1000'000'000);

        uint64_t lazyMatches = 0;
        uint64_t lazyCandidatesVisited = 0;
        uint64_t lazyRecursiveCalls = 0;

        auto lazyStart = Clock::now();
        for (size_t queryIndex = 0; queryIndex < queryGraphs.size(); ++queryIndex)
        {
            ex::QueryExecutionResult result = lazyExecutor.executeQuery(queryIndex);
            lazyMatches += result.matches.size();
            lazyCandidatesVisited += result.metrics.regularCandidatesVisited;
            lazyRecursiveCalls +=
                std::accumulate(
                    result.metrics.recursiveCallsAtDepth.begin(),
                    result.metrics.recursiveCallsAtDepth.end(),
                    uint64_t{0});
        }
        auto lazyEnd = Clock::now();
        // ===========================================================================
        // Results
        // ===========================================================================

        auto ms = [](auto duration)
        {
            return std::chrono::duration<double, std::milli>(duration).count();
        };

        double eagerPrecomputeMs = ms(eagerPrecomputeEnd - eagerPrecomputeStart);
        double eagerEnumerationMs = ms(eagerEnumerationEnd - eagerEnumerationStart);
        double eagerTotalMs = eagerPrecomputeMs + eagerEnumerationMs;
        double lazyMs = ms(lazyEnd - lazyStart);

        std::cout << "\n";
        std::cout << "================ comparison ================\n";

        std::cout << "\nEAGER\n";
        std::cout
            << "  precomputation:       "
            << eagerPrecomputeMs
            << " ms\n";

        std::cout
            << "  enumeration:          "
            << eagerEnumerationMs
            << " ms\n";

        std::cout
            << "  total:                "
            << eagerTotalMs
            << " ms\n";

        std::cout
            << "  candidates visited:   "
            << eagerCandidatesVisited
            << "\n";

        std::cout
            << "  recursive calls:      "
            << eagerRecursiveCalls
            << "\n";

        std::cout
            << "  matches:              "
            << eagerMatches
            << "\n";

        std::cout << "\nLAZY\n";

        std::cout
            << "  enumeration:          "
            << lazyMs
            << " ms\n";

        std::cout
            << "  candidates visited:   "
            << lazyCandidatesVisited
            << "\n";

        std::cout
            << "  recursive calls:      "
            << lazyRecursiveCalls
            << "\n";

        std::cout
            << "  matches:              "
            << lazyMatches
            << "\n";

        std::cout << "\nRATIOS\n";

        if (lazyCandidatesVisited != 0)
        {
            std::cout
                << "  eager/lazy candidates: "
                << static_cast<double>(eagerCandidatesVisited) /
                       static_cast<double>(lazyCandidatesVisited)
                << "\n";
        }

        if (lazyMs != 0.0)
        {
            std::cout
                << "  eager total/lazy time: "
                << eagerTotalMs / lazyMs
                << "\n";

            std::cout
                << "  eager enum/lazy time:  "
                << eagerEnumerationMs / lazyMs
                << "\n";
        }

        std::cout << "\nCORRECTNESS\n";

        std::cout
            << "  match counts equal:   "
            << std::boolalpha
            << (eagerMatches == lazyMatches)
            << "\n";

        if (eagerMatches != lazyMatches)
        {
            std::cout
                << "WARNING: eager and lazy produced different match counts\n";
        }
    }

} // namespace evaluation