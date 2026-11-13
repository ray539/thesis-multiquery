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

  DNodeId InMemoryDatabase::addNode(std::unordered_map<std::string, int64_t> properties, std::unordered_set<std::string> labels)
  {
    DNode new_node {
      .id=DNodeId(next_node_id_),
      .properties=properties,
      .labels=labels
    };
    nodes_.emplace(new_node.id, std::move(new_node));
    next_node_id_++;
    return new_node.id;
  }

  DRelId InMemoryDatabase::addRelationship(DNodeId start, DNodeId end, std::string relType, std::unordered_map<std::string, int64_t> properties)
  {
    DRelationship new_rel {
      .id=DRelId(next_rel_id_),
      .start=start,
      .end=end,
      .relType=relType,
      .properties=properties
    };

    outgoing_[new_rel.start].push_back(new_rel.id);
    incoming_[new_rel.end].push_back(new_rel.id);

    allRelationships_.push_back(new_rel.id);
    relationships_.emplace(new_rel.id, std::move(new_rel));
    next_rel_id_++;
    return new_rel.id;
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

  size_t InMemoryDatabase::nodeDegree(DNodeId node, common::Direction direction) {
    if (direction == common::Direction::Outgoing) {
      // we use [] because if the node is lone, we still want to create the empty list
      return outgoing_[node].size();
    }
    return incoming_[node].size();
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