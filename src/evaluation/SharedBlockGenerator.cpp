#pragma once

#include <algorithm>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <common/Evaluation.hpp>
#include <common/QueryGraph.hpp>
#include <common/Kernel.hpp>


namespace evaluation
{
  namespace k = kernel;
  namespace qg = query_graph;


  class SharedBlockStressGenerator
  {
  public:
    explicit SharedBlockStressGenerator(
        SharedBlockStressConfig config)
        : config_{config},
          rng_{config.seed}
    {
      validateConfig();
    }

    SharedBlockStressResult populate(k::InMemoryDatabase &db)
    {
      SharedBlockStressResult result;

      result.centralNodes.reserve(config_.centralCount);
      result.boundaryRelationships.resize(config_.centralCount);
      std::vector<k::DNodeId> structuralNodes;

      // ---------------------------------------------------------------------
      // Plant the actual query motifs.
      // ---------------------------------------------------------------------

      for (size_t c = 0; c < config_.centralCount; ++c)
      {
        k::DNodeId center = db.addNode({}, {});
        result.centralNodes.push_back(center);
        structuralNodes.push_back(center);
        auto &boundaries = result.boundaryRelationships[c];

        boundaries.reserve(config_.blockCount);

        for (size_t b = 0; b < config_.blockCount; ++b)
        {
          /**
           * Boundary node of this block.
           *
           * All planted internal embeddings for this block share this node
           * and therefore share the same boundary relationship.
           */
          k::DNodeId boundaryNode = db.addNode({}, {});
          structuralNodes.push_back(boundaryNode);

          k::DRelId boundaryRelationship =
              db.addRelationship(
                  boundaryNode,
                  center,
                  "R",
                  {});

          boundaries.push_back(
              boundaryRelationship);

          // ---------------------------------------------------------------
          // Plant multiple distinct 4-cycles around the same boundary.
          //
          //       x
          //      ^ \
          //     /   \
          //    b     y
          //     ^   /
          //      \ /
          //       z
          //
          // b -> x -> y -> z -> b
          //
          // Different embeddings share only b.
          // ---------------------------------------------------------------

          for (size_t e = 0;
               e < config_.embeddingsPerBlock;
               ++e)
          {
            k::DNodeId x =
                db.addNode({}, {});

            k::DNodeId y =
                db.addNode({}, {});

            k::DNodeId z =
                db.addNode({}, {});

            structuralNodes.push_back(x);
            structuralNodes.push_back(y);
            structuralNodes.push_back(z);

            db.addRelationship(
                boundaryNode,
                x,
                "R",
                {});

            db.addRelationship(
                x,
                y,
                "R",
                {});

            db.addRelationship(
                y,
                z,
                "R",
                {});

            db.addRelationship(
                z,
                boundaryNode,
                "R",
                {});
          }
        }
      }

      // ---------------------------------------------------------------------
      // Add homogeneous R noise.
      //
      // Each structural node receives:
      //
      //     noiseDegree incoming R relationships
      //     noiseDegree outgoing R relationships
      //
      // Noise endpoints come from two separate pools:
      //
      //     incomingNoise -> structural node
      //     structural node -> outgoingNoise
      //
      // This increases local degree without accidentally constructing extra
      // directed cycles through the noise nodes.
      // ---------------------------------------------------------------------

      addNoise(
          db,
          structuralNodes);

      // ---------------------------------------------------------------------
      // Generate the two identical query graphs.
      // ---------------------------------------------------------------------

      result.queryGraphs.reserve(2);

      result.queryGraphs.push_back(
          buildQueryGraph(false));

      result.queryGraphs.push_back(
          buildQueryGraph(true));

      return result;
    }

  private:
    SharedBlockStressConfig config_;
    std::mt19937_64 rng_;

    friend qg::QueryGraph buildQG(bool sharedBlocks);

    void validateConfig() const
    {
      if (config_.centralCount == 0)
      {
        throw std::invalid_argument(
            "SharedBlockStressGenerator: centralCount must be positive");
      }

      if (config_.blockCount == 0)
      {
        throw std::invalid_argument(
            "SharedBlockStressGenerator: blockCount must be positive");
      }

      if (config_.embeddingsPerBlock == 0)
      {
        throw std::invalid_argument(
            "SharedBlockStressGenerator: embeddingsPerBlock must be positive");
      }
    }

