#include <common/Evaluation.hpp>

#include <stdexcept>

namespace evaluation
{
  PlantedMotifGenerator::PlantedMotifGenerator(
      PlantedMotifGenConfig config)
      : config_{std::move(config)},
        rng_{config_.seed}
  {
    validateConfig();
  }

  void PlantedMotifGenerator::validateConfig() const
  {
    if (config_.nodeCount < 2)
    {
      throw std::invalid_argument(
          "PlantedMotifGenerator: nodeCount must be at least 2");
    }

    if (config_.relationshipTypes.size() < 9)
    {
      throw std::invalid_argument(
          "PlantedMotifGenerator: at least 9 relationship types are required");
    }

    if (config_.level1Count > config_.centralCandidateCount ||
        config_.level2Count > config_.level1Count ||
        config_.level3Count > config_.level2Count)
    {
      throw std::invalid_argument(
          "PlantedMotifGenerator: expected "
          "level3 <= level2 <= level1 <= centralCandidateCount");
    }
  }

  void PlantedMotifGenerator::createNodes(
      k::InMemoryDatabase &db)
  {
    nodes_.reserve(config_.nodeCount);

    for (size_t i = 0; i < config_.nodeCount; ++i)
    {
      nodes_.push_back(db.addNode({}, {}));
    }
  }

  k::DNodeId PlantedMotifGenerator::allocateNode()
  {
    if (nextUnusedNode_ >= nodes_.size())
    {
      throw std::runtime_error(
          "PlantedMotifGenerator: nodeCount too small for requested workload");
    }

    return nodes_[nextUnusedNode_++];
  }

  k::DNodeId PlantedMotifGenerator::randomNode()
  {
    std::uniform_int_distribution<size_t> dist(
        0,
        nodes_.size() - 1);

    return nodes_[dist(rng_)];
  }

  k::DNodeId PlantedMotifGenerator::randomNodeOtherThan(
      k::DNodeId node)
  {
    while (true)
    {
      k::DNodeId result =
          randomNode();

      if (result != node)
        return result;
    }
  }

  void PlantedMotifGenerator::addNoise(
      k::InMemoryDatabase &db,
      k::DNodeId node)
  {
    const std::string &noiseType =
        config_.relationshipTypes.back();

    for (size_t i = 0; i < config_.noiseOutgoingPerEndpoint; ++i)
    {
      k::DNodeId other =
          randomNodeOtherThan(node);

      db.addRelationship(
          node,
          other,
          noiseType,
          {});
    }
  }

  void PlantedMotifGenerator::addBackgroundNoise(k::InMemoryDatabase &db)
  {
    const std::string &noiseType =
        config_.relationshipTypes.back();

    for (size_t i = 0; i < config_.backgroundNoiseRelationshipCount; ++i)
    {
      k::DNodeId start =
          randomNode();

      k::DNodeId end =
          randomNodeOtherThan(start);

      db.addRelationship(
          start,
          end,
          noiseType,
          {});
    }
  }

  PlantedMotifGenResult PlantedMotifGenerator::populate(
      k::InMemoryDatabase &db)
  {
    createNodes(db);

    const std::string &A = config_.relationshipTypes[0];
    const std::string &B = config_.relationshipTypes[1];
    const std::string &C = config_.relationshipTypes[2];
    const std::string &D = config_.relationshipTypes[3];
    const std::string &E = config_.relationshipTypes[4];
    const std::string &F = config_.relationshipTypes[5];
    const std::string &G = config_.relationshipTypes[6];
    const std::string &H = config_.relationshipTypes[7];

    PlantedMotifGenResult result;

    result.centralRelationships.reserve(config_.centralCandidateCount);

    result.fullMotifCenters.reserve(config_.level3Count);

    for (size_t i = 0; i < config_.centralCandidateCount; ++i)
    {
      // ---------------------------------------------------------------------
      // Level 0
      //
      // Every candidate has an A relationship.
      // ---------------------------------------------------------------------

      k::DNodeId s = allocateNode();
      k::DNodeId t = allocateNode();
      k::DRelId center = db.addRelationship(s, t, A, {});
      result.centralRelationships.push_back(center);

      /**
       * High-degree endpoints.
       *
       * These relationships use a reserved noise type, so they increase the
       * cost of lazy adjacency scans without accidentally satisfying the
       * structural query predicates.
       */
      addNoise(db, s);
      addNoise(db, t);

      // ---------------------------------------------------------------------
      // Level 1
      //
      // Required:
      //
      //   s -B-> b1
      //   s -C-> b2
      //   s -D-> b3
      //
      //   t -E-> e1
      //   t -F-> e2
      //
      // Candidates outside level1Count get four out of five.
      // ---------------------------------------------------------------------

      bool fullLevel1 = i < config_.level1Count;

      size_t missingLevel1 = i % 5;

      std::optional<k::DNodeId> b1;
      std::optional<k::DNodeId> b2;
      std::optional<k::DNodeId> b3;
      std::optional<k::DNodeId> e1;
      std::optional<k::DNodeId> e2;

      auto addLevel1Edge =
          [&](size_t index,
              k::DNodeId start,
              const std::string &type,
              std::optional<k::DNodeId> &end)
      {
        if (!fullLevel1 && missingLevel1 == index)
          return;

        end =
            allocateNode();

        db.addRelationship(
            start,
            *end,
            type,
            {});
      };

      addLevel1Edge(0, s, B, b1);
      addLevel1Edge(1, s, C, b2);
      addLevel1Edge(2, s, D, b3);
      addLevel1Edge(3, t, E, e1);
      addLevel1Edge(4, t, F, e2);

      if (!fullLevel1)
        continue;

      // ---------------------------------------------------------------------
      // Level 2
      //
      // Required:
      //
      //   b1 -G-> g1
      //   b2 -G-> g2
      //   e1 -G-> g3
      //
      // Candidates that passed level 1 but should fail level 2 receive
      // two out of these three relationships.
      // ---------------------------------------------------------------------

      bool fullLevel2 = i < config_.level2Count;

      size_t missingLevel2 = i % 3;

      std::optional<k::DNodeId> g1;
      std::optional<k::DNodeId> g2;
      std::optional<k::DNodeId> g3;

      auto addLevel2Edge =
          [&](size_t index,
              k::DNodeId start,
              std::optional<k::DNodeId> &end)
      {
        if (!fullLevel2 && missingLevel2 == index)
          return;

        end =
            allocateNode();

        db.addRelationship(
            start,
            *end,
            G,
            {});
      };

      addLevel2Edge(0, *b1, g1);
      addLevel2Edge(1, *b2, g2);
      addLevel2Edge(2, *e1, g3);

      if (!fullLevel2)
        continue;

      // ---------------------------------------------------------------------
      // Level 3
      //
      // Required:
      //
      //   g1 -H-> h1
      //   g2 -H-> h2
      //   g3 -H-> h3
      //
      // Again, level-3 decoys receive two out of three.
      // ---------------------------------------------------------------------

      bool fullLevel3 = i < config_.level3Count;

      size_t missingLevel3 = i % 3;

      auto addLevel3Edge = [&](size_t index, k::DNodeId start)
      {
        if (!fullLevel3 && missingLevel3 == index)
          return;

        k::DNodeId end = allocateNode();

        db.addRelationship(start, end, H, {});
      };

      addLevel3Edge(0, *g1);
      addLevel3Edge(1, *g2);
      addLevel3Edge(2, *g3);

      if (fullLevel3)
      {
        result.fullMotifCenters.push_back(center);
      }
    }

    addBackgroundNoise(db);

    result.queryGraphs.reserve(
        config_.queryCount);

    for (size_t i = 0; i < config_.queryCount; ++i)
    {
      result.queryGraphs.push_back(
          buildQueryGraph());
    }

    return result;
  }

  qg::QueryGraph PlantedMotifGenerator::buildQueryGraph() const
  {
    qg::QueryGraph graph;

    int64_t nextVariable = 0;

    auto createNode =
        [&]()
    {
      qg::VariableId variable{
          nextVariable++};

      graph.patternNodes.push_back(
          qg::PatternNode{
              .variable = variable,
              .inlinePredicates = {},
          });

      return variable;
    };

    auto createTypedRelationship =
        [&](qg::VariableId start,
            qg::VariableId end,
            const std::string &type)
    {
      qg::VariableId relationship{
          nextVariable++};

      qg::ExprId variableExpression =
          graph.expressions.add(
              qg::Expression{
                  .type = qg::ValueType::Relationship,
                  .data = qg::VariableReference(relationship),
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
              .inlinePredicates = {
                  typePredicate,
              },
          });

      return relationship;
    };

    const std::string &A = config_.relationshipTypes[0];
    const std::string &B = config_.relationshipTypes[1];
    const std::string &C = config_.relationshipTypes[2];
    const std::string &D = config_.relationshipTypes[3];
    const std::string &E = config_.relationshipTypes[4];
    const std::string &F = config_.relationshipTypes[5];
    const std::string &G = config_.relationshipTypes[6];
    const std::string &H = config_.relationshipTypes[7];

    // Central relationship.
    qg::VariableId s = createNode();
    qg::VariableId t = createNode();

    createTypedRelationship(
        s,
        t,
        A);

    // Level 1.
    qg::VariableId b1 = createNode();
    qg::VariableId b2 = createNode();
    qg::VariableId b3 = createNode();
    qg::VariableId e1 = createNode();
    qg::VariableId e2 = createNode();

    createTypedRelationship(s, b1, B);
    createTypedRelationship(s, b2, C);
    createTypedRelationship(s, b3, D);

    createTypedRelationship(t, e1, E);
    createTypedRelationship(t, e2, F);

    // Level 2.
    qg::VariableId g1 = createNode();
    qg::VariableId g2 = createNode();
    qg::VariableId g3 = createNode();

    createTypedRelationship(b1, g1, G);
    createTypedRelationship(b2, g2, G);
    createTypedRelationship(e1, g3, G);

    // Level 3.
    qg::VariableId h1 = createNode();
    qg::VariableId h2 = createNode();
    qg::VariableId h3 = createNode();

    createTypedRelationship(g1, h1, H);
    createTypedRelationship(g2, h2, H);
    createTypedRelationship(g3, h3, H);

    return graph;
  }

  PlantedMotifGenResult populatePlantedMotifGraph(
      k::InMemoryDatabase &db,
      PlantedMotifGenConfig config)
  {
    PlantedMotifGenerator generator{
        std::move(config)};

    return generator.populate(db);
  }

 

} // namespace evaluation