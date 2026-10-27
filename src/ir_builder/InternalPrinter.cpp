#include <ir_builder/IRBuilderInternal.hpp>

namespace ir_builder
{
  std::string to_string(const PatternRelationshipInternal &relationship)
  {
    std::stringstream out;

    out << "PatternRelationshipInternal{"
        << "queryIndex=" << relationship.queryIndex
        << ", localId=" << query_graph::to_string(relationship.localId)
        << ", localStartNode=" << query_graph::to_string(relationship.localStartNode)
        << ", localEndNode=" << query_graph::to_string(relationship.localEndNode)
        << ", globalId=" << ir_builder::to_string(relationship.globalId)
        << ", globalStartNode=" << ir_builder::to_string(relationship.globalStartNode)
        << ", globalEndNode=" << ir_builder::to_string(relationship.globalEndNode)
        << ", signatureId=" << ir_builder::to_string(relationship.signatureId)
        << ", atomicPredicates=[";

    for (size_t i = 0; i < relationship.atomicPredicates.size(); ++i)
    {
      if (i != 0)
        out << ", ";

      out << ir_builder::to_string(relationship.atomicPredicates[i]);
    }

    out << "]}";

    return out.str();
  }

  std::string to_string(const QueryLocalToGlobal &mapping)
  {
    std::stringstream out;

    out << "QueryLocalToGlobal {\n";

    out << "  node_to_globalID: {\n";

    std::vector<std::pair<query_graph::VariableId, GlobalNodeId>> nodes(
        mapping.node_to_globalID.begin(),
        mapping.node_to_globalID.end());

    std::sort(nodes.begin(), nodes.end(),
              [](const auto &a, const auto &b)
              {
                return a.first.val < b.first.val;
              });

    for (const auto &[local, global] : nodes)
    {
      out << "    " << query_graph::to_string(local)
          << " -> " << ir_builder::to_string(global) << "\n";
    }

    out << "  },\n";

    out << "  rel_to_globalID: {\n";

    std::vector<std::pair<query_graph::VariableId, GlobalRelId>> relationships(
        mapping.rel_to_globalID.begin(),
        mapping.rel_to_globalID.end());

    std::sort(relationships.begin(), relationships.end(),
              [](const auto &a, const auto &b)
              {
                return a.first.val < b.first.val;
              });

    for (const auto &[local, global] : relationships)
    {
      out << "    " << query_graph::to_string(local)
          << " -> " << ir_builder::to_string(global) << "\n";
    }

    out << "  }\n";
    out << "}";

    return out.str();
  }

  std::string to_string(const Incidence &incidence)
  {
    std::stringstream out;

    out << "Incidence{relId=" << ir_builder::to_string(incidence.relId)
        << ", endpoint=" << common::to_string(incidence.endpoint)
        << "}";

    return out.str();
  }

  std::string to_string(const GlobalEntities &entities)
  {
    std::stringstream out;

    out << "GlobalEntities {\n";

    // -----------------------------------------------------------------------
    // Per-query local/global mappings
    // -----------------------------------------------------------------------

    out << "  queryLocalToGlobals: [\n";

    for (size_t qi = 0; qi < entities.queryLocalToGlobals.size(); ++qi)
    {
      out << "    query " << qi << ":\n";
      out << common::indent(ir_builder::to_string(entities.queryLocalToGlobals[qi]), 6) << "\n";
    }

    out << "  ],\n";

    // -----------------------------------------------------------------------
    // Relationships
    // -----------------------------------------------------------------------

    out << "  relationships: {\n";

    std::vector<GlobalRelId> relationshipIds;
    relationshipIds.reserve(entities.relationships.size());

    for (const auto &[id, _] : entities.relationships)
      relationshipIds.push_back(id);

    std::sort(relationshipIds.begin(), relationshipIds.end(),
              [](GlobalRelId a, GlobalRelId b)
              {
                return a.val < b.val;
              });

    for (GlobalRelId id : relationshipIds)
    {
      out << "    " << ir_builder::to_string(id) << ":\n";
      out << common::indent(ir_builder::to_string(entities.relationships.at(id)), 6) << "\n";
    }

    out << "  },\n";

    // -----------------------------------------------------------------------
    // Node incidence
    // -----------------------------------------------------------------------

    out << "  adjListNode: {\n";

    std::vector<GlobalNodeId> nodeIds;
    nodeIds.reserve(entities.adjListNode.size());

    for (const auto &[id, _] : entities.adjListNode)
      nodeIds.push_back(id);

    std::sort(nodeIds.begin(), nodeIds.end(),
              [](GlobalNodeId a, GlobalNodeId b)
              {
                return a.val < b.val;
              });

    for (GlobalNodeId nodeId : nodeIds)
    {
      out << "    " << ir_builder::to_string(nodeId) << ": [";

      const auto &incidences = entities.adjListNode.at(nodeId);

      for (size_t i = 0; i < incidences.size(); ++i)
      {
        if (i != 0)
          out << ", ";

        out << ir_builder::to_string(incidences[i]);
      }

      out << "]\n";
    }

    out << "  }\n";
    out << "}";

    return out.str();
  }

  std::string to_string(const SignatureBuilderResult &result)
  {
    std::stringstream out;

    out << "SignatureBuilderResult {\n";

    out << "  signatures: [\n";

    for (size_t layer = 0; layer < result.signatures.size(); ++layer)
    {
      out << "    layer " << layer << ": {\n";

      std::vector<std::pair<GlobalRelId, PredicateId>> entries(
          result.signatures[layer].begin(),
          result.signatures[layer].end());

      std::sort(entries.begin(), entries.end(),
                [](const auto &a, const auto &b)
                {
                  return a.first.val < b.first.val;
                });

      for (const auto &[relId, predicateId] : entries)
      {
        out << "      " << ir_builder::to_string(relId)
            << " -> " << ir_builder::to_string(predicateId) << "\n";
      }

      out << "    }\n";
    }

    out << "  ],\n";

    out << "  predicateRegistry:\n";
    out << common::indent(ir_builder::to_string(result.pRegistry), 4) << "\n";

    out << "}";

    return out.str();
  }

  // ===========================================================================
  // Raw edge graph
  // ===========================================================================

  std::string to_string(EIdxRaw eidx)
  {
    return "re" + std::to_string(eidx.val);
  }

  std::string to_string(const EdgeGraphRaw &edgeGraph)
  {
    std::stringstream out;

    out << "EdgeGraphRaw {\n";

    out << "  nodes: [\n";

    for (size_t i = 0; i < edgeGraph.num_nodes(); ++i)
    {
      EIdxRaw eidx{i};

      out << "    " << ir_builder::to_string(eidx)
          << ": " << ir_builder::to_string(edgeGraph.node_at(eidx))
          << "\n";
    }

    out << "  ],\n";

    out << "  adjacency: [\n";

    for (size_t i = 0; i < edgeGraph.num_nodes(); ++i)
    {
      EIdxRaw eidx{i};

      out << "    " << ir_builder::to_string(eidx) << " -> [";

      const auto edges = edgeGraph.adj(eidx);

      for (size_t j = 0; j < edges.size(); ++j)
      {
        if (j != 0)
          out << ", ";

        out << "{type=" << common::to_string(edges[j].adjType)
            << ", other=" << ir_builder::to_string(edges[j].other)
            << "}";
      }

      out << "]\n";
    }

    out << "  ]\n";
    out << "}";

    return out.str();
  }

  // ===========================================================================
  // Shared-block builder intermediates
  // ===========================================================================

  namespace edge_graph_helpers
  {
    std::string to_string(const BlockOccurrence::Boundary &boundary)
    {
      std::stringstream out;

      out << "Boundary{boundary_local_index=" << boundary.boundary_local_index
          << ", boundary_outer_index=" << ir_builder::to_string(boundary.boundary_outer_index)
          << "}";

      return out.str();
    }

    std::string to_string(const BlockOccurrence &occurrence)
    {
      std::stringstream out;

      out << "BlockOccurrence {\n";
      out << "  block_id: " << query_graph::to_string(occurrence.block_id) << ",\n";

      out << "  outer_node_indices: [";

      for (size_t i = 0; i < occurrence.outer_node_indices.size(); ++i)
      {
        if (i != 0)
          out << ", ";

        out << ir_builder::to_string(occurrence.outer_node_indices[i]);
      }

      out << "],\n";

      out << "  boundary: ";

      if (occurrence.boundary)
        out << edge_graph_helpers::to_string(*occurrence.boundary);
      else
        out << "null";

      out << "\n";
      out << "}";

      return out.str();
    }

    std::string to_string(const BlockShape &shape)
    {
      std::stringstream out;

      out << "BlockShape {\n";

      out << "  predicate_ids: [";

      for (size_t i = 0; i < shape.predicate_ids.size(); ++i)
      {
        if (i != 0)
          out << ", ";

        out << ir_builder::to_string(shape.predicate_ids[i]);
      }

      out << "],\n";

      out << "  boundary_local_index: ";

      if (shape.boundary_local_index)
        out << ir_builder::to_string(*shape.boundary_local_index);
      else
        out << "null";

      out << ",\n";

      out << "  edges: [\n";

      for (const auto &[from, to, adjacency] : shape.edges)
      {
        out << "    {from=" << ir_builder::to_string(from)
            << ", to=" << ir_builder::to_string(to)
            << ", adjacency=" << common::to_string(static_cast<common::Adjacency>(adjacency))
            << "}\n";
      }

      out << "  ]\n";
      out << "}";

      return out.str();
    }

    namespace node_classification
    {
      std::string to_string(const Boundary &classification)
      {
        std::stringstream out;

        out << "Boundary{occurrence_ids=[";

        for (size_t i = 0; i < classification.occurrence_ids.size(); ++i)
        {
          if (i != 0)
            out << ", ";

          out << classification.occurrence_ids[i];
        }

        out << "]}";

        return out.str();
      }

      std::string to_string(const Inner &classification)
      {
        return "Inner{occurrence_id=" + std::to_string(classification.occurrence_id) + "}";
      }

      std::string to_string(const Outer &)
      {
        return "Outer{}";
      }

      std::string to_string(const nc &classification)
      {
        return std::visit(
            [](const auto &value)
            {
              return node_classification::to_string(value);
            },
            classification);
      }

    } // namespace node_classification

  } // namespace edge_graph_helpers

} // namespace ir_builder