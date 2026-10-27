#include <common/IRBuilder.hpp>

namespace ir_builder
{
  namespace
  {
    std::string adjacencyListToString(
        const SharedBlockIR::EdgeGraphSB &edgeGraph,
        SharedBlockIR::EIdx from)
    {
      std::stringstream out;

      out << "[";

      const auto edges = edgeGraph.adj(from);

      for (size_t i = 0; i < edges.size(); ++i)
      {
        if (i != 0)
          out << ", ";

        out << "{type=" << common::to_string(edges[i].adjType)
            << ", other=" << ir_builder::to_string(edges[i].other) << "}";
      }

      out << "]";

      return out.str();
    }

    std::string adjacencyListToString(const EdgeGraphCollapsed &edgeGraph, EIdxCollapsed from)
    {
      std::stringstream out;

      out << "[";

      const auto edges = edgeGraph.adj(from);

      for (size_t i = 0; i < edges.size(); ++i)
      {
        if (i != 0)
          out << ", ";

        out << "{type=" << common::to_string(edges[i].adjType)
            << ", other=" << ir_builder::to_string(edges[i].other) << "}";
      }

      out << "]";

      return out.str();
    }
  } // namespace

  // ===========================================================================
  // Atomic relationship predicates
  // ===========================================================================

  std::string to_string(const RelPropertyEquals &predicate)
  {
    std::stringstream out;

    out << "RelPropertyEquals{prop=" << quoted(predicate.prop) << ", val=" << predicate.val << "}";

    return out.str();
  }

  std::string to_string(const RelTypeEquals &predicate)
  {
    std::stringstream out;

    out << "RelTypeEquals{relType=" << quoted(predicate.relType) << "}";

    return out.str();
  }

  std::string to_string(const EndpointPropertyEquals &predicate)
  {
    std::stringstream out;

    out << "EndpointPropertyEquals{endpoint=" << common::to_string(predicate.endpoint)
        << ", prop=" << quoted(predicate.prop)
        << ", val=" << predicate.val << "}";

    return out.str();
  }

  std::string to_string(const EndpointHasLabel &predicate)
  {
    std::stringstream out;

    out << "EndpointHasLabel{endpoint=" << common::to_string(predicate.endpoint)
        << ", label=" << quoted(predicate.label) << "}";

    return out.str();
  }

  std::string to_string(const EndpointsEqual &)
  {
    return "EndpointsEqual{}";
  }

  std::string to_string(const AtomicRelPredicate &predicate)
  {
    return std::visit(
        [](const auto &value)
        {
          return ir_builder::to_string(value);
        },
        predicate);
  }

  // ===========================================================================
  // Hierarchical predicates
  // ===========================================================================

  std::string to_string(const AdjacencyRequirement &requirement)
  {
    std::stringstream out;

    out << "AdjacencyRequirement{adjacency=" << common::to_string(requirement.adjacency)
        << ", predId=" << ir_builder::to_string(requirement.predId)
        << ", multiplicity=" << requirement.multiplicity << "}";

    return out.str();
  }

  std::string to_string(const BaseRelPredicate &predicate)
  {
    std::stringstream out;

    out << "BaseRelPredicate{atomicPredicates=[";

    for (size_t i = 0; i < predicate.atomicPredicates.size(); ++i)
    {
      if (i != 0)
        out << ", ";

      out << ir_builder::to_string(predicate.atomicPredicates[i]);
    }

    out << "]}";

    return out.str();
  }

  std::string to_string(const OtherRelPredicate &predicate)
  {
    std::stringstream out;

    out << "OtherRelPredicate{\n";
    out << "  previousId=" << ir_builder::to_string(predicate.previousId) << ",\n";
    out << "  adjacencyRequirements=[\n";

    for (const auto &requirement : predicate.adjacencyRequirements)
      out << "    " << ir_builder::to_string(requirement) << "\n";

    out << "  ]\n";
    out << "}";

    return out.str();
  }

  std::string to_string(const RelPredicate &predicate)
  {
    return std::visit(
        [](const auto &value)
        {
          return ir_builder::to_string(value);
        },
        predicate);
  }

