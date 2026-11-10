#include <bits/stdc++.h>

#include <common/Kernel.hpp>
#include <common/Common.hpp>

namespace kernel
{
  namespace
  {
    std::string indent(const std::string &text, size_t spaces)
    {
      std::string prefix(spaces, ' ');
      std::stringstream out;

      std::stringstream input(text);
      std::string line;

      bool first = true;

      while (std::getline(input, line))
      {
        if (!first)
          out << '\n';

        out << prefix << line;
        first = false;
      }

      return out.str();
    }

    std::string endpointToString(common::Endpoint endpoint)
    {
      switch (endpoint)
      {
      case common::Endpoint::Start:
        return "Start";

      case common::Endpoint::End:
        return "End";
      }

      return "?";
    }

    template <typename T>
    std::vector<T> sortedStrongIds(
        const auto &container)
    {
      std::vector<T> result;

      for (const auto &[id, _] : container)
        result.push_back(id);

      std::sort(
          result.begin(),
          result.end(),
          [](T lhs, T rhs)
          {
            return lhs.val < rhs.val;
          });

      return result;
    }

    std::string propertiesToString(
        const std::unordered_map<std::string, int64_t> &properties)
    {
      std::vector<std::pair<std::string, int64_t>> sorted(
          properties.begin(),
          properties.end());

      std::sort(
          sorted.begin(),
          sorted.end(),
          [](const auto &lhs, const auto &rhs)
          {
            return lhs.first < rhs.first;
          });

      std::stringstream out;

      out << "{";

      for (size_t i = 0; i < sorted.size(); ++i)
      {
        if (i != 0)
          out << ", ";

        out << sorted[i].first
            << ": "
            << sorted[i].second;
      }

      out << "}";

      return out.str();
    }

    std::string labelsToString(
        const std::unordered_set<std::string> &labels)
    {
      std::vector<std::string> sorted(
          labels.begin(),
          labels.end());

      std::sort(
          sorted.begin(),
          sorted.end());

      std::stringstream out;

      out << "[";

      for (size_t i = 0; i < sorted.size(); ++i)
      {
        if (i != 0)
          out << ", ";

        out << sorted[i];
      }

      out << "]";

      return out.str();
    }

    std::string relListToString(
        const std::vector<DRelId> &rels)
    {
      std::stringstream out;

      out << "[";

      for (size_t i = 0; i < rels.size(); ++i)
      {
        if (i != 0)
          out << ", ";

        out << to_string(rels[i]);
      }

      out << "]";

      return out.str();
    }

    std::string cacheMetricsLine(
        const CacheMetrics &metrics)
    {
      std::stringstream out;

      out << "lookups=" << metrics.lookups
          << ", hits=" << metrics.hits
          << ", misses=" << metrics.misses;

      if (metrics.lookups != 0)
      {
        long double hitRate =
            static_cast<long double>(metrics.hits) /
            static_cast<long double>(metrics.lookups);

        out << ", hitRate="
            << std::fixed
            << std::setprecision(3)
            << static_cast<double>(hitRate);
      }

      return out.str();
    }
  }

  // ---------------------------------------------------------------------------
  // DNode
  // ---------------------------------------------------------------------------

  std::string to_string(const DNode &node)
  {
    std::stringstream out;

    out << to_string(node.id)
        << " {\n";

    out << "  labels: "
        << labelsToString(node.labels)
        << "\n";

    out << "  properties: "
        << propertiesToString(node.properties)
        << "\n";

    out << "}";

    return out.str();
  }

  // ---------------------------------------------------------------------------
  // DRelationship
  // ---------------------------------------------------------------------------

  std::string to_string(const DRelationship &rel)
  {
    std::stringstream out;

    out << to_string(rel.id)
        << " {\n";

    out << "  "
        << to_string(rel.start)
        << " -[:"
        << rel.relType
        << "]-> "
        << to_string(rel.end)
        << "\n";

    out << "  properties: "
        << propertiesToString(rel.properties)
        << "\n";

    out << "}";

    return out.str();
  }

  // ---------------------------------------------------------------------------
  // Metrics
  // ---------------------------------------------------------------------------

  std::string to_string(const CacheMetrics &metrics)
  {
    return cacheMetricsLine(metrics);
  }

