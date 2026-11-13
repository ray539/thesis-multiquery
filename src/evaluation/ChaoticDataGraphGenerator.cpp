#include <common/Evaluation.hpp>

namespace evaluation
{
  ChaoticDataGraphGenerator::ChaoticDataGraphGenerator(
      ChaoticDataGraphGenConfig config)
      : config_{std::move(config)},
        rng_{config_.seed}
  {
    validateConfig();
  }

  void ChaoticDataGraphGenerator::validateConfig() const
  {
    if (config_.nodeCount < 2)
    {
      throw std::invalid_argument(
          "Chaotic data graph must contain at least two nodes");
    }

    if (config_.relationshipTypes.empty())
    {
      throw std::invalid_argument(
          "At least one relationship type is required");
    }
  }

  void ChaoticDataGraphGenerator::populate(
      k::InMemoryDatabase &db)
  {
    std::vector<k::DNodeId> nodes = createNodes(db);
    createRelationships(db, nodes);
  }

  std::vector<k::DNodeId> ChaoticDataGraphGenerator::createNodes(
      k::InMemoryDatabase &db)
  {
    std::vector<k::DNodeId> nodes;
    nodes.reserve(config_.nodeCount);
    for (size_t i = 0; i < config_.nodeCount; ++i)
    {
      nodes.push_back(db.addNode({}, {}));
    }
    return nodes;
  }

  size_t ChaoticDataGraphGenerator::randomNodeIndex()
  {
    std::uniform_int_distribution<size_t> distribution(0,config_.nodeCount - 1);
    return distribution(rng_);
  }

  size_t ChaoticDataGraphGenerator::randomRelationshipTypeIndex()
  {
    std::uniform_int_distribution<size_t> distribution(
        0,
        config_.relationshipTypes.size() - 1);

    return distribution(rng_);
  }

  void ChaoticDataGraphGenerator::createRelationships(
      k::InMemoryDatabase &db,
      const std::vector<k::DNodeId> &nodes)
  {
    for (size_t i = 0; i < config_.relationshipCount; ++i)
    {
      size_t startIndex =
          randomNodeIndex();

      size_t endIndex =
          randomNodeIndex();

      while (endIndex == startIndex)
      {
        endIndex =
            randomNodeIndex();
      }

      size_t typeIndex =
          randomRelationshipTypeIndex();

      db.addRelationship(
          nodes[startIndex],
          nodes[endIndex],
          config_.relationshipTypes[typeIndex],
          {});
    }
  }

  void populateChaoticDataGraph(
      k::InMemoryDatabase &db,
      ChaoticDataGraphGenConfig config)
  {
    ChaoticDataGraphGenerator generator{std::move(config)};
    generator.populate(db);
  }

} // namespace evaluation