#include <common/IRBuilder.hpp>
#include <ir_builder/IRBuilderInternal.hpp>
#include <common/QueryGraph.hpp>

namespace qg = query_graph;
namespace ib = ir_builder;

/**
 * (n0)-[r0]->(n1)
 * r0.x = 1
 */
qg::QueryGraph buildQueryGraph1() {
  qg::QueryGraphBuilder b;

  auto n0 = b.createNode();
  auto n1 = b.createNode();
  auto r0 = b.createRelationship(n0, n1);

  qg::QueryGraphBuilder::ExpressionBuilder e = b.expr();

  auto r0Ref = e.variableReference(r0);
  auto r0X = e.property(r0Ref, "x"); // r0.x
  auto one = e.integerLiteral(1); // 1

  auto predicate = e.comparison(r0X, qg::ComparisonOp::Equal, one); // r0.x = 1
  b.addPredicateTo(r0, predicate);
  return b.build();
}

/**
 * (a:Person {age:18}) - [r:KNOWS] -> (c)
 */
qg::QueryGraph buildQueryGraph2() {
  qg::QueryGraphBuilder builder;
  auto a = builder.createNode();
  auto c = builder.createNode();
  auto r = builder.createRelationship(a, c);
  auto e = builder.expr();
  auto aref = e.variableReference(a);
  auto rref = e.variableReference(r);

  auto aperson = e.hasLabel(aref, "Person"); // (a:Person)
  auto rKnows = e.hasRelType(rref, "KNOWS"); // [r:KNOWS]

  auto a_age = e.property(aref, "age");
  auto v18 = e.integerLiteral(18);
  auto a_age18 = e.comparison(a_age, qg::ComparisonOp::Equal, v18); //  a.age=18

  builder.addPredicateTo(a, e.booleanBinary(aperson, qg::BooleanOp::And, a_age18));
  builder.addPredicateTo(r, rKnows);
  return builder.build();
}

/**
 * (a)-[r1:A]->(b)-[r2:B]->(c)-[r3:C]->(d)-[r4:D]->(e)-[r5:E]->(f)
 */
qg::QueryGraph makeHierarchicalSignatureTestQuery()
{
  qg::QueryGraphBuilder b;
  auto e = b.expr();
  auto a = b.createNode();
  auto bNode = b.createNode();
  auto c = b.createNode();
  auto d = b.createNode();
  auto eNode = b.createNode();
  auto f = b.createNode();

  auto r1 = b.createRelationship(a, bNode);
  auto r2 = b.createRelationship(bNode, c);
  auto r3 = b.createRelationship(c, d);
  auto r4 = b.createRelationship(d, eNode);
  auto r5 = b.createRelationship(eNode, f);

  b.addPredicateTo(r1, e.hasRelType(e.variableReference(r1), "A"));
  b.addPredicateTo(r2, e.hasRelType(e.variableReference(r2), "B"));
  b.addPredicateTo(r3, e.hasRelType(e.variableReference(r3), "C"));
  b.addPredicateTo(r4, e.hasRelType(e.variableReference(r4), "D"));
  b.addPredicateTo(r5, e.hasRelType(e.variableReference(r5), "E"));

  return b.build();
}

qg::QueryGraph makeTwoSharedCycleBlocks()
{
  qg::QueryGraphBuilder b;

  // ---------------------------------------------------------------------------
  // Nodes
  // ---------------------------------------------------------------------------

  auto a1 = b.createNode();
  auto a2 = b.createNode();
  auto a3 = b.createNode();
  auto a4 = b.createNode();

  auto c = b.createNode();

  auto b1 = b.createNode();
  auto b2 = b.createNode();
  auto b3 = b.createNode();
  auto b4 = b.createNode();

  // ---------------------------------------------------------------------------
  // Left cycle: anticlockwise
  //
  //       a2
  //      /  \
  //    a3    a1
  //      \  /
  //       a4
  //
  // a1 -> a2 -> a3 -> a4 -> a1
  // ---------------------------------------------------------------------------

  auto r1 = b.createRelationship(a1, a2);
  auto r2 = b.createRelationship(a2, a3);
  auto r3 = b.createRelationship(a3, a4);
  auto r4 = b.createRelationship(a4, a1);

  // ---------------------------------------------------------------------------
  // Right cycle: clockwise
  //
  //       b2
  //      /  \
  //    b1    b3
  //      \  /
  //       b4
  //
  // b1 -> b2 -> b3 -> b4 -> b1
  // ---------------------------------------------------------------------------

  auto r6 = b.createRelationship(b1, b2);
  auto r7 = b.createRelationship(b2, b3);
  auto r8 = b.createRelationship(b3, b4);
  auto r9 = b.createRelationship(b4, b1);

  // ---------------------------------------------------------------------------
  // Central bridge
  //
  // a1 <-r5- c -r10-> b1
  // ---------------------------------------------------------------------------

  auto r5 = b.createRelationship(c, a1);
  auto r10 = b.createRelationship(c, b1);

  // ---------------------------------------------------------------------------
  // Shared blocks
  //
  // Both cycle occurrences use the same BlockId because they have the same
  // context-independent structure.
  //
  // The bridge relationships r5 and r10 are outside the shared blocks.
  // ---------------------------------------------------------------------------

  qg::BlockId cycleBlock{0};

  b.addSharedBlock(
      cycleBlock,
      {
          r5,
          r1,
          r2,
          r3,
          r4,
      });

  b.addSharedBlock(
      cycleBlock,
      {
          r10,
          r6,
          r7,
          r8,
          r9,
      });

  return b.build();
}

