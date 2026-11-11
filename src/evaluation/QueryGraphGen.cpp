#include <common/Evaluation.hpp>

namespace evaluation
{


  QueryGraphSampler::QueryGraphSampler(
      k::InMemoryDatabase &db,
      QueryGraphSamplerConfig config)
      : db_{db},
        config_{std::move(config)},
        rng_{config_.seed}
  {
    loadRelationships();
    validateConfig();
  }

  void QueryGraphSampler::validateConfig() const
  {
    if (config_.relationshipCount == 0)
    {
      throw std::invalid_argument(
          "Query graph must contain at least one relationship");
    }

    if (config_.relationshipCount > allRelationships_.size())
    {
      throw std::invalid_argument(
          "Query graph cannot contain more relationships than the data graph");
    }

    if (config_.maxAttempts == 0)
    {
      throw std::invalid_argument(
          "QueryGraphSampler maxAttempts must be positive");
    }
  }

  void QueryGraphSampler::loadRelationships()
  {
    allRelationships_.clear();
    auto cursor = db_.getRelationshipCursor();
    k::DRelId relationship{0};
    while (cursor.next(relationship))
      allRelationships_.push_back(relationship);
  }

  k::DRelId QueryGraphSampler::randomRelationship()
  {
    if (allRelationships_.empty())
    {
      throw std::logic_error(
          "Cannot sample from an empty data graph");
    }
    std::uniform_int_distribution<size_t> distribution(0, allRelationships_.size() - 1);
    return allRelationships_[distribution(rng_)];
  }

  qg::QueryGraph QueryGraphSampler::sample()
  {
    for (size_t attempt = 0; attempt < config_.maxAttempts; ++attempt)
    {
      auto sampled = sampleRelationships();
      if (sampled) return buildQueryGraph(*sampled);
    }

    throw std::runtime_error(
        "Failed to sample a connected query graph of requested size");
  }

  std::vector<qg::QueryGraph> QueryGraphSampler::sample(size_t count)
  {
    std::vector<qg::QueryGraph> result;
    result.reserve(count);

    for (size_t i = 0; i < count; ++i)
      result.push_back(sample());

    return result;
  }

  std::optional<std::vector<k::DRelId>>
  QueryGraphSampler::sampleRelationships()
  {
    k::DRelId start = randomRelationship();
    std::vector<k::DRelId> selectedOrder;
    selectedOrder.reserve(config_.relationshipCount);

    std::unordered_set<
        k::DRelId,
        IntStrongHash>
        selected;

    // vector for selecting random relationship
    // set for checking membership (and to avoid duplicates)
    std::vector<k::DRelId> frontier;
    std::unordered_set<
        k::DRelId,
        IntStrongHash>
        frontierMembership;

    selected.insert(start);
    selectedOrder.push_back(start);

    addAdjacentRelationships(start, selected, frontier, frontierMembership);

    while (selectedOrder.size() < config_.relationshipCount)
    {
      if (frontier.empty())
        return std::nullopt;
      k::DRelId next = popRandomFrontier(frontier, frontierMembership);
      if (!selected.insert(next).second)
        continue;
      selectedOrder.push_back(next);
      addAdjacentRelationships(
          next,
          selected,
          frontier,
          frontierMembership);
    }

    return selectedOrder;
  }

  void QueryGraphSampler::addAdjacentRelationships(
      k::DRelId drel,
      const std::unordered_set<k::DRelId, IntStrongHash> &selected,
      std::vector<k::DRelId> &frontier,
      std::unordered_set<k::DRelId, IntStrongHash> &frontierMembership)
  {
    k::DNodeId start =
        db_.accessRelationshipEndpoint(
            drel,
            common::Endpoint::Start);

    k::DNodeId end =
        db_.accessRelationshipEndpoint(
            drel,
            common::Endpoint::End);

    addNodeAdjacency(
        start,
        selected,
        frontier,
        frontierMembership);

    if (end != start)
    {
      addNodeAdjacency(
          end,
          selected,
          frontier,
          frontierMembership);
    }
  }

  void QueryGraphSampler::addNodeAdjacency(
      k::DNodeId node,
      const std::unordered_set<k::DRelId, IntStrongHash> &selected,
      std::vector<k::DRelId> &frontier,
      std::unordered_set<k::DRelId, IntStrongHash> &frontierMembership)
  {
    auto addCursor = [&](common::Direction direction)
    {
      auto cursor = db_.accessAdjacentRels(node, direction);
      k::DRelId relationship{0};
      while (cursor.next(relationship))
      {
        if (selected.contains(relationship))
          continue;

        if (!frontierMembership.insert(relationship).second)
          continue;

        frontier.push_back(relationship);
      }
    };

    addCursor(common::Direction::Outgoing);
    addCursor(common::Direction::Incoming);
  }

  k::DRelId QueryGraphSampler::popRandomFrontier(
      std::vector<k::DRelId> &frontier,
      std::unordered_set<k::DRelId, IntStrongHash> &frontierMembership)
  {
    assert(!frontier.empty());

    std::uniform_int_distribution<size_t> distribution(0, frontier.size() - 1);
    size_t index =  distribution(rng_);
    k::DRelId result = frontier[index];

    /**
     * O(1) erase: order within the frontier is irrelevant.
     */
    frontier[index] = frontier.back();
    frontier.pop_back();
    frontierMembership.erase(result);
    return result;
  }

  qg::QueryGraph QueryGraphSampler::buildQueryGraph(const std::vector<k::DRelId> &relationships)
  {
    qg::QueryGraphBuilder builder;

    std::unordered_map<
        k::DNodeId,
        qg::VariableId,
        IntStrongHash>
        nodeMapping;

    /**
     * Get corresponding `queryNode:VariableId` for the given `dataNode`
     * - If `dataNode` isn't already mapped to a `VariableId`, create new query node
     * - else, return existing one
     */
    auto queryNodeFor = [&](k::DNodeId dataNode) -> qg::VariableId
    {
      auto it = nodeMapping.find(dataNode);
      if (it != nodeMapping.end()) return it->second;

      qg::VariableId queryNode = builder.createNode();

      nodeMapping.emplace(dataNode, queryNode);
      return queryNode;
    };

    // go through the sampled relationships in the BFS order
    for (k::DRelId dataRelationship : relationships)
    {
      k::DNodeId dataStart =
          db_.accessRelationshipEndpoint(
              dataRelationship,
              common::Endpoint::Start);

      k::DNodeId dataEnd =
          db_.accessRelationshipEndpoint(
              dataRelationship,
              common::Endpoint::End);

      qg::VariableId queryStart = queryNodeFor(dataStart);
      qg::VariableId queryEnd = queryNodeFor(dataEnd);

      qg::VariableId queryRelationship =
          builder.createRelationship(
              queryStart,
              queryEnd,
              common::Direction::Outgoing);

      const std::string &relationshipType = db_.accessRelationshipType(dataRelationship);

      // add hasRelType predicate
      auto expr = builder.expr();
      qg::ExprId relationshipReference = expr.variableReference(queryRelationship);
      qg::ExprId typePredicate = expr.hasRelType(relationshipReference, relationshipType);
      builder.addPredicateTo(queryRelationship, typePredicate);
    }

    return builder.build();
  }

  std::vector<qg::QueryGraph> getQueryGraphsFrom(k::InMemoryDatabase& db, QueryGraphSamplerConfig config, size_t count) {
    QueryGraphSampler qgs(db, config);
    return qgs.sample(count);
  }

  

} // namespace evaluation