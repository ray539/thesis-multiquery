#pragma once
#include <bits/stdc++.h>
#include <common/Kernel.hpp>

namespace kernel
{

  bool RelationshipCursor::next(DRelId &out)
  {
    if (pos_ == rels_->size())
      return false;
    out = (*rels_)[pos_++];
    ++*incrementCounter_;
    return true;
  }

  void InMemoryDatabase::addNode(DNode node)
  {
    nodes_.emplace(node.id, std::move(node));
  }

  void InMemoryDatabase::addRelationship(DRelationship rel)
  {
    DRelId id = rel.id;

    outgoing_[rel.start].push_back(id);
    incoming_[rel.end].push_back(id);

    allRelationships_.push_back(id);
    relationships_.emplace(id, std::move(rel));
  }

  RelationshipCursor InMemoryDatabase::getRelationshipCursor()
  {
    ++metrics_.database.allRelationshipCursorOpens;
    return RelationshipCursor{
        allRelationships_,
        metrics_.database.allRelationshipCursorIncrements};
  }

  RelationshipCursor InMemoryDatabase::accessAdjacentRels(
      DNodeId node,
      common::Direction direction)
  {

    ++metrics_.database.adjacencyCursorOpens;

    if (direction == common::Direction::Outgoing)
    {
      return RelationshipCursor{
          outgoing_[node],
          metrics_.database.adjacencyCursorIncrements};
    }

    return RelationshipCursor{
        incoming_[node],
        metrics_.database.adjacencyCursorIncrements};
  }

  std::optional<int64_t> InMemoryDatabase::accessNodeProperty(
      DNodeId node,
      const std::string &property) const
  {
    const auto &properties = nodes_.at(node).properties;
    auto it = properties.find(property);
    if (it == properties.end())
      return std::nullopt;
    return it->second;
  }

  std::optional<int64_t> InMemoryDatabase::accessRelationshipProperty(
      DRelId rel,
      const std::string &property) const
  {

    const auto &properties = relationships_.at(rel).properties;
    auto it = properties.find(property);
    if (it == properties.end())
      return std::nullopt;
    return it->second;
  }

  DNodeId InMemoryDatabase::accessRelationshipEndpoint(
      DRelId rel,
      common::Endpoint endpoint) const
  {
    const auto &r = relationships_.at(rel);
    return endpoint == common::Endpoint::Start
               ? r.start
               : r.end;
  }

  const std::string &InMemoryDatabase::accessRelationshipType(
      DRelId rel) const
  {
    return relationships_.at(rel).relType;
  }

  bool InMemoryDatabase::accessNodeHasLabel(
      DNodeId node,
      const std::string &label) const
  {
    return nodes_.at(node).labels.contains(label);
  }

}