  std::string to_string(const CursorMetrics &metrics)
  {
    std::stringstream out;

    out << "CursorMetrics {\n";

    out << "  allRelationshipCursorOpens: "
        << metrics.allRelationshipCursorOpens
        << "\n";

    out << "  allRelationshipCursorIncrements: "
        << metrics.allRelationshipCursorIncrements
        << "\n";

    out << "  adjacencyCursorOpens: "
        << metrics.adjacencyCursorOpens
        << "\n";

    out << "  adjacencyCursorIncrements: "
        << metrics.adjacencyCursorIncrements
        << "\n";

    out << "}";

    return out.str();
  }

  std::string to_string(const DatabaseMetrics &metrics)
  {
    std::stringstream out;

    out << "DatabaseMetrics {\n";

    out << "  database:\n"
        << indent(to_string(metrics.database), 4)
        << "\n";

    out << "  relationshipAttributeCache: "
        << to_string(metrics.relationshipAttributeCache)
        << "\n";

    out << "  nodeAttributeCache: "
        << to_string(metrics.nodeAttributeCache)
        << "\n";

    out << "  relationshipEndpointCache: "
        << to_string(metrics.relationshipEndpointCache)
        << "\n";

    out << "  predicateCache: "
        << to_string(metrics.predicateCache)
        << "\n";

    out << "}";

    return out.str();
  }

  // ---------------------------------------------------------------------------
  // Cache keys
  // ---------------------------------------------------------------------------

  std::string to_string(const DNodePropertyKey &key)
  {
    std::stringstream out;

    out << "("
        << to_string(key.node)
        << ", "
        << key.property
        << ")";

    return out.str();
  }

  std::string to_string(const DRelPropertyKey &key)
  {
    std::stringstream out;

    out << "("
        << to_string(key.rel)
        << ", "
        << key.property
        << ")";

    return out.str();
  }

  std::string to_string(const DRelEndpointKey &key)
  {
    std::stringstream out;

    out << "("
        << to_string(key.rel)
        << ", "
        << endpointToString(key.endpoint)
        << ")";

    return out.str();
  }

  // ---------------------------------------------------------------------------
  // InMemoryDatabase
  // ---------------------------------------------------------------------------

  std::string to_string(const InMemoryDatabase &db)
  {
    std::stringstream out;

    out << "InMemoryDatabase {\n";

    // -------------------------------------------------------------------------
    // Nodes
    // -------------------------------------------------------------------------

    out << "  nodes (" << db.nodes_.size() << "):\n";

    auto nodeIds =
        sortedStrongIds<DNodeId>(
            db.nodes_);

    for (DNodeId id : nodeIds)
    {
      out << indent(
                 to_string(db.nodes_.at(id)),
                 4)
          << "\n";
    }

    // -------------------------------------------------------------------------
    // Relationships
    // -------------------------------------------------------------------------

    out << "  relationships ("
        << db.relationships_.size()
        << "):\n";

    auto relIds =
        sortedStrongIds<DRelId>(
            db.relationships_);

    for (DRelId id : relIds)
    {
      out << indent(
                 to_string(db.relationships_.at(id)),
                 4)
          << "\n";
    }

    // -------------------------------------------------------------------------
    // allRelationships
    // -------------------------------------------------------------------------

    out << "  allRelationships: "
        << relListToString(db.allRelationships_)
        << "\n";

    // -------------------------------------------------------------------------
    // Outgoing index
    // -------------------------------------------------------------------------

    out << "  outgoing:\n";

    auto outgoingNodes =
        sortedStrongIds<DNodeId>(
            db.outgoing_);

    for (DNodeId node : outgoingNodes)
    {
      out << "    "
          << to_string(node)
          << ": "
          << relListToString(db.outgoing_.at(node))
          << "\n";
    }

    // -------------------------------------------------------------------------
    // Incoming index
    // -------------------------------------------------------------------------

    out << "  incoming:\n";

    auto incomingNodes =
        sortedStrongIds<DNodeId>(
            db.incoming_);

    for (DNodeId node : incomingNodes)
    {
      out << "    "
          << to_string(node)
          << ": "
          << relListToString(db.incoming_.at(node))
          << "\n";
    }

    // -------------------------------------------------------------------------
    // Metrics
    // -------------------------------------------------------------------------

    out << "  metrics:\n"
        << indent(
               to_string(db.metrics_),
               4)
        << "\n";

    out << "}";

    return out.str();
  }

  // ---------------------------------------------------------------------------
  // CachedDatabaseAccess
  // ---------------------------------------------------------------------------