  // ===========================================================================
  // Pattern relationship IR
  // ===========================================================================

  std::string to_string(const PatternRelationshipIR &relationship)
  {
    std::stringstream out;

    out << "PatternRelationshipIR{"
        << "global_rel_id=" << ir_builder::to_string(relationship.global_rel_id)
        << ", lv=" << query_graph::to_string(relationship.lv)
        << ", start=" << query_graph::to_string(relationship.start)
        << ", end=" << query_graph::to_string(relationship.end)
        << ", signatureId=" << ir_builder::to_string(relationship.signatureId)
        << "}";

    return out.str();
  }

  // ===========================================================================
  // Shared block IR
  // ===========================================================================

  std::string to_string(SharedBlockIR::EIdx eidx)
  {
    return "sb" + std::to_string(eidx.val);
  }

  std::string to_string(const SharedBlockIR::EdgeGraphSB &edgeGraph)
  {
    std::stringstream out;

    out << "SharedBlockEdgeGraph {\n";

    out << "  nodes: [\n";

    for (size_t i = 0; i < edgeGraph.num_nodes(); ++i)
    {
      SharedBlockIR::EIdx eidx{i};

      out << "    " << ir_builder::to_string(eidx)
          << ": " << ir_builder::to_string(edgeGraph.node_at(eidx))
          << "\n";
    }

    out << "  ],\n";

    out << "  adjacency: [\n";

    for (size_t i = 0; i < edgeGraph.num_nodes(); ++i)
    {
      SharedBlockIR::EIdx eidx{i};

      out << "    " << ir_builder::to_string(eidx)
          << " -> " << adjacencyListToString(edgeGraph, eidx)
          << "\n";
    }

    out << "  ]\n";
    out << "}";

    return out.str();
  }

  std::string to_string(const SharedBlockIR &sharedBlock)
  {
    std::stringstream out;

    out << "SharedBlockIR {\n";
    out << "  block_id: " << query_graph::to_string(sharedBlock.block_id) << ",\n";

    out << "  boundary_node: ";

    if (sharedBlock.boundary_node)
      out << ir_builder::to_string(*sharedBlock.boundary_node);
    else
      out << "null";

    out << ",\n";

    out << "  edge_graph:\n";
    out << common::indent(ir_builder::to_string(sharedBlock.edge_graph), 4) << "\n";

    out << "}";

    return out.str();
  }

  // ===========================================================================
  // Collapsed edge graph
  // ===========================================================================

  std::string to_string(EIdxCollapsed eidx)
  {
    return "ce" + std::to_string(eidx.val);
  }

  std::string to_string(const SharedBlockReference &reference)
  {
    std::stringstream out;

    out << "SharedBlockReference {\n";
    out << "  block_id: " << query_graph::to_string(reference.block_id) << ",\n";
    out << "  boundary_node: " << query_graph::to_string(reference.boundary_node) << ",\n";
    out << "  back_mapping: {\n";

    std::vector<std::pair<SharedBlockIR::EIdx, query_graph::VariableId>> mappings;

    mappings.reserve(reference.back_mapping.size());

    for (const auto &[inner, outer] : reference.back_mapping)
      mappings.emplace_back(inner, outer);

    std::sort(
        mappings.begin(),
        mappings.end(),
        [](const auto &lhs, const auto &rhs)
        {
          return lhs.first.val < rhs.first.val;
        });

    for (const auto &[inner, outer] : mappings)
    {
      out << "    " << ir_builder::to_string(inner)
          << " -> " << query_graph::to_string(outer)
          << "\n";
    }

    out << "  }\n";
    out << "}";

    return out.str();
  }

  std::string to_string(const NodeType &node)
  {
    return std::visit(
        [](const auto &value)
        {
          return ir_builder::to_string(value);
        },
        node);
  }

