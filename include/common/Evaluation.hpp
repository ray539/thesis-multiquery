#pragma once

#include <bits/stdc++.h>

#include <common/Kernel.hpp>
#include <common/QueryGraph.hpp>

namespace evaluation
{
  namespace k = kernel;
  namespace qg = query_graph;

  struct DataGraphGenConfig
  {
    size_t nodeCount = 100'000;
    size_t outgoingDegree = 4;

    std::vector<std::string> relationshipTypes{
        "A",
        "B",
        "C",
        "D",
        "E",
    };
    uint64_t seed = 1;
  };

  class RandomDataGraphGenerator
  {
  public:
    explicit RandomDataGraphGenerator(DataGraphGenConfig config);

    /**
     * populate the database with the given config options
     */
    void populate(k::InMemoryDatabase &db);

  private:
    DataGraphGenConfig config_;
    std::mt19937_64 rng_;

    void validateConfig() const;
    std::vector<k::DNodeId> createNodes(k::InMemoryDatabase &db);
    /**
     * random value from [0 ... config.nodeCount - 1]
     */
    size_t randomNodeIndex();

    /**
     * random value from [0 ... config.relTypes.size() - 1]
     * - use integers instead of strings
     */
    size_t randomRelationshipTypeIndex();

    /**
     * create relationships for each source node in `nodes`
     */
    void createRelationships(
        k::InMemoryDatabase &db,
        const std::vector<k::DNodeId> &nodes);

    /**
     * create outgoing relationship for given `sourceIndex`
     */
    void createOutgoingRelationships(
        k::InMemoryDatabase &db,
        const std::vector<k::DNodeId> &nodes,
        size_t sourceIndex);
  };

  /**
   * Generate a data graph with a custom configuration.
   */
  void populateDataGraph(
      k::InMemoryDatabase &db,
      DataGraphGenConfig config);

  /**
   * Generate the baseline data graph used by the evaluation.
   *
   * - 100,000 nodes
   * - exactly 4 outgoing relationships per node
   * - 400,000 relationships
   * - relationship types A/B/C/D/E
   * - no properties or node labels
   * - no self loops
   * - no duplicate (start, end, type) relationships
   * - deterministic seed
   */
  void populateDataGraph(k::InMemoryDatabase &db);

  // ---------------------- QUERY GRAPH SAMPLER -----------------
  struct QueryGraphSamplerConfig
  {
    size_t relationshipCount = 10;
    size_t maxAttempts = 100;
    uint64_t seed = 1;
  };

  class QueryGraphSampler
  {
  public:
    QueryGraphSampler(
        k::InMemoryDatabase &db,
        QueryGraphSamplerConfig config = {});

    /**
     * Sample one connected query graph.
     *
     * The query contains exactly config.relationshipCount relationships.
     * Relationship type is the only predicate copied from the data graph.
     */
    query_graph::QueryGraph sample();

    /**
     * Sample several query graphs using the same RNG stream.
     */
    std::vector<query_graph::QueryGraph> sample(size_t count);

  private:
    k::InMemoryDatabase &db_;
    QueryGraphSamplerConfig config_;
    std::mt19937_64 rng_;

    std::vector<k::DRelId> allRelationships_;

    void validateConfig() const;

    /**
     * load all relationships into the sampler class
     */
    void loadRelationships();

    /**
     * get a random relationship
     */
    k::DRelId randomRelationship();

    /**
     * Sample a connected set of data relationships.
     *
     * Returns nullopt if the connected component reached from the randomly
     * selected starting relationship is too small.
     */
    std::optional<std::vector<k::DRelId>> sampleRelationships();

    /**
     * Add to frontier every relationship which shares some endpoint node with `drel`,
     * and is not in `frontier`
     */
    void addAdjacentRelationships(
        k::DRelId drel,
        const std::unordered_set<k::DRelId, IntStrongHash> &selected,
        std::vector<k::DRelId> &frontier,
        std::unordered_set<k::DRelId, IntStrongHash> &frontierMembership);

    /**
     * modifies: `frontier`
     * add all relationships adjacent to `node` which is NOT
     * already in selected to `frontier`
     * 
     */
    void addNodeAdjacency(
        k::DNodeId node,
        const std::unordered_set<k::DRelId, IntStrongHash> &selected,
        std::vector<k::DRelId> &frontier,
        std::unordered_set<k::DRelId, IntStrongHash> &frontierMembership);

    /**
     * Remove and return a uniformly random relationship from frontier.
     */
    k::DRelId popRandomFrontier(
        std::vector<k::DRelId> &frontier,
        std::unordered_set<k::DRelId, IntStrongHash> &frontierMembership);

    /**
     * Convert sampled data relationships into a QueryGraph while preserving
     * shared data-node endpoints.
     */
    query_graph::QueryGraph buildQueryGraph(const std::vector<k::DRelId> &relationships);
  };

  std::vector<qg::QueryGraph> getQueryGraphsFrom(k::InMemoryDatabase& db, QueryGraphSamplerConfig config, size_t count);

} // namespace evaluation