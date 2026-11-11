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

  e::DataGraphGenConfig genConfig{
    .nodeCount=10,
    .outgoingDegree=4,
    .relationshipTypes={"A","B","C","D"},
    .seed=1
  };
  e::populateDataGraph(db_, genConfig);
  e::QueryGraphSamplerConfig qconfig {
    .relationshipCount=5,
    .maxAttempts=100,
    .seed=1
  };
  auto query_graphs = e::getQueryGraphsFrom(db_, qconfig, 1);


  std::cout << "kernel state: " << std::endl;
  std::cout << k::to_string(db_) << std::endl;
  std::cout << "query graph: " << std::endl;
  std::cout << qg::to_string(query_graphs[0]) << std::endl;

  // -------------- COMPILATION -------------------------
  ib::IRBuilder builder{};
  ib::BuildResult buildResult = builder.run(query_graphs, 0);
  ex::PredicateEvaluator predEvaluator(buildResult.predicateRegistry, db, metrics, ex::PredicateEvaluatorConfig{
    .neighborhoodCondition = ex::NeighborhoodCondition::BipartiteMatching
  });


  // --------------- EXECUTION
  ex::EagerExecutor executor(buildResult, db, predEvaluator, ex::EagerExecutorOptions {
    .signatureLevel = 0
  });

  auto embeddings = executor.executeQuery(0);
  std::cout << "resultant embeddings: "<< std::endl;
  for (auto embedding : embeddings) {
    std::cout << ex::to_string(embedding) << std::endl;
  }
  std::cout << "executor metrics: " << std::endl;
  std::cout << ex::to_string(executor.metrics()) << std::endl;

  std::cout << "database metrics: " << std::endl;
  std::cout << k::to_string(db.metrics()) << std::endl;

}

int main()
{
  simple_test();
}