#pragma once
#include <bits/stdc++.h>
#include <common/Common.hpp>



template <>
struct std::hash<kernel::DRelId>
{
  size_t operator()(const kernel::DRelId &id) const noexcept
  {
    return std::hash<int64_t>{}(id.val);
  }
};

namespace kernel
{
  // for our toy backend
  struct DNode
  {
    DNodeId id;
    std::unordered_map<std::string, int64_t> properties;
    std::unordered_set<std::string> labels;
  };

  struct DRelationship
  {
    DRelId id;

    DNodeId start;
    DNodeId end;

    std::string relType;

    std::unordered_map<std::string, int64_t> properties;
  };

  struct CacheMetrics
  {
    uint64_t lookups = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;
  };

  // incremented by 'Database'
  struct CursorMetrics
  {
    uint64_t allRelationshipCursorOpens = 0;
    uint64_t allRelationshipCursorIncrements = 0;

    uint64_t adjacencyCursorOpens = 0;
    uint64_t adjacencyCursorIncrements = 0;
  };

  struct DatabaseMetrics
  {
    CursorMetrics database;

    // incremented by CacheAccess
    CacheMetrics relationshipAttributeCache;
    CacheMetrics nodeAttributeCache;
    CacheMetrics relationshipEndpointCache;
    CacheMetrics predicateCache;
  };

  /**
   * basically an iterator over `DRelId`
   */
  class RelationshipCursor
  {
  public:
    // should only be constructed in 'kernel'
    RelationshipCursor(
        const std::vector<DRelId> &rels,
        uint64_t &incrementCounter)
        : rels_{&rels},
          incrementCounter_{&incrementCounter} {}

    /**
     * increment cursor
     */
    bool next(DRelId &out);

  private:
    const std::vector<DRelId> *rels_;
    size_t pos_ = 0;

    uint64_t *incrementCounter_;
  };

  class InMemoryDatabase
  {
  public:
    explicit InMemoryDatabase(
        DatabaseMetrics &metrics)
        : metrics_{metrics}
    {
    }

    void addNode(DNode node);

    void addRelationship(DRelationship rel);

    // cursor to relationships
    RelationshipCursor getRelationshipCursor();

    RelationshipCursor accessAdjacentRels(
        DNodeId node,
        common::Direction direction);

    std::optional<int64_t> accessNodeProperty(
        DNodeId node,
        const std::string &property) const;

    std::optional<int64_t> accessRelationshipProperty(
        DRelId rel,
        const std::string &property) const;

    DNodeId accessRelationshipEndpoint(
        DRelId rel,
        common::Endpoint endpoint) const;

    const std::string &accessRelationshipType(
        DRelId rel) const;

    bool accessNodeHasLabel(
        DNodeId node,
        const std::string &label) const;

  private:
    DatabaseMetrics &metrics_;

    std::unordered_map<DNodeId, DNode> nodes_;
    std::unordered_map<DRelId, DRelationship> relationships_;

    std::vector<DRelId> allRelationships_;

    std::unordered_map<DNodeId, std::vector<DRelId>> outgoing_;
    std::unordered_map<DNodeId, std::vector<DRelId>> incoming_;
  };

  struct DNodePropertyKey
  {
    kernel::DNodeId node;
    std::string property;

    bool operator==(const DNodePropertyKey &) const = default;
  };

  struct DRelPropertyKey
  {
    kernel::DRelId rel;
    std::string property;

    bool operator==(const DRelPropertyKey &) const = default;
  };

  struct DRelEndpointKey
  {
    kernel::DRelId rel;
    common::Endpoint endpoint;

    bool operator==(const DRelEndpointKey &) const = default;
  };

}

template <>
struct std::hash<kernel::DNodePropertyKey>
{
  size_t operator()(const kernel::DNodePropertyKey &x) const noexcept
  {
    size_t h = std::hash<kernel::DNodeId>{}(x.node);
    common::hashCombine(h, x.property);
    return h;
  }
};

template <>
struct std::hash<kernel::DRelPropertyKey>
{
  size_t operator()(const kernel::DRelPropertyKey &x) const noexcept
  {
    size_t h = std::hash<kernel::DRelId>{}(x.rel);
    common::hashCombine(h, x.property);
    return h;
  }
};

template <>
struct std::hash<kernel::DRelEndpointKey>
{
  size_t operator()(const kernel::DRelEndpointKey &x) const noexcept
  {
    size_t h = std::hash<kernel::DRelId>{}(x.rel);
    common::hashCombine(h, static_cast<int>(x.endpoint));
    return h;
  }
};

namespace kernel
{
  /**
   * cached layer with metrics tracking, over InMemoryDatabase operations
   */
  class CachedDatabaseAccess
  {
  public:
    CachedDatabaseAccess(
        kernel::InMemoryDatabase &db,
        kernel::DatabaseMetrics &metrics)
        : db_{db},
          metrics_{metrics}
    {
    }

    RelationshipCursor getRelationshipCursor();

    RelationshipCursor accessAdjacentRels(DNodeId node, common::Direction direction);

    std::optional<int64_t> nodeProperty(DNodeId node, const std::string &property);

    std::optional<int64_t> relationshipProperty(DRelId rel, const std::string &property);

    DNodeId endpoint(DRelId rel, common::Endpoint endpoint);

    /**
     * for metric recording purposes, accessing a relationship type is treated
     * the same as accessing a relationship attribute
     */
    const std::string &relationshipType(DRelId rel);

    /**
     * for metric recording purposes, accessing a node label is treated
     * the same as accessing a node attribute
     */
    bool nodeHasLabel(
        DNodeId node,
        const std::string &label);

    /**
     * returns cursor. Metrics records number of opens
     */
    RelationshipCursor adjacent(
        DNodeId node,
        common::Direction direction);


    /**
     * reference to database metrics object within
     */
    kernel::DatabaseMetrics& metrics() {
      return metrics_;
    }

  private:
    kernel::InMemoryDatabase &db_;
    kernel::DatabaseMetrics &metrics_;

    // (dnodeId, string) -> val
    std::unordered_map<
        DNodePropertyKey,
        std::optional<int64_t>>
        nodePropertyCache_;

    // (drelId, string) -> val
    std::unordered_map<
        DRelPropertyKey,
        std::optional<int64_t>>
        relPropertyCache_;

    // (drelId, start|end) -> DNodeId
    std::unordered_map<
        DRelEndpointKey,
        kernel::DNodeId>
        endpointCache_;

    // drelId -> string (rel. label)
    std::unordered_map<
        kernel::DRelId,
        std::string>
        relTypeCache_;

    // (dnodeId, string) -> bool (haslabel?)
    std::unordered_map<
        DNodePropertyKey,
        bool>
        nodeLabelCache_;
  };
}
