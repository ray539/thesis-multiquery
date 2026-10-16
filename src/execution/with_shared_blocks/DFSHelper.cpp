#include <common/Execution.hpp>

namespace execution
{
  namespace dfs_helper
  {

    bool areAdjacent(
        kernel::CachedDatabaseAccess &db,
        kernel::DRelId mine,
        kernel::DRelId theirs,
        common::Adjacency adjacency)
    {
      if (adjacency == common::Adjacency::BoundaryToSB)
      {
        throw std::logic_error("BoundaryToSB is not a data-graph adjacency constraint");
      }
      kernel::DNodeId mineNode = db.endpoint(mine, common::myEndpoint(adjacency));
      kernel::DNodeId theirNode = db.endpoint(theirs, common::neighborEndpoint(adjacency));
      return mineNode == theirNode;
    }

    /**
     * not Adjacency::BoundaryToSB
     */
    bool isOrdinaryAdjacency(common::Adjacency adjacency)
    {
      return adjacency != common::Adjacency::BoundaryToSB;
    }

    common::Direction directionForEndpoint(common::Endpoint endpoint)
    {
      return endpoint == common::Endpoint::Start
                 ? common::Direction::Outgoing
                 : common::Direction::Incoming;
    }

    std::vector<BoundConstraint> SBBoundConstraints(
        const ib::SharedBlockIR::EdgeGraphSB &edgeGraph,
        ib::SharedBlockIR::EIdx current,

        const std::unordered_map<
            ib::SharedBlockIR::EIdx,
            k::DRelId,
            IntStrongHash> &assignment)
    {
      std::vector<BoundConstraint> result;

      // (collapsed_idx, adjtype) set just in case there are duplicate edge graph edges
      std::set<std::pair<ib::SharedBlockIR::EIdx, int>> seen;
      for (const auto &edge : edgeGraph.adj(current))
      {
        if (!isOrdinaryAdjacency(edge.adjType))
          continue;

        if (!assignment.contains(edge.other))
          continue;

        if (!seen.insert({edge.other, (int)edge.adjType}).second)
        {
          // duplicate edge
          continue;
        }
        result.push_back(BoundConstraint{
            .adjacency = edge.adjType,
            .otherDRel = assignment.at(edge.other)});
      }
      return result;
    }

    std::vector<BoundConstraint> regularBoundConstraints(
        const ib::collapsed::EdgeGraphCollapsed &edgeGraph,
        ib::collapsed::EIdx current,
        const Embedding &embedding,
        const std::unordered_set<
            ib::collapsed::EIdx,
            IntStrongHash> &processed)
    {
      std::vector<BoundConstraint> result;

      // (collapsed_idx, adjtype) set just in case there are duplicate edge graph edges
      std::set<std::pair<ib::collapsed::EIdx, int>> seen;
      for (const auto &edge : edgeGraph.adj(current))
      {
        if (!isOrdinaryAdjacency(edge.adjType))
          continue;

        if (!processed.contains(edge.other))
          continue;

        auto otherNode = edgeGraph.node_at(edge.other);
        const auto *otherRel = std::get_if<ib::PatternRelationshipIR>(&otherNode);
        if (!otherRel)
          continue;

        if (!embedding.relationships.contains(otherRel->lv))
          continue;

        if (!seen.insert({edge.other, (int)edge.adjType}).second)
        {
          // duplicate edge
          continue;
        }
        result.push_back(BoundConstraint{
            .adjacency = edge.adjType,
            .otherDRel = embedding.relationships.at(otherRel->lv)});
      }
      return result;
    }

    k::RelationshipCursor regularCandidateCursor(
        const std::vector<BoundConstraint> &constraints,
        k::CachedDatabaseAccess &cached_database)
    {
      if (constraints.empty())
        return cached_database.getRelationshipCursor();
      const BoundConstraint &source = constraints.front();

      k::DNodeId sharedNode = cached_database.endpoint(source.otherDRel, common::neighborEndpoint(source.adjacency));

      common::Endpoint myEndpoint = common::myEndpoint(source.adjacency);
      return cached_database.adjacent(sharedNode, directionForEndpoint(myEndpoint));
    }

    bool satisfiesConstraints(
        k::DRelId candidate,
        const std::vector<BoundConstraint> &constraints,
        k::CachedDatabaseAccess &cached_database)
    {
      for (const auto &constraint : constraints)
      {
        if (!areAdjacent(
                cached_database,
                candidate,
                constraint.otherDRel,
                constraint.adjacency))
        {
          return false;
        }
      }

      return true;
    }

    std::optional<size_t> indexWhichArgmaxes(
        std::vector<size_t> indices,
        std::function<bool(size_t)> &&filter,
        std::function<int64_t(size_t)> &&scorer)
    {
      std::optional<size_t> best;
      int64_t best_score;
      for (size_t i : indices)
      {
        if (!filter(i))
          continue;
        int64_t this_score = scorer(i);
        if (!best || this_score > best_score)
        {
          best = i;
          best_score = this_score;
        }
      }
      return best;
    }

  } // namespace dfs_helper
} // namespace execution