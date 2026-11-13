#include <common/Execution.hpp>

struct SharedBlockTestData
{
  kernel::DRelId l0;
  kernel::DRelId l1;
  kernel::DRelId l2;
  kernel::DRelId l3;
  kernel::DRelId lBoundary;

  kernel::DRelId r0;
  kernel::DRelId r1;
  kernel::DRelId r2;
  kernel::DRelId r3;
  kernel::DRelId rBoundary;
};

SharedBlockTestData populateSharedBlockTestData(kernel::InMemoryDatabase &db)
{
  namespace k = kernel;

  k::DNodeId c = db.addNode({}, {});

  k::DNodeId lt = db.addNode({}, {});

  k::DNodeId ll = db.addNode({}, {});

  k::DNodeId lr = db.addNode({}, {});

  k::DNodeId lb = db.addNode({}, {});

  k::DNodeId rt = db.addNode({}, {});

  k::DNodeId rl = db.addNode({}, {});

  k::DNodeId rr = db.addNode({}, {});

  k::DNodeId rb = db.addNode({}, {});

  SharedBlockTestData result{
      .l0 =
          db.addRelationship(
              lt,
              ll,
              "L0",
              {}),

      .l1 =
          db.addRelationship(
              lt,
              lr,
              "L1",
              {}),

      .l2 =
          db.addRelationship(
              lb,
              ll,
              "L2",
              {}),

      .l3 =
          db.addRelationship(
              lb,
              lr,
              "L3",
              {}),

      .lBoundary =
          db.addRelationship(
              lr,
              c,
              "LB",
              {}),

      .r0 =
          db.addRelationship(
              rt,
              rl,
              "R0",
              {}),

      .r1 =
          db.addRelationship(
              rt,
              rr,
              "R1",
              {}),

      .r2 =
          db.addRelationship(
              rb,
              rl,
              "R2",
              {}),

      .r3 =
          db.addRelationship(
              rb,
              rr,
              "R3",
              {}),

      .rBoundary =
          db.addRelationship(
              rl,
              c,
              "RB",
              {}),
  };

  return result;
}

query_graph::QueryGraph makeSharedBlockTestQuery()
{
  namespace qg = query_graph;

  qg::QueryGraph graph;

  int64_t nextVariable = 0;

  auto createNode =
      [&]()
  {
    qg::VariableId variable{nextVariable++};

    graph.patternNodes.push_back(
        qg::PatternNode{
            .variable = variable,
            .inlinePredicates = {},
        });

    return variable;
  };

  auto createRelationship =
      [&](qg::VariableId start,
          qg::VariableId end,
          const std::string &type)
  {
    qg::VariableId relationship{nextVariable++};

    qg::ExprId variableExpression =
        graph.expressions.add(
            qg::Expression{
                .type = qg::ValueType::Relationship,
                .data = qg::VariableReference(relationship)
            });

    qg::ExprId typePredicate =
        graph.expressions.add(
            qg::Expression{
                .type = qg::ValueType::Bool,
                .data = qg::HasRelType(variableExpression, type)
            });

    graph.patternRelationships.push_back(
        qg::PatternRelationship{
            .variable = relationship,
            .startNode = start,
            .endNode = end,
            .direction = common::Direction::Outgoing,
            .inlinePredicates = {typePredicate},
        });

    return relationship;
  };

  // ---------------------------------------------------------------------------
  // Nodes
  // ---------------------------------------------------------------------------

  qg::VariableId c = createNode();

  qg::VariableId lt = createNode();
  qg::VariableId ll = createNode();
  qg::VariableId lr = createNode();
  qg::VariableId lb = createNode();

  qg::VariableId rt = createNode();
  qg::VariableId rl = createNode();
  qg::VariableId rr = createNode();
  qg::VariableId rb = createNode();

  // ---------------------------------------------------------------------------
  // Left diamond
  //
  //       lt
  //      /  \
  //     ll  lr ---> c
  //      \  /
  //       lb
  // ---------------------------------------------------------------------------

  qg::VariableId l0 =
      createRelationship(lt, ll, "L0");

  qg::VariableId l1 =
      createRelationship(lt, lr, "L1");

  qg::VariableId l2 =
      createRelationship(lb, ll, "L2");

  qg::VariableId l3 =
      createRelationship(lb, lr, "L3");

  qg::VariableId lBoundary =
      createRelationship(lr, c, "LB");

  // ---------------------------------------------------------------------------
  // Right diamond
  //
  //             rt
  //            /  \
  //     c <---rl  rr
  //            \  /
  //             rb
  // ---------------------------------------------------------------------------

  qg::VariableId r0 =
      createRelationship(rt, rl, "R0");

  qg::VariableId r1 =
      createRelationship(rt, rr, "R1");

  qg::VariableId r2 =
      createRelationship(rb, rl, "R2");

  qg::VariableId r3 =
      createRelationship(rb, rr, "R3");

  qg::VariableId rBoundary =
      createRelationship(rl, c, "RB");

  // ---------------------------------------------------------------------------
  // Shared blocks
  //
  // The boundary relationship is intentionally included in the shared block.
  // IRBuilder should identify lBoundary / rBoundary as the articulation
  // relationship in the relationship graph.
  // ---------------------------------------------------------------------------

  graph.sharedBlocks.push_back(
      qg::SharedBlock{
          .block_id = qg::BlockId{0},
          .patternRelationshipIDs = {
              l0,
              l1,
              l2,
              l3,
              lBoundary,
          },
      });

  graph.sharedBlocks.push_back(
      qg::SharedBlock{
          .block_id = qg::BlockId{1},
          .patternRelationshipIDs = {
              r0,
              r1,
              r2,
              r3,
              rBoundary,
          },
      });

  return graph;
}

void test_shared_blocks()
{
  namespace k = kernel;
  namespace ib = ir_builder;
  namespace ex = execution;

  k::DatabaseMetrics metrics;

  k::InMemoryDatabase database{
      metrics};

  auto expected =
      populateSharedBlockTestData(
          database);

  auto query =
      makeSharedBlockTestQuery();

  std::vector<query_graph::QueryGraph> queries{
      query};

  ib::IRBuilder builder;

  ib::BuildResult buildResult =
      builder.run(
          queries,
          3);

  std::cout
      << to_string(buildResult)
      << "\n";

  ex::LazyExecutorWithSB executor(
      database,
      metrics,
      buildResult,
      ex::PredicateEvaluatorConfig{
          .neighborhoodCondition =
              ex::NeighborhoodCondition::ExistenceDegree,
      },1000'000'000);

  auto result =
      executor.executeQuery(0);

  std::cout
      << "matches = "
      << result.matches.size()
      << "\n";

  for (const auto &match : result.matches)
  {
    std::cout
        << ex::to_string(match)
        << "\n";
  }

  std::cout
      << ex::to_string(
             executor.sharedBlockMetrics())
      << "\n";
}

int main()
{
  test_shared_blocks();
}