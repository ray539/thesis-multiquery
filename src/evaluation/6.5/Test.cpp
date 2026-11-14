#include <common/Kernel.hpp>
#include <common/IRBuilder.hpp>
#include <common/Execution.hpp>
#include <common/Evaluation.hpp>

namespace evaluation
{
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
        };

        runExperiment("connected greedy", ex::NeighborhoodCondition::ExistenceDegree, ex::EagerOrderingStrategy::ConnectedGreedy);
        runExperiment("lazy greedy", ex::NeighborhoodCondition::ExistenceDegree, ex::EagerOrderingStrategy::LazyGreedy);
        runExperiment("smallest |C| first", ex::NeighborhoodCondition::ExistenceDegree, ex::EagerOrderingStrategy::SmallestSetFirst);
    }
}