  std::string to_string(const CachedDatabaseAccess &db)
  {
    std::stringstream out;

    out << "CachedDatabaseAccess {\n";

    // -------------------------------------------------------------------------
    // Node property cache
    // -------------------------------------------------------------------------

    out << "  nodePropertyCache ("
        << db.nodePropertyCache_.size()
        << "):\n";

    {
      std::vector<std::pair<DNodePropertyKey, std::optional<int64_t>>> entries(
          db.nodePropertyCache_.begin(),
          db.nodePropertyCache_.end());

      std::sort(
          entries.begin(),
          entries.end(),
          [](const auto &lhs, const auto &rhs)
          {
            if (lhs.first.node.val != rhs.first.node.val)
              return lhs.first.node.val < rhs.first.node.val;

            return lhs.first.property < rhs.first.property;
          });

      for (const auto &[key, value] : entries)
      {
        out << "    "
            << to_string(key)
            << " -> ";

        if (value)
          out << *value;
        else
          out << "nullopt";

        out << "\n";
      }
    }

    // -------------------------------------------------------------------------
    // Relationship property cache
    // -------------------------------------------------------------------------

    out << "  relPropertyCache ("
        << db.relPropertyCache_.size()
        << "):\n";

    {
      std::vector<std::pair<DRelPropertyKey, std::optional<int64_t>>> entries(
          db.relPropertyCache_.begin(),
          db.relPropertyCache_.end());

      std::sort(
          entries.begin(),
          entries.end(),
          [](const auto &lhs, const auto &rhs)
          {
            if (lhs.first.rel.val != rhs.first.rel.val)
              return lhs.first.rel.val < rhs.first.rel.val;

            return lhs.first.property < rhs.first.property;
          });

      for (const auto &[key, value] : entries)
      {
        out << "    "
            << to_string(key)
            << " -> ";

        if (value)
          out << *value;
        else
          out << "nullopt";

        out << "\n";
      }
    }

    // -------------------------------------------------------------------------
    // Endpoint cache
    // -------------------------------------------------------------------------

    out << "  endpointCache ("
        << db.endpointCache_.size()
        << "):\n";

    {
      std::vector<std::pair<DRelEndpointKey, DNodeId>> entries(
          db.endpointCache_.begin(),
          db.endpointCache_.end());

      std::sort(
          entries.begin(),
          entries.end(),
          [](const auto &lhs, const auto &rhs)
          {
            if (lhs.first.rel.val != rhs.first.rel.val)
              return lhs.first.rel.val < rhs.first.rel.val;

            return static_cast<int>(lhs.first.endpoint) <
                   static_cast<int>(rhs.first.endpoint);
          });

      for (const auto &[key, value] : entries)
      {
        out << "    "
            << to_string(key)
            << " -> "
            << to_string(value)
            << "\n";
      }
    }

    // -------------------------------------------------------------------------
    // Relationship type cache
    // -------------------------------------------------------------------------

    out << "  relTypeCache ("
        << db.relTypeCache_.size()
        << "):\n";

    {
      std::vector<std::pair<DRelId, std::string>> entries(
          db.relTypeCache_.begin(),
          db.relTypeCache_.end());

      std::sort(
          entries.begin(),
          entries.end(),
          [](const auto &lhs, const auto &rhs)
          {
            return lhs.first.val < rhs.first.val;
          });

      for (const auto &[rel, type] : entries)
      {
        out << "    "
            << to_string(rel)
            << " -> "
            << type
            << "\n";
      }
    }

    // -------------------------------------------------------------------------
    // Node label cache
    // -------------------------------------------------------------------------

    out << "  nodeLabelCache ("
        << db.nodeLabelCache_.size()
        << "):\n";

    {
      std::vector<std::pair<DNodePropertyKey, bool>> entries(
          db.nodeLabelCache_.begin(),
          db.nodeLabelCache_.end());

      std::sort(
          entries.begin(),
          entries.end(),
          [](const auto &lhs, const auto &rhs)
          {
            if (lhs.first.node.val != rhs.first.node.val)
              return lhs.first.node.val < rhs.first.node.val;

            return lhs.first.property < rhs.first.property;
          });

      for (const auto &[key, value] : entries)
      {
        out << "    "
            << to_string(key)
            << " -> "
            << std::boolalpha
            << value
            << "\n";
      }
    }

    // -------------------------------------------------------------------------
    // Metrics
    // -------------------------------------------------------------------------

    out << "  metrics:\n"
        << indent(
               to_string(db.metrics_),
               4)
        << "\n";

    out << "}";

    return out.str();
  }

} // namespace kernel