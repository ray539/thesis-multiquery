#include <common/Evaluation.hpp>

namespace evaluation
{
  RandomDataGraphGeneratorFixedDegree::RandomDataGraphGeneratorFixedDegree(
      DataGraphFixedDegreeGenConfig config)
      : config_{std::move(config)},
        rng_{config_.seed}
  {
    validateConfig();
  }

  void RandomDataGraphGeneratorFixedDegree::populate(
      k::InMemoryDatabase &db)
  {
    std::vector<k::DNodeId> nodes = createNodes(db);

    createRelationships(db, nodes);
  }

  void RandomDataGraphGeneratorFixedDegree::validateConfig() const
  {
    if (config_.nodeCount < 2)
    {
      throw std::invalid_argument(
          "Data graph must contain at least two nodes");
    }

    if (config_.relationshipTypes.empty())
    {
      throw std::invalid_argument(
          "At least one relationship type is required");
    }

    // simple validationcheck
    // - no self loops, so this node can connect to at most N-1 others
    // - then each of these rels can be one of relTypes.size() types
    size_t maxDistinctRelationshipsPerNode = (config_.nodeCount - 1) * config_.relationshipTypes.size();

    if (config_.outgoingDegree > maxDistinctRelationshipsPerNode)
    {
      throw std::invalid_argument(
          "Requested outgoing degree cannot be generated without duplicates");
    }
  }

  std::vector<k::DNodeId> RandomDataGraphGeneratorFixedDegree::createNodes(
      k::InMemoryDatabase &db)
  {
    std::vector<k::DNodeId> result;

    result.reserve(config_.nodeCount);
    for (size_t i = 0; i < config_.nodeCount; ++i)
    {
      result.push_back(db.addNode({}, {}));
    }
    return result;
  }

  size_t RandomDataGraphGeneratorFixedDegree::randomNodeIndex()
  {
    std::uniform_int_distribution<size_t> distribution(0, config_.nodeCount - 1);
    return distribution(rng_);
  }

  size_t RandomDataGraphGeneratorFixedDegree::randomRelationshipTypeIndex()
  {
    std::uniform_int_distribution<size_t> distribution(0, config_.relationshipTypes.size() - 1);
    return distribution(rng_);
  }

  void RandomDataGraphGeneratorFixedDegree::createRelationships(
      k::InMemoryDatabase &db,
      const std::vector<k::DNodeId> &nodes)
  {
    for (size_t sourceIndex = 0; sourceIndex < nodes.size(); ++sourceIndex)
    {
      createOutgoingRelationships(
          db,
          nodes,
          sourceIndex);
    }
  }

  void RandomDataGraphGeneratorFixedDegree::createOutgoingRelationships(
      k::InMemoryDatabase &db,
      const std::vector<k::DNodeId> &nodes,
      size_t sourceIndex)
  {
    /**
     * For a fixed source node:
     *
     *   (targetIndex, typeIndex)
     *
     * uniquely identifies a relationship.
     */
    std::set<std::pair<size_t, size_t>> usedRelationships;

    while (usedRelationships.size() < config_.outgoingDegree)
    {
      size_t targetIndex = randomNodeIndex();

      if (targetIndex == sourceIndex)
        continue;

      size_t typeIndex = randomRelationshipTypeIndex();

      if (!usedRelationships.insert({targetIndex, typeIndex}).second)
        continue;

      db.addRelationship(
          nodes[sourceIndex],
          nodes[targetIndex],
          config_.relationshipTypes[typeIndex],
          {});
    }
  }

  void populateDataGraphFixedDegree(
      k::InMemoryDatabase &db,
      DataGraphFixedDegreeGenConfig config)
  {
    RandomDataGraphGeneratorFixedDegree generator{std::move(config)};
    generator.populate(db);
  }

  void populateDataGraphFixedDegree(k::InMemoryDatabase &db)
  {
    populateDataGraphFixedDegree(
        db,
        DataGraphFixedDegreeGenConfig{
            .nodeCount = 100'000,
            .outgoingDegree = 4,
            .relationshipTypes =
                {
                    "A",
                    "B",
                    "C",
                    "D",
                    "E",
                },
            .seed = 1,
        });
  }

} // namespace evaluation