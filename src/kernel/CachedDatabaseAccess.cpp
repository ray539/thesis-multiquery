#include <bits/stdc++.h>
#include <common/Kernel.hpp>

namespace kernel
{

  RelationshipCursor CachedDatabaseAccess::getRelationshipCursor() {
    return db_.getRelationshipCursor();
  }

  RelationshipCursor CachedDatabaseAccess::accessAdjacentRels(DNodeId node, common::Direction direction) {
    return db_.accessAdjacentRels(node, direction);
  }

  std::optional<int64_t> CachedDatabaseAccess::nodeProperty(DNodeId node, const std::string &property)
  {
    auto key = DNodePropertyKey{node, property};
    ++metrics_.nodeAttributeCache.lookups;
    if (auto it = nodePropertyCache_.find(key); it != nodePropertyCache_.end())
    {
      ++metrics_.nodeAttributeCache.hits;
      return it->second;
    }

    ++metrics_.nodeAttributeCache.misses;
    auto result = db_.accessNodeProperty(node, property);
    nodePropertyCache_.emplace(std::move(key), result);
    return result;
  }

  std::optional<int64_t> CachedDatabaseAccess::relationshipProperty(DRelId rel, const std::string &property)
  {
    auto key = DRelPropertyKey{rel, property};
    ++metrics_.relationshipAttributeCache.lookups;

    if (auto it = relPropertyCache_.find(key); it != relPropertyCache_.end())
    {
      ++metrics_.relationshipAttributeCache.hits;
      return it->second;
    }
    ++metrics_.relationshipAttributeCache.misses;
    auto result = db_.accessRelationshipProperty(rel, property);
    relPropertyCache_.emplace(std::move(key), result);
    return result;
  }

  DNodeId CachedDatabaseAccess::endpoint(DRelId rel, common::Endpoint endpoint)
  {
    auto key = DRelEndpointKey{rel, endpoint};
    ++metrics_.relationshipEndpointCache.lookups;

    if (auto it = endpointCache_.find(key); it != endpointCache_.end())
    {
      ++metrics_.relationshipEndpointCache.hits;
      return it->second;
    }

    ++metrics_.relationshipEndpointCache.misses;

    DNodeId result = db_.accessRelationshipEndpoint(
        rel,
        endpoint);

    endpointCache_.emplace(key, result);

    return result;
  }


  const std::string &CachedDatabaseAccess::relationshipType(DRelId rel)
  {
    ++metrics_.relationshipAttributeCache.lookups;
    if (auto it = relTypeCache_.find(rel); it != relTypeCache_.end())
    {
      ++metrics_.relationshipAttributeCache.hits;
      return it->second;
    }

    ++metrics_.relationshipAttributeCache.misses;
    auto [it, _] = relTypeCache_.emplace(rel, db_.accessRelationshipType(rel));
    return it->second;
  }

  bool CachedDatabaseAccess::nodeHasLabel(
      DNodeId node,
      const std::string &label)
  {

    auto key = DNodePropertyKey{node, label};
    ++metrics_.nodeAttributeCache.lookups;
    if (auto it = nodeLabelCache_.find(key); it != nodeLabelCache_.end())
    {
      ++metrics_.nodeAttributeCache.hits;
      return it->second;
    }

    ++metrics_.nodeAttributeCache.misses;
    bool result = db_.accessNodeHasLabel(node, label);
    nodeLabelCache_.emplace(std::move(key), result);

    return result;
  }

  RelationshipCursor CachedDatabaseAccess::adjacent(
    DNodeId node,
    common::Direction direction
  ) {
    return db_.accessAdjacentRels(node, direction);
  }



}