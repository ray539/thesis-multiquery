#pragma once

#include <bits/stdc++.h>

#include <common/Kernel.hpp>
#include <common/QueryGraph.hpp>

namespace evaluation
{
  namespace k = kernel;
  namespace qg = query_graph;

  struct DataGraphFixedDegreeGenConfig
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

  class RandomDataGraphGeneratorFixedDegree
  {
  public:
    explicit RandomDataGraphGeneratorFixedDegree(DataGraphFixedDegreeGenConfig config);

    /**
     * populate the database with the given config options
     */
    void populate(k::InMemoryDatabase &db);

  private:
    DataGraphFixedDegreeGenConfig config_;
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
  void populateDataGraphFixedDegree(
      k::InMemoryDatabase &db,
      DataGraphFixedDegreeGenConfig config);

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
  void populateDataGraphFixedDegree(k::InMemoryDatabase &db);

  // -------------------- CHAOTIC GRAPH GENERATOR
  struct ChaoticDataGraphGenConfig
  {
    size_t nodeCount = 100'000;
    size_t relationshipCount = 400'000;

    std::vector<std::string> relationshipTypes{
        "A",
        "B",
        "C",
        "D",
        "E",
    };

    uint64_t seed = 1;
  };

  class ChaoticDataGraphGenerator
  {
  public:
    explicit ChaoticDataGraphGenerator(
        ChaoticDataGraphGenConfig config);

    void populate(k::InMemoryDatabase &db);

  private:
    ChaoticDataGraphGenConfig config_;
    std::mt19937_64 rng_;

    void validateConfig() const;

    std::vector<k::DNodeId> createNodes(
        k::InMemoryDatabase &db);

    size_t randomNodeIndex();

    size_t randomRelationshipTypeIndex();

    void createRelationships(
        k::InMemoryDatabase &db,
        const std::vector<k::DNodeId> &nodes);
  };

  void populateChaoticDataGraph(
      k::InMemoryDatabase &db,
      ChaoticDataGraphGenConfig config);

  // -------------------------- planted motif generator
  namespace k = kernel;
  namespace qg = query_graph;

  struct NoisyDatagraphGenConfig
  {
    size_t nodeCount = 100'000;

    // Number of A relationships initially admitted by the base predicate.
    size_t centralCandidateCount = 10'000;

    // Nested survivor populations.
    size_t level1Count = 1'000;
    size_t level2Count = 100;
    size_t level3Count = 10;

    // Makes the endpoints of central A relationships high-degree.
    size_t noiseOutgoingPerEndpoint = 32;

    // Additional unrelated graph noise.
    size_t backgroundNoiseRelationshipCount = 100'000;

    /**
     * The first eight types are interpreted as:
     *
     *   0: A
     *   1: B
     *   2: C
     *   3: D
     *   4: E
     *   5: F
     *   6: G
     *   7: H
     *
     * The final type is reserved for noise.
     */
    std::vector<std::string> relationshipTypes{
        "A",
        "B",
        "C",
        "D",
        "E",
        "F",
        "G",
        "H",
        "NOISE",
    };

    size_t queryCount = 50;

    uint64_t seed = 1;
  };

  struct NoisyDatagraphGenResult
  {
    // All A relationships created as potential central candidates.
    std::vector<k::DRelId> centralRelationships;

    // Central A relationships that have the complete level-3 motif.
    std::vector<k::DRelId> fullMotifCenters;

    // Identical structural queries, intentionally maximising signature reuse.
    std::vector<qg::QueryGraph> queryGraphs;
  };

  class NoisyDatagraphGenerator
  {
  public:
    explicit NoisyDatagraphGenerator(NoisyDatagraphGenConfig config);

    NoisyDatagraphGenResult populate(k::InMemoryDatabase &db);

  private:
    NoisyDatagraphGenConfig config_;
    std::mt19937_64 rng_;

    std::vector<k::DNodeId> nodes_;
    size_t nextUnusedNode_ = 0;

    void validateConfig() const;

    void createNodes(k::InMemoryDatabase &db);

    k::DNodeId allocateNode();

    k::DNodeId randomNode();

    k::DNodeId randomNodeOtherThan(k::DNodeId node);

    void addNoise(
        k::InMemoryDatabase &db,
        k::DNodeId node);

    void addBackgroundNoise(
        k::InMemoryDatabase &db);

    qg::QueryGraph buildQueryGraph() const;
  };

  NoisyDatagraphGenResult populatePlantedMotifGraph(
      k::InMemoryDatabase &db,
      NoisyDatagraphGenConfig config = {});

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

  std::vector<qg::QueryGraph> getQueryGraphsFrom(k::InMemoryDatabase &db, QueryGraphSamplerConfig config, size_t count);

  struct SharedBlockStressConfig
  {
    size_t centralCount = 10;

    // Number of loops attached to each center.
    size_t blockCount = 4;

    // Number of distinct internal embeddings for each
    // (block, boundary relationship).
    size_t embeddingsPerBlock = 20;

    // Extra homogeneous R relationships around structural nodes.
    size_t noiseDegree = 20;

    uint64_t seed = 1;
  };

  struct SharedBlockStressResult
  {
    /**
     * queryGraphs[0]:
     *   identical query with no shared blocks marked.
     *
     * queryGraphs[1]:
     *   same query, with each loop marked as a shared block.
     */
    std::vector<qg::QueryGraph> queryGraphs;

    /**
     * One central data node for each planted instance family.
     */
    std::vector<k::DNodeId> centralNodes;

    /**
     * boundaryRelationships[c][b]
     *
     * Boundary relationship for block b around central node c.
     */
    std::vector<std::vector<k::DRelId>> boundaryRelationships;
  };

  qg::QueryGraph buildQG(bool sharedBlocks);
  SharedBlockStressResult populateSharedBlockStressGraph(k::InMemoryDatabase &db, SharedBlockStressConfig config = {});


  // ---------------------- EVALUTION TEST FUNCTIONS -------------
  // 6.3
  void candidate_set_evaluation_layers();
  // 6.4
  void hp_neighbor_condition();
  // 6.5
  void ordering_techniques();
  // 6.6
  void cdg_lazy();
  void cdg_precomp();
  void noise_ddg_eager_lazy();
  // 6.7
  void shared_block_eval();

} // namespace evaluation