std::vector<qg::QueryGraph> makeIdenticalFullySharedQueries()
{
  constexpr int64_t SHARED_BLOCK_ID = 0;

  auto makeQuery = []()
  {
    qg::QueryGraphBuilder b;

    /**
     *    n0
     *   /  \
     * n1    n3
     *   \  /
     *    n2
     */
    // Directed cycle:
    // n0 -> n1 -> n2 -> n3 -> n0
    auto n0 = b.createNode();
    auto n1 = b.createNode();
    auto n2 = b.createNode();
    auto n3 = b.createNode();

    auto r0 = b.createRelationship(n0, n1);
    auto r1 = b.createRelationship(n1, n2);
    auto r2 = b.createRelationship(n2, n3);
    auto r3 = b.createRelationship(n3, n0);

    // The entire query graph is one occurrence of block 0.
    //
    // Since there is nothing outside the block, the compiled SharedBlockIR
    // should have boundary_node = std::nullopt.
    b.addSharedBlock(
        qg::BlockId{SHARED_BLOCK_ID},
        {
            r0,
            r1,
            r2,
            r3,
        });

    return b.build();
  };

  qg::QueryGraph q1 = makeQuery();
  qg::QueryGraph q2 = makeQuery();

  return {q1, q2};
}

// OK
void test_buildQueryGraph1() {
  auto qg = buildQueryGraph1();
  std::cout << qg::to_string(qg) << '\n';
}

// OK
void test_buildQueryGraph2() {
  auto qg = buildQueryGraph2();
  std::cout << qg::to_string(qg) << std::endl;
}

// OK
void test_irbuilder_nosharedblocks_qg1() {
  auto qg = buildQueryGraph1();
  std::vector<qg::QueryGraph> qgs{qg};
  auto ge = ib::buildGlobalEntities(qgs);
  std::cout << ib::to_string(ge) << std::endl;
  ib::collectPredicates(qgs,ge);
  std::cout << ib::to_string(ge) << std::endl;
  auto [signatures, pRegistry] = buildAndInternSignatures(ge, 3);
  std::cout << ib::to_string(pRegistry) << std::endl;
}

// OK
void test_irbuilder_qg1() {
  auto qg = buildQueryGraph2();
  ib::IRBuilder ibuilder;
  auto buildResult = ibuilder.run({qg});
  std::cout << ib::to_string(buildResult) << std::endl;
}

// OK
void test_irbuilder_qg2() {
  auto qg = buildQueryGraph2();
  ib::IRBuilder ibuilder;
  auto buildResult = ibuilder.run({qg});
  std::cout << ib::to_string(buildResult) << std::endl;
}

// OK
void test_hierarchical_predicates() {
  auto qg = makeHierarchicalSignatureTestQuery();
  ib::IRBuilder ir_builder;
  auto buildResult = ir_builder.run({qg});
  std::cout<<ib::to_string(buildResult)<<std::endl;
}

// OK
void test_shared_blocks() {
  auto qg = makeTwoSharedCycleBlocks();
  ib::IRBuilder irbuilder;
  auto buildResult = irbuilder.run({qg});
  std::cout<<ib::to_string(buildResult)<<std::endl;
}

// OK
void test_2fullgraph_identical() {
  auto qgs = makeIdenticalFullySharedQueries();
  ib::IRBuilder irbuilder;
  auto buildResult = irbuilder.run(qgs);
  std::cout<<ib::to_string(buildResult)<<std::endl;
}

int main() {
  test_shared_blocks();
}
