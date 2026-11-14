#include <common/Kernel.hpp>
#include <common/IRBuilder.hpp>
#include <common/Execution.hpp>
#include <common/Evaluation.hpp>

namespace evaluation
{

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

        for (size_t max_layer = 0; max_layer <= 3; max_layer++)
        {
            // -------------- COMPILATION -------------------------
            std::cout << "max_layer: " << max_layer << std::endl;
            ib::IRBuilder builder;
            ib::BuildResult buildResult = builder.run(query_graphs, max_layer);
            ex::PredicateEvaluator predEvaluator(buildResult.predicateRegistry, db, metrics, ex::PredicateEvaluatorConfig{.neighborhoodCondition = ex::NeighborhoodCondition::BipartiteMatching});
            // -------------- CANDIDATE SPACE BUILDER ---------------
            ex::CandidateSpaceBuilder csb(buildResult, db, predEvaluator);
            auto scs = csb.build(max_layer);
            std::cout << ex::to_string(scs) << std::endl;
        }
    }
} // namespace evaluation