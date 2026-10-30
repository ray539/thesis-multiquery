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


void test_buildQueryGraph1() {
  auto qg = buildQueryGraph1();
  std::cout << qg::to_string(qg) << '\n';
}


void test_buildQueryGraph2() {
  auto qg = buildQueryGraph2();
  std::cout << qg::to_string(qg) << std::endl;
}


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

void test_irbuilder_qg1() {
  auto qg = buildQueryGraph2();
  ib::IRBuilder ibuilder;
  auto buildResult = ibuilder.run({qg});
  std::cout << ib::to_string(buildResult) << std::endl;
}

void test_irbuilder_qg2() {
  auto qg = buildQueryGraph2();
  ib::IRBuilder ibuilder;
  auto buildResult = ibuilder.run({qg});
  std::cout << ib::to_string(buildResult) << std::endl;
}




int main() {
  test_buildQueryGraph1();
}
