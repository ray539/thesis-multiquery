#pragma once

#include <bits/stdc++.h>

#include <common/Kernel.hpp>

namespace evaluation
{
  namespace k = kernel;

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

} // namespace evaluation