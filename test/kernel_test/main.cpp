#include <common/Kernel.hpp>

#include <cassert>
#include <iostream>
#include <vector>

namespace qg = query_graph;
namespace ib = ir_builder;
namespace k = kernel;

/**
 * build data graph as
 * (n0{age:20,score:100})-[r0:KNOWS]->(n1)-[r1:LIKES]->(n2)
 * (n0)-[r2:FOLLOWS]->(n2)
 */
void test_raw_kernel()
{
  k::DatabaseMetrics metrics;
  k::InMemoryDatabase db{metrics};
  k::DNodeId n0{0};
  k::DNodeId n1{1};
  k::DNodeId n2{2};

  k::DRelId r0{0};
  k::DRelId r1{1};
  k::DRelId r2{2};

  db.addNode(
      k::DNode{
          .id = n0,
          .properties = {
              {"age", 20},
              {"score", 100},
          },
          .labels = {
              "Person",
              "Student",
          }});

  db.addNode(
      k::DNode{
          .id = n1,
          .properties = {
              {"age", 30},
          },
          .labels = {
              "Person",
          }});

  db.addNode(
      k::DNode{
          .id = n2,
          .properties = {
              {"value", 50},
          },
          .labels = {
              "Item",
          }});

  db.addRelationship(
      k::DRelationship{
          .id = r0,
          .start = n0,
          .end = n1,
          .relType = "KNOWS",
          .properties = {
              {"weight", 5},
          }});

  db.addRelationship(
      k::DRelationship{
          .id = r1,
          .start = n1,
          .end = n2,
          .relType = "LIKES",
          .properties = {
              {"weight", 10},
          }});

  db.addRelationship(
      k::DRelationship{
          .id = r2,
          .start = n0,
          .end = n2,
          .relType = "FOLLOWS",
          .properties = {}});

  // ---------------------------------------------------------------------------
  // Node property access
  // ---------------------------------------------------------------------------

  assert(db.accessNodeProperty(n0, "age") == 20);
  assert(db.accessNodeProperty(n0, "score") == 100);
  assert(db.accessNodeProperty(n1, "age") == 30);

  assert(!db.accessNodeProperty(n0, "missing").has_value());

  // ---------------------------------------------------------------------------
  // Relationship property access
  // ---------------------------------------------------------------------------

  assert(db.accessRelationshipProperty(r0, "weight") == 5);
  assert(db.accessRelationshipProperty(r1, "weight") == 10);

  assert(!db.accessRelationshipProperty(r2, "weight").has_value());

  // ---------------------------------------------------------------------------
  // Endpoints
  // ---------------------------------------------------------------------------

  assert(db.accessRelationshipEndpoint(r0, common::Endpoint::Start) == n0);
  assert(db.accessRelationshipEndpoint(r0, common::Endpoint::End) == n1);

  assert(db.accessRelationshipEndpoint(r1, common::Endpoint::Start) == n1);
  assert(db.accessRelationshipEndpoint(r1, common::Endpoint::End) == n2);

  // ---------------------------------------------------------------------------
  // Relationship types
  // ---------------------------------------------------------------------------

  assert(db.accessRelationshipType(r0) == "KNOWS");
  assert(db.accessRelationshipType(r1) == "LIKES");
  assert(db.accessRelationshipType(r2) == "FOLLOWS");

  // ---------------------------------------------------------------------------
  // Labels
  // ---------------------------------------------------------------------------

  assert(db.accessNodeHasLabel(n0, "Person"));
  assert(db.accessNodeHasLabel(n0, "Student"));
  assert(!db.accessNodeHasLabel(n0, "Item"));

  assert(db.accessNodeHasLabel(n2, "Item"));
  assert(!db.accessNodeHasLabel(n2, "Person"));

  // ---------------------------------------------------------------------------
  // All relationship cursor
  // ---------------------------------------------------------------------------

  {
    auto cursor = db.getRelationshipCursor(); // one open
    std::vector<k::DRelId> found;
    k::DRelId rel{0};
    while (cursor.next(rel)) // increments when producing {0,1,2}
      found.push_back(rel);
    assert(found.size() == 3);
    assert(found[0] == r0);
    assert(found[1] == r1);
    assert(found[2] == r2);
  }
  assert(metrics.database.allRelationshipCursorOpens == 1);
  assert(metrics.database.allRelationshipCursorIncrements == 3);

  // ---------------------------------------------------------------------------
  // Outgoing adjacency
  // ---------------------------------------------------------------------------

  {
    auto cursor = db.accessAdjacentRels(n0, common::Direction::Outgoing);
    std::vector<k::DRelId> found;
    k::DRelId rel{0};
    while (cursor.next(rel))
      found.push_back(rel);
    assert(found.size() == 2);
    std::unordered_set<k::DRelId, IntStrongHash> expected{
        r0,
        r2,
    };
    assert(expected.contains(found[0]));
    assert(expected.contains(found[1]));
  }

  // ---------------------------------------------------------------------------
  // Incoming adjacency
  // ---------------------------------------------------------------------------

  {
    auto cursor = db.accessAdjacentRels(n2, common::Direction::Incoming);
    std::unordered_set<k::DRelId, IntStrongHash> found;
    k::DRelId rel{0};
    while (cursor.next(rel))
      found.insert(rel);
    assert(found.size() == 2);
    assert(found.contains(r1));
    assert(found.contains(r2));
  }

  std::cout << "test_raw_kernel passed\n";
}

