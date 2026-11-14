#include <common/Kernel.hpp>
#include <common/IRBuilder.hpp>
#include <common/Execution.hpp>
#include <common/Evaluation.hpp>

namespace evaluation
{
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

        auto runExperiment = [&](const std::string &name, ex::NeighborhoodCondition condition)
        {
            std::cout << name << ":" << std::endl;
            ib::IRBuilder builder;
            ib::BuildResult buildResult = builder.run(query_graphs, max_layer);
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

            auto scs = csb.build(max_layer);
            std::cout
                << ex::to_string(scs)
                << std::endl;
        };

        runExperiment("existence", ex::NeighborhoodCondition::Existence);

        runExperiment("existence degree", ex::NeighborhoodCondition::ExistenceDegree);

        runExperiment("bipartite matching", ex::NeighborhoodCondition::BipartiteMatching);
    }

} // namespace evaluation