  std::string to_string(const EdgeGraphCollapsed &edgeGraph)
  {
    std::stringstream out;

    out << "EdgeGraphCollapsed {\n";

    out << "  nodes: [\n";

    for (size_t i = 0; i < edgeGraph.num_nodes(); ++i)
    {
      EIdxCollapsed eidx{i};

      out << "    " << ir_builder::to_string(eidx) << ":\n";
      out << common::indent(ir_builder::to_string(edgeGraph.node_at(eidx)), 6) << "\n";
    }

    out << "  ],\n";

    out << "  adjacency: [\n";

    for (size_t i = 0; i < edgeGraph.num_nodes(); ++i)
    {
      EIdxCollapsed eidx{i};

      out << "    " << ir_builder::to_string(eidx)
          << " -> " << adjacencyListToString(edgeGraph, eidx)
          << "\n";
    }

    out << "  ]\n";
    out << "}";

    return out.str();
  }

  // ===========================================================================
  // Predicate registry
  // ===========================================================================

  std::string to_string(const PredicateRegistry &registry)
  {
    std::stringstream out;

    out << "PredicateRegistry {\n";

    const auto &entries = registry.entries();

    for (size_t i = 0; i < entries.size(); ++i)
    {
      PredicateId predId{static_cast<int64_t>(i)};

      out << "  " << ir_builder::to_string(predId) << ":\n";
      out << common::indent(ir_builder::to_string(entries[i]), 4) << "\n";
    }

    out << "}";

    return out.str();
  }

  // ===========================================================================
  // Complete build result
  // ===========================================================================

  std::string to_string(const BuildResult &result)
  {
    std::stringstream out;

    out << "BuildResult {\n";

    // -------------------------------------------------------------------------
    // Predicate registry
    // -------------------------------------------------------------------------

    out << "  predicateRegistry:\n";
    out << common::indent(ir_builder::to_string(result.predicateRegistry), 4) << ",\n";

    // -------------------------------------------------------------------------
    // Signatures
    // -------------------------------------------------------------------------

    out << "  signatures: [\n";

    for (size_t layer = 0; layer < result.signatures.size(); ++layer)
    {
      out << "    layer " << layer << ": {\n";

      std::vector<std::pair<GlobalRelId, PredicateId>> signatures;

      signatures.reserve(result.signatures[layer].size());

      for (const auto &[globalRelId, predId] : result.signatures[layer])
        signatures.emplace_back(globalRelId, predId);

      std::sort(
          signatures.begin(),
          signatures.end(),
          [](const auto &lhs, const auto &rhs)
          {
            return lhs.first.val < rhs.first.val;
          });

      for (const auto &[globalRelId, predId] : signatures)
      {
        out << "      " << ir_builder::to_string(globalRelId)
            << " -> " << ir_builder::to_string(predId)
            << "\n";
      }

      out << "    }\n";
    }

    out << "  ],\n";

    // -------------------------------------------------------------------------
    // Shared block registry
    // -------------------------------------------------------------------------

    out << "  sharedBlockRegistry: {\n";
    std::vector<qg::BlockId> blockIds;
    blockIds.reserve(result.shared_block_registry.size());
    for (const auto &[blockId, _] : result.shared_block_registry)
      blockIds.push_back(blockId);
    std::sort(
        blockIds.begin(),
        blockIds.end(),
        [](qg::BlockId lhs, qg::BlockId rhs)
        {
          return lhs.val < rhs.val;
        });
    for (qg::BlockId blockId : blockIds)
    {
      out << "    " << query_graph::to_string(blockId) << ":\n";
      out << common::indent(ir_builder::to_string(result.shared_block_registry.at(blockId)), 6) << "\n";
    }
    out << "  },\n";

    // -------------------------------------------------------------------------
    // Final edge graphs
    // -------------------------------------------------------------------------

    out << "  edgeGraphs: [\n";
    for (size_t i = 0; i < result.edgeGraphs.size(); ++i)
    {
      out << "    query " << i << ":\n";
      out << common::indent(ir_builder::to_string(result.edgeGraphs[i]), 6) << "\n";
    }
    out << "  ]\n";
    out << "}";
    return out.str();
  }

} // namespace ir_builder