/**
 * build data graph as
 * (n0:Person {age:20})-[r0:KNOWS {weight:5}]->(n1)
 */
void test_cached_properties()
{
  k::DatabaseMetrics metrics;
  k::InMemoryDatabase db{metrics};
  k::DNodeId n0{0};
  k::DNodeId n1{1};
  k::DRelId r0{0};
  db.addNode(
      k::DNode{
          .id = n0,
          .properties = {
              {"age", 20},
          },
          .labels = {
              "Person",
          }});

  db.addNode(
      k::DNode{
          .id = n1,
          .properties = {},
          .labels = {}});

  db.addRelationship(
      k::DRelationship{
          .id = r0,
          .start = n0,
          .end = n1,
          .relType = "KNOWS",
          .properties = {
              {"weight", 5},
          }});

  k::CachedDatabaseAccess cached{db, metrics};

  // ---------------------------------------------------------------------------
  // Node property cache
  // ---------------------------------------------------------------------------

  assert(cached.nodeProperty(n0, "age") == 20);
  assert(metrics.nodeAttributeCache.lookups == 1);
  assert(metrics.nodeAttributeCache.hits == 0);
  assert(metrics.nodeAttributeCache.misses == 1);

  assert(cached.nodeProperty(n0, "age") == 20);

  assert(metrics.nodeAttributeCache.lookups == 2);
  assert(metrics.nodeAttributeCache.hits == 1);
  assert(metrics.nodeAttributeCache.misses == 1);

  // Missing values should be cached too.
  assert(!cached.nodeProperty(n0, "missing").has_value());

  assert(metrics.nodeAttributeCache.lookups == 3);
  assert(metrics.nodeAttributeCache.hits == 1);
  assert(metrics.nodeAttributeCache.misses == 2);

  assert(!cached.nodeProperty(n0, "missing").has_value());

  assert(metrics.nodeAttributeCache.lookups == 4);
  assert(metrics.nodeAttributeCache.hits == 2);
  assert(metrics.nodeAttributeCache.misses == 2);

  // ---------------------------------------------------------------------------
  // Relationship property cache
  // ---------------------------------------------------------------------------

  assert(cached.relationshipProperty(r0, "weight") == 5);

  assert(metrics.relationshipAttributeCache.lookups == 1);
  assert(metrics.relationshipAttributeCache.hits == 0);
  assert(metrics.relationshipAttributeCache.misses == 1);

  assert(cached.relationshipProperty(r0, "weight") == 5);

  assert(metrics.relationshipAttributeCache.lookups == 2);
  assert(metrics.relationshipAttributeCache.hits == 1);
  assert(metrics.relationshipAttributeCache.misses == 1);

  std::cout << "test_cached_properties passed\n";
}

/**
 * build datagraph as
 * (n0)-[r0:R]->(n1)
 * 
 */
void test_cached_endpoints()
{
  k::DatabaseMetrics metrics;
  k::InMemoryDatabase db{metrics};

  k::DNodeId n0{0};
  k::DNodeId n1{1};
  k::DRelId r0{0};

  db.addNode(k::DNode{.id = n0, .properties={}, .labels={}});
  db.addNode(k::DNode{.id = n1, .properties={}, .labels={}});

  db.addRelationship(
      k::DRelationship{
          .id = r0,
          .start = n0,
          .end = n1,
          .relType = "R",
          .properties={}
      });

  k::CachedDatabaseAccess cached{db, metrics};

  assert(cached.endpoint(r0, common::Endpoint::Start) == n0);

  assert(metrics.relationshipEndpointCache.lookups == 1);
  assert(metrics.relationshipEndpointCache.hits == 0);
  assert(metrics.relationshipEndpointCache.misses == 1);

  assert(cached.endpoint(r0, common::Endpoint::Start) == n0);

  assert(metrics.relationshipEndpointCache.lookups == 2);
  assert(metrics.relationshipEndpointCache.hits == 1);
  assert(metrics.relationshipEndpointCache.misses == 1);

  // Start and End are separate cache keys.
  assert(cached.endpoint(r0, common::Endpoint::End) == n1);

  assert(metrics.relationshipEndpointCache.lookups == 3);
  assert(metrics.relationshipEndpointCache.hits == 1);
  assert(metrics.relationshipEndpointCache.misses == 2);

  assert(cached.endpoint(r0, common::Endpoint::End) == n1);

  assert(metrics.relationshipEndpointCache.lookups == 4);
  assert(metrics.relationshipEndpointCache.hits == 2);
  assert(metrics.relationshipEndpointCache.misses == 2);

  std::cout << "test_cached_endpoints passed\n";
}

/**
 * build datagraph as
 * (n0:Person)-[r0:KNOWS]->(n1)
 */
void test_cached_types_and_labels()
{
  k::DatabaseMetrics metrics;
  k::InMemoryDatabase db{metrics};

  k::DNodeId n0{0};
  k::DNodeId n1{1};
  k::DRelId r0{0};

  db.addNode(
      k::DNode{
          .id = n0,
          .properties = {},
          .labels = {
              "Person",
          }});

  db.addNode(
      k::DNode{
          .id = n1,
          .properties = {},
          .labels = {}});

  db.addRelationship(
      k::DRelationship{
          .id = r0,
          .start = n0,
          .end = n1,
          .relType = "KNOWS",
          .properties={}
      });

  k::CachedDatabaseAccess cached{db, metrics};

  // ---------------------------------------------------------------------------
  // Relationship type
  // counted as relationship attribute access.
  // ---------------------------------------------------------------------------

  assert(cached.relationshipType(r0) == "KNOWS");

  assert(metrics.relationshipAttributeCache.lookups == 1);
  assert(metrics.relationshipAttributeCache.hits == 0);
  assert(metrics.relationshipAttributeCache.misses == 1);

  assert(cached.relationshipType(r0) == "KNOWS");

  assert(metrics.relationshipAttributeCache.lookups == 2);
  assert(metrics.relationshipAttributeCache.hits == 1);
  assert(metrics.relationshipAttributeCache.misses == 1);

  // ---------------------------------------------------------------------------
  // Node label
  // counted as node attribute access.
  // ---------------------------------------------------------------------------

  assert(cached.nodeHasLabel(n0, "Person"));

  assert(metrics.nodeAttributeCache.lookups == 1);
  assert(metrics.nodeAttributeCache.hits == 0);
  assert(metrics.nodeAttributeCache.misses == 1);

  assert(cached.nodeHasLabel(n0, "Person"));

  assert(metrics.nodeAttributeCache.lookups == 2);
  assert(metrics.nodeAttributeCache.hits == 1);
  assert(metrics.nodeAttributeCache.misses == 1);

  // Negative label lookup should also be cached.
  assert(!cached.nodeHasLabel(n0, "Student"));

  assert(metrics.nodeAttributeCache.lookups == 3);
  assert(metrics.nodeAttributeCache.hits == 1);
  assert(metrics.nodeAttributeCache.misses == 2);

  assert(!cached.nodeHasLabel(n0, "Student"));

  assert(metrics.nodeAttributeCache.lookups == 4);
  assert(metrics.nodeAttributeCache.hits == 2);
  assert(metrics.nodeAttributeCache.misses == 2);

  std::cout << "test_cached_types_and_labels passed\n";
}

void test_cached_cursors()
{
  k::DatabaseMetrics metrics;
  k::InMemoryDatabase db{metrics};

  k::DNodeId n0{0};
  k::DNodeId n1{1};
  k::DNodeId n2{2};

  k::DRelId r0{0};
  k::DRelId r1{1};

  db.addNode(k::DNode{.id = n0, .properties={}, .labels={}});
  db.addNode(k::DNode{.id = n1, .properties={}, .labels={}});
  db.addNode(k::DNode{.id = n2, .properties={}, .labels={}});

  db.addRelationship(
      k::DRelationship{
          .id = r0,
          .start = n0,
          .end = n1,
          .relType = "A",
          .properties={}
    });

  db.addRelationship(
      k::DRelationship{
          .id = r1,
          .start = n0,
          .end = n2,
          .relType = "B",
          .properties={}
  });

  k::CachedDatabaseAccess cached{db, metrics};

  // CachedDatabaseAccess should not cache cursors themselves.
  // It should just delegate traversal while preserving cursor metrics.
  {
    auto cursor = cached.getRelationshipCursor();

    k::DRelId rel{0};
    size_t count = 0;

    while (cursor.next(rel))
      ++count;

    assert(count == 2);
  }

  assert(metrics.database.allRelationshipCursorOpens == 1);

  {
    auto cursor = cached.adjacent(n0, common::Direction::Outgoing);

    std::unordered_set<k::DRelId, IntStrongHash> found;

    k::DRelId rel{0};

    while (cursor.next(rel))
      found.insert(rel);

    assert(found.size() == 2);
    assert(found.contains(r0));
    assert(found.contains(r1));
  }

  assert(metrics.database.adjacencyCursorOpens == 1);

  std::cout << "test_cached_cursors passed\n";
}

void test_simple()
{
  test_raw_kernel();
  test_cached_properties();
  test_cached_endpoints();
  test_cached_types_and_labels();
  test_cached_cursors();
  std::cout << "all kernel tests passed\n";
}

int main()
{
  test_simple();
}