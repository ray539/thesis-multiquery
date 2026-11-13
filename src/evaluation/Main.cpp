#include <common/Kernel.hpp>
#include <common/IRBuilder.hpp>
#include <common/Execution.hpp>
#include <common/Evaluation.hpp>

void simple_test()
{
    namespace k = kernel;
    namespace ib = ir_builder;
    namespace qg = query_graph;
    namespace ex = execution;
    namespace e = evaluation;

    // ------------- KERNEL LAYER AND SAMPLING -----------
    k::DatabaseMetrics metrics;
    k::InMemoryDatabase db_(metrics);
    k::CachedDatabaseAccess db(db_, metrics);

    e::DataGraphFixedDegreeGenConfig genConfig{
        .nodeCount = 10,
        .outgoingDegree = 4,
        .relationshipTypes = {"A", "B", "C", "D"},
        .seed = 1};
    e::populateDataGraphFixedDegree(db_, genConfig);
    e::QueryGraphSamplerConfig qconfig{
        .relationshipCount = 5,
        .maxAttempts = 100,
        .seed = 1};
    auto query_graphs = e::getQueryGraphsFrom(db_, qconfig, 1);
    std::cout << "kernel state: " << std::endl;
    std::cout << k::to_string(db_) << std::endl;
    std::cout << "query graph: " << std::endl;
    std::cout << qg::to_string(query_graphs[0]) << std::endl;
    // -------------- COMPILATION -------------------------
    ib::IRBuilder builder{};
    ib::BuildResult buildResult = builder.run(query_graphs, 0);
    ex::PredicateEvaluator predEvaluator(buildResult.predicateRegistry, db, metrics, ex::PredicateEvaluatorConfig{.neighborhoodCondition = ex::NeighborhoodCondition::BipartiteMatching});
    // --------------- EXECUTION
    ex::EagerExecutor executor(buildResult, db, predEvaluator, ex::EagerExecutorOptions{.signatureLevel = 0});

    auto embeddings = executor.executeQuery(0);
    std::cout << "resultant embeddings: " << std::endl;
    for (auto embedding : embeddings)
    {
        std::cout << ex::to_string(embedding) << std::endl;
    }
    std::cout << "executor metrics: " << std::endl;
    std::cout << ex::to_string(executor.metrics()) << std::endl;

    std::cout << "database metrics: " << std::endl;
    std::cout << k::to_string(db.metrics()) << std::endl;
}

/**
 * keeping
 * - neighbor strategy as `BipartiteMatching`
 * - ordering strategy as `greedy`
 *
 * compare the candidate set sizes for max_layer = 0 ... 3, from the candidate set structure
 */
void candidate_set_evaluation_layers()
{
    namespace k = kernel;
    namespace ib = ir_builder;
    namespace qg = query_graph;
    namespace ex = execution;
    namespace e = evaluation;

    // ------------- KERNEL LAYER AND SAMPLING -----------
    k::DatabaseMetrics metrics;
    k::InMemoryDatabase db_(metrics);
    k::CachedDatabaseAccess db(db_, metrics);

    e::DataGraphFixedDegreeGenConfig genConfig{
        .nodeCount = 100000,
        .outgoingDegree = 4,
        .relationshipTypes = {"A", "B", "C", "D"},
        .seed = 1};
    e::populateDataGraphFixedDegree(db_, genConfig);
    e::QueryGraphSamplerConfig qconfig{
        .relationshipCount = 10,
        .maxAttempts = 100,
        .seed = 1};
    auto query_graphs = e::getQueryGraphsFrom(db_, qconfig, 1);

    // std::cout << to_string(db_) << std::endl;

    for (size_t max_layer = 0; max_layer <= 3; max_layer++)
    {
        // -------------- COMPILATION -------------------------
        std::cout << "max_layer: " << max_layer << std::endl;
        ib::IRBuilder builder;
        ib::BuildResult buildResult = builder.run(query_graphs, max_layer);
        ex::PredicateEvaluator predEvaluator(buildResult.predicateRegistry, db, metrics, ex::PredicateEvaluatorConfig{.neighborhoodCondition = ex::NeighborhoodCondition::BipartiteMatching});
        // std::cout << "build result:" << std::endl;
        // std::cout << to_string(buildResult) << std::endl;
        // -------------- CANDIDATE SPACE BUILDER ---------------
        ex::CandidateSpaceBuilder csb(buildResult, db, predEvaluator);
        auto scs = csb.build(max_layer);

        std::cout << ex::to_string(scs) << std::endl;
    }
}

/**
 * keeping
 * - num_layers = 3
 * - we will try each neighbor condition, also taking note of the time it takes to run
 * - then output results
 */
void hp_neighbor_condition()
{
    namespace k = kernel;
    namespace ib = ir_builder;
    namespace ex = execution;
    namespace e = evaluation;

    // ------------- KERNEL LAYER AND SAMPLING -----------
    k::DatabaseMetrics metrics;
    k::InMemoryDatabase db_(metrics);
    k::CachedDatabaseAccess db(db_, metrics);

    e::DataGraphFixedDegreeGenConfig genConfig{
        .nodeCount = 100000,
        .outgoingDegree = 4,
        .relationshipTypes = {"A", "B", "C", "D"},
        .seed = 1,
    };

    e::populateDataGraphFixedDegree(db_, genConfig);

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

    const size_t max_layer = 3;

    auto runExperiment =
        [&](const std::string &name, ex::NeighborhoodCondition condition)
    {
        std::cout << name << ":" << std::endl;

        ib::IRBuilder builder;

        ib::BuildResult buildResult =
            builder.run(
                query_graphs,
                max_layer);

        ex::PredicateEvaluator predEvaluator(
            buildResult.predicateRegistry,
            db,
            metrics,
            ex::PredicateEvaluatorConfig{
                .neighborhoodCondition = condition,
            });

        ex::CandidateSpaceBuilder csb(
            buildResult,
            db,
            predEvaluator);

        auto start =
            std::chrono::steady_clock::now();

        auto scs =
            csb.build(max_layer);

        auto end =
            std::chrono::steady_clock::now();

        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start);

        std::cout
            << "candidate space build time: "
            << elapsed.count()
            << " ms"
            << std::endl;

        std::cout
            << ex::to_string(scs)
            << std::endl;
    };

    runExperiment(
        "existence",
        ex::NeighborhoodCondition::Existence);

    runExperiment(
        "existence degree",
        ex::NeighborhoodCondition::ExistenceDegree);

    runExperiment(
        "bipartite matching",
        ex::NeighborhoodCondition::BipartiteMatching);
}

void ordering_techniques()
{
    namespace k = kernel;
    namespace ib = ir_builder;
    namespace ex = execution;
    namespace e = evaluation;

    const size_t max_layer = 3;
    auto runExperiment = [&](const std::string &name, ex::NeighborhoodCondition condition, ex::EagerOrderingStrategy eos)
    {
        // ------------- KERNEL LAYER AND SAMPLING -----------
        k::DatabaseMetrics metrics;
        k::InMemoryDatabase db_(metrics);
        k::CachedDatabaseAccess db(db_, metrics);

        e::DataGraphFixedDegreeGenConfig genConfig{
            .nodeCount = 25000,
            .outgoingDegree = 16,
            .relationshipTypes = {"A", "B", "C", "D", "E"},
            .seed = 1,
        };

        e::populateDataGraphFixedDegree(db_, genConfig);

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

        std::cout << "query graph: " << std::endl;
        std::cout << query_graph::to_string(query_graphs[0]) << std::endl;

        std::cout << name << ":" << std::endl;
        ib::IRBuilder builder;
        ib::BuildResult buildResult = builder.run(query_graphs, max_layer);

        ex::PredicateEvaluator predEvaluator(
            buildResult.predicateRegistry,
            db,
            metrics,
            ex::PredicateEvaluatorConfig{
                .neighborhoodCondition = condition});

        auto start = std::chrono::steady_clock::now();
        ex::EagerExecutor executor(buildResult, db, predEvaluator, ex::EagerExecutorOptions{.signatureLevel = max_layer, .ordering = eos},
                                   5000'000);

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
        std::cout << ex::to_string(execution_metrics) << std::endl;
    };

    runExperiment("connected greedy", ex::NeighborhoodCondition::ExistenceDegree, ex::EagerOrderingStrategy::ConnectedGreedy);
    runExperiment("lazy greedy", ex::NeighborhoodCondition::ExistenceDegree, ex::EagerOrderingStrategy::LazyGreedy);
    runExperiment("smallest |C| first", ex::NeighborhoodCondition::ExistenceDegree, ex::EagerOrderingStrategy::SmallestSetFirst);
}

void lazy()
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

        // std::cout << "kernel" << std::endl;
        // std::cout << to_string(db_) << std::endl;

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

        // std::cout << "query graph: " << std::endl;
        // std::cout << query_graph::to_string(query_graphs[0]) << std::endl;

        std::cout << name << ":" << std::endl;
        ib::IRBuilder builder;
        ib::BuildResult buildResult = builder.run(query_graphs, max_layer);

        // std::cout << "predReg" << std::endl;
        // std::cout << to_string(buildResult.predicateRegistry) << std::endl;

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

    for (size_t limit = 7000'000; limit <= 8000'000; limit += 1000'000)
    {
        runExperiment("lazy executor, limit = " + std::to_string(limit), limit);
    }
}

void precomp()
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

        // std::cout << "query graph: " << std::endl;
        // std::cout << query_graph::to_string(query_graphs[0]) << std::endl;

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

    for (size_t limit = 8000'000; limit >= 5000'000; limit -= 1000'000)
    {
        runExperiment("precomp, limit = " + std::to_string(limit), ex::EagerOrderingStrategy::ConnectedGreedy, limit);
    }
}

void compare_planted_motif_eager_lazy()
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

    e::PlantedMotifGenConfig config{
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
    auto eagerPrecomputeStart =
        Clock::now();

    ex::EagerExecutor eagerExecutor(
        buildResult,
        eagerDatabase,
        eagerPredicateEvaluator,
        ex::EagerExecutorOptions{
            .signatureLevel = maxLayer,
            .ordering =
                ex::EagerOrderingStrategy::LazyGreedy,
        });

    auto eagerPrecomputeEnd =
        Clock::now();

    uint64_t eagerMatches = 0;
    uint64_t eagerCandidatesVisited = 0;
    uint64_t eagerRecursiveCalls = 0;

    auto eagerEnumerationStart =
        Clock::now();

    for (size_t queryIndex = 0;
         queryIndex < queryGraphs.size();
         ++queryIndex)
    {
        auto matches =
            eagerExecutor.executeQuery(
                queryIndex);

        eagerMatches +=
            matches.size();

        const auto &queryMetrics =
            eagerExecutor.metrics();

        eagerCandidatesVisited += queryMetrics.candidatesVisited;

        eagerRecursiveCalls +=
            std::accumulate(
                queryMetrics.recursiveCallsAtDepth.begin(),
                queryMetrics.recursiveCallsAtDepth.end(),
                uint64_t{0});
    }

    auto eagerEnumerationEnd =
        Clock::now();

    const auto eagerDatabaseMetrics =
        metrics;

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

    auto lazyStart =
        Clock::now();

    for (size_t queryIndex = 0;
         queryIndex < queryGraphs.size();
         ++queryIndex)
    {
        ex::QueryExecutionResult result =
            lazyExecutor.executeQuery(
                queryIndex);

        lazyMatches +=
            result.matches.size();

        lazyCandidatesVisited +=
            result.metrics.regularCandidatesVisited;

        lazyRecursiveCalls +=
            std::accumulate(
                result.metrics.recursiveCallsAtDepth.begin(),
                result.metrics.recursiveCallsAtDepth.end(),
                uint64_t{0});
    }

    auto lazyEnd =
        Clock::now();

    const auto lazyDatabaseMetrics =
        metrics;

    // ===========================================================================
    // Results
    // ===========================================================================

    auto ms =
        [](auto duration)
    {
        return std::chrono::duration<double, std::milli>(
                   duration)
            .count();
    };

    double eagerPrecomputeMs =
        ms(eagerPrecomputeEnd - eagerPrecomputeStart);

    double eagerEnumerationMs =
        ms(eagerEnumerationEnd - eagerEnumerationStart);

    double eagerTotalMs =
        eagerPrecomputeMs +
        eagerEnumerationMs;

    double lazyMs =
        ms(lazyEnd - lazyStart);

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

    // e::SharedBlockStressConfig config{
    //     .centralCount = 10,
    //     .blockCount = 4,
    //     .embeddingsPerBlock = 20,
    //     .noiseDegree = 20,
    //     .seed = 1,
    // };
    e::SharedBlockStressConfig config{
        .centralCount = 2,
        .blockCount = 4,
        .embeddingsPerBlock = 5,
        .noiseDegree = 0,
        .seed = 1,
    };
    // e::SharedBlockStressConfig config{
    //     .centralCount = 1,
    //     .blockCount = 3,
    //     .embeddingsPerBlock = 2,
    //     .noiseDegree = 0,
    //     .seed = 1,
    // };

    auto workload =
        e::populateSharedBlockStressGraph(
            database,
            config);

    auto &withoutSharedBlocks =
        workload.queryGraphs[0];

    auto &withSharedBlocks =
        workload.queryGraphs[1];

    std::cout << "kernel: " << std::endl;
    std::cout << k::to_string(database) << std::endl;

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

    auto sumVector =
        [](const std::vector<uint64_t> &values)
    {
        return std::accumulate(
            values.begin(),
            values.end(),
            uint64_t{0});
    };

    auto sumSharedBlockRecursiveCalls =
        [&](const ex::SharedBlockExecutionMetrics &sbMetrics)
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
        ib::BuildResult buildResult =
            builder.run(
                queryGraphs,
                maxLayer);

        std::cout << "build result: " << std::endl;
        std::cout << ib::to_string(buildResult) << std::endl;

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

        result.elapsedMs =
            std::chrono::duration<double, std::milli>(
                end - start)
                .count();

        result.matches =
            matchCount;

        result.outerCandidatesVisited =
            queryMetrics.regularCandidatesVisited;

        result.sharedBlockCandidatesVisited =
            sbMetrics.candidateRelationshipsVisited;

        result.totalCandidatesVisited =
            result.outerCandidatesVisited +
            result.sharedBlockCandidatesVisited;

        result.outerRecursiveCalls =
            sumVector(
                queryMetrics.recursiveCallsAtDepth);

        result.sharedBlockRecursiveCalls =
            sumSharedBlockRecursiveCalls(
                sbMetrics);

        result.totalRecursiveCalls =
            result.outerRecursiveCalls +
            result.sharedBlockRecursiveCalls;

        result.sharedBlockRowsVisited =
            queryMetrics.sharedBlockRowsVisited;

        result.sharedBlockRowsRejectedByOuterUniqueness =
            queryMetrics.sharedBlockRowsRejectedByOuterUniqueness;

        result.sharedBlockMetrics =
            sbMetrics;

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

void shared_block_chaotic()
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


    // e::ChaoticDataGraphGenConfig genConfig{
    //     .nodeCount = 100'000,
    //     .relationshipCount = 400'000,
    //     .relationshipTypes = {"A"},
    //     .seed = 1};
    // e::populateChaoticDataGraph(database, genConfig);

    e::DataGraphFixedDegreeGenConfig genConfig{
        . nodeCount = 100'000,
        . outgoingDegree = 4,

        . relationshipTypes = {
            "A"
        },
        . seed = 1
    };
    e::populateDataGraphFixedDegree(database, genConfig);

    // e::SharedBlockStressConfig config{
    //     .centralCount = 2,
    //     .blockCount = 4,
    //     .embeddingsPerBlock = 5,
    //     .noiseDegree = 0,
    //     .seed = 1,
    // };
    

    auto withoutSharedBlocks = e::buildQG(false);
    auto withSharedBlocks = e::buildQG(true);

    std::cout << "QG: " << std::endl;
    std::cout << qg::to_string(withSharedBlocks) << std::endl;
    /**
     * use hierarchical predicates without shared blocks
     */
    constexpr size_t maxLayer = 3;
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

    auto sumVector =
        [](const std::vector<uint64_t> &values)
    {
        return std::accumulate(
            values.begin(),
            values.end(),
            uint64_t{0});
    };

    auto sumSharedBlockRecursiveCalls =
        [&](const ex::SharedBlockExecutionMetrics &sbMetrics)
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
        ib::BuildResult buildResult =
            builder.run(
                queryGraphs,
                maxLayer);

        // std::cout << "build result: " << std::endl;
        // std::cout << ib::to_string(buildResult) << std::endl;

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

        result.elapsedMs =
            std::chrono::duration<double, std::milli>(
                end - start)
                .count();

        result.matches =
            matchCount;

        result.outerCandidatesVisited =
            queryMetrics.regularCandidatesVisited;

        result.sharedBlockCandidatesVisited =
            sbMetrics.candidateRelationshipsVisited;

        result.totalCandidatesVisited =
            result.outerCandidatesVisited +
            result.sharedBlockCandidatesVisited;

        result.outerRecursiveCalls =
            sumVector(
                queryMetrics.recursiveCallsAtDepth);

        result.sharedBlockRecursiveCalls =
            sumSharedBlockRecursiveCalls(
                sbMetrics);

        result.totalRecursiveCalls =
            result.outerRecursiveCalls +
            result.sharedBlockRecursiveCalls;

        result.sharedBlockRowsVisited =
            queryMetrics.sharedBlockRowsVisited;

        result.sharedBlockRowsRejectedByOuterUniqueness =
            queryMetrics.sharedBlockRowsRejectedByOuterUniqueness;

        result.sharedBlockMetrics =
            sbMetrics;

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

    // std::cout
    //     << "centralCount:          "
    //     << config.centralCount
    //     << "\n";

    // std::cout
    //     << "blockCount:            "
    //     << config.blockCount
    //     << "\n";

    // std::cout
    //     << "embeddingsPerBlock:    "
    //     << config.embeddingsPerBlock
    //     << "\n";

    // std::cout
    //     << "noiseDegree:           "
    //     << config.noiseDegree
    //     << "\n";

    // std::cout
    //     << "hierarchical level:    "
    //     << maxLayer
    //     << "\n";

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

int main()
{
    // lazy();
    // precomp();
    // compare_planted_motif_eager_lazy();
    shared_block_chaotic();
}