    void addNoise(
        k::InMemoryDatabase &db,
        const std::vector<k::DNodeId> &structuralNodes)
    {
      if (config_.noiseDegree == 0)
        return;

      /**
       * Reuse noise endpoints instead of allocating one leaf per edge.
       *
       * The two pools are kept disjoint:
       *
       *   incoming pool:
       *       noise -> structural
       *
       *   outgoing pool:
       *       structural -> noise
       *
       * Therefore a noise node cannot become an intermediate node in one of
       * the planted directed 4-cycles.
       */
      size_t poolSize =
          std::max<size_t>(
              32,
              structuralNodes.size());

      std::vector<k::DNodeId> incomingPool;
      std::vector<k::DNodeId> outgoingPool;

      incomingPool.reserve(poolSize);
      outgoingPool.reserve(poolSize);

      for (size_t i = 0; i < poolSize; ++i)
      {
        incomingPool.push_back(
            db.addNode({}, {}));

        outgoingPool.push_back(
            db.addNode({}, {}));
      }

      std::uniform_int_distribution<size_t> incomingDist(
          0,
          incomingPool.size() - 1);

      std::uniform_int_distribution<size_t> outgoingDist(
          0,
          outgoingPool.size() - 1);

      for (k::DNodeId node : structuralNodes)
      {
        for (size_t i = 0; i < config_.noiseDegree; ++i)
        {
          k::DNodeId incoming =
              incomingPool[incomingDist(rng_)];

          k::DNodeId outgoing =
              outgoingPool[outgoingDist(rng_)];

          db.addRelationship(
              incoming,
              node,
              "R",
              {});

          db.addRelationship(
              node,
              outgoing,
              "R",
              {});
        }
      }
    }

    qg::QueryGraph buildQueryGraph(bool markSharedBlocks) const
    {
      qg::QueryGraph graph;

      int64_t nextVariable = 0;

      auto createNode = [&]()
      {
        qg::VariableId variable{nextVariable++};

        graph.patternNodes.push_back(
            qg::PatternNode{
                .variable = variable,
                .inlinePredicates = {},
            });

        return variable;
      };

      auto createRelationship =
          [&](qg::VariableId start,
              qg::VariableId end)
      {
        qg::VariableId relationship{
            nextVariable++};

        qg::ExprId relationshipExpression =
            graph.expressions.add(
                qg::Expression{
                    .type = qg::ValueType::Relationship,
                    .data = qg::VariableReference(relationship),
                });

        qg::ExprId typePredicate =
            graph.expressions.add(
                qg::Expression{
                    .type = qg::ValueType::Bool,
                    .data = qg::HasRelType(relationshipExpression,"R")
                    }
                );

        graph.patternRelationships.push_back(
            qg::PatternRelationship{
                .variable = relationship,
                .startNode = start,
                .endNode = end,
                .direction =
                    common::Direction::Outgoing,
                .inlinePredicates = {
                    typePredicate,
                },
            });

        return relationship;
      };

      // ---------------------------------------------------------------------
      // Shared central node.
      // ---------------------------------------------------------------------

      qg::VariableId center = createNode();

      // ---------------------------------------------------------------------
      // Attach blockCount independent 4-cycles.
      // ---------------------------------------------------------------------

      for (size_t b = 0; b < config_.blockCount; ++b)
      {
        qg::VariableId boundaryNode =
            createNode();

        qg::VariableId x =
            createNode();

        qg::VariableId y =
            createNode();

        qg::VariableId z =
            createNode();

        // Directed cycle:
        //
        // boundary -> x -> y -> z -> boundary

        qg::VariableId r0 =
            createRelationship(
                boundaryNode,
                x);

        qg::VariableId r1 =
            createRelationship(
                x,
                y);

        qg::VariableId r2 =
            createRelationship(
                y,
                z);

        qg::VariableId r3 =
            createRelationship(
                z,
                boundaryNode);

        // Boundary relationship:
        //
        // boundary -> center

        qg::VariableId boundaryRelationship =
            createRelationship(
                boundaryNode,
                center);

        if (markSharedBlocks)
        {
          graph.sharedBlocks.push_back(
              qg::SharedBlock{
                  .block_id =
                      qg::BlockId{
                          static_cast<int64_t>(b),
                      },
                  .patternRelationshipIDs = {
                      r0,
                      r1,
                      r2,
                      r3,
                      boundaryRelationship,
                  },
              });
        }
      }

      return graph;
    }
  };

  qg::QueryGraph buildQG(bool sharedBlocks) {
    SharedBlockStressGenerator generator{{}};
    return generator.buildQueryGraph(sharedBlocks);
  }

  SharedBlockStressResult populateSharedBlockStressGraph(
      k::InMemoryDatabase &db,
      SharedBlockStressConfig config)
  {
    SharedBlockStressGenerator generator{
        config};

    return generator.populate(db);
  }

} // namespace evaluation