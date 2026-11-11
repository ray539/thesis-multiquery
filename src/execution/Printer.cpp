#include <bits/stdc++.h>

#include <common/Execution.hpp>

namespace execution
{
  namespace
  {
    std::string indent(
        const std::string &text,
        size_t spaces)
    {
      std::stringstream input(text);
      std::stringstream output;

      std::string prefix(spaces, ' ');
      std::string line;

      bool first = true;

      while (std::getline(input, line))
      {
        if (!first)
          output << '\n';

        output << prefix << line;
        first = false;
      }

      return output.str();
    }

    std::string drelToString(k::DRelId id)
    {
      return k::to_string(id);
    }

    std::string variableToString(qg::VariableId id)
    {
      return qg::to_string(id);
    }

    std::string predicateToString(ib::PredicateId id)
    {
      return ib::to_string(id);
    }

    std::string globalRelToString(ib::GlobalRelId id)
    {
      return ib::to_string(id);
    }

    std::string collapsedIndexToString(ib::EIdxCollapsed id)
    {
      return ib::to_string(id);
    }

    std::string blockIndexToString(ib::SharedBlockIR::EIdx id)
    {
      return ib::to_string(id);
    }

    template <typename T, typename Printer>
    std::string limitedList(
        const std::vector<T> &values,
        size_t limit,
        Printer printer)
    {
      std::stringstream out;

      out << "[";

      size_t shown =
          std::min(
              values.size(),
              limit);

      for (size_t i = 0; i < shown; ++i)
      {
        if (i != 0)
          out << ", ";

        out << printer(values[i]);
      }

      if (shown < values.size())
      {
        if (shown != 0)
          out << ", ";

        out << "... +" << values.size() - shown;
      }

      out << "]";

      return out.str();
    }

    std::string drelList(
        const std::vector<k::DRelId> &values,
        size_t limit)
    {
      return limitedList(
          values,
          limit,
          [](k::DRelId id)
          {
            return drelToString(id);
          });
    }

    std::string uintList(
        const std::vector<uint64_t> &values)
    {
      return limitedList(
          values,
          values.size(),
          [](uint64_t value)
          {
            return std::to_string(value);
          });
    }

    template <typename T>
    std::vector<T> sortedStrongKeys(
        const auto &map)
    {
      std::vector<T> result;
      result.reserve(map.size());

      for (const auto &[key, _] : map)
        result.push_back(key);

      std::sort(
          result.begin(),
          result.end(),
          [](T lhs, T rhs)
          {
            return lhs.val < rhs.val;
          });

      return result;
    }
  }

  // ---------------------------------------------------------------------------
  // Predicate evaluator configuration
  // ---------------------------------------------------------------------------

  std::string to_string(
      NeighborhoodCondition condition)
  {
    switch (condition)
    {
    case NeighborhoodCondition::Existence:
      return "Existence";

    case NeighborhoodCondition::ExistenceDegree:
      return "ExistenceDegree";

    case NeighborhoodCondition::BipartiteMatching:
      return "BipartiteMatching";
    }

    throw std::logic_error(
        "Unknown NeighborhoodCondition");
  }

  std::string to_string(
      const PredicateEvaluatorConfig &config)
  {
    std::stringstream out;

    out << "PredicateEvaluatorConfig {\n";
    out << "  neighborhoodCondition: "
        << to_string(config.neighborhoodCondition)
        << "\n";
    out << "}";

    return out.str();
  }

  // ---------------------------------------------------------------------------
  // Embeddings
  // ---------------------------------------------------------------------------

  std::string to_string(
      const Embedding &embedding)
  {
    std::vector<std::pair<qg::VariableId, k::DRelId>> entries(
        embedding.relationships.begin(),
        embedding.relationships.end());

    std::sort(
        entries.begin(),
        entries.end(),
        [](const auto &lhs, const auto &rhs)
        {
          return lhs.first.val < rhs.first.val;
        });

    std::stringstream out;

    out << "Embedding {";

    if (!entries.empty())
      out << "\n";

    for (const auto &[variable, drel] : entries)
    {
      out << "  "
          << variableToString(variable)
          << " -> "
          << drelToString(drel)
          << "\n";
    }

    out << "}";

    return out.str();
  }

  std::string to_string(
      const SharedBlockRow &row)
  {
    std::vector<std::pair<ib::SharedBlockIR::EIdx, k::DRelId>> entries(
        row.relationships.begin(),
        row.relationships.end());

    std::sort(
        entries.begin(),
        entries.end(),
        [](const auto &lhs, const auto &rhs)
        {
          return lhs.first.val < rhs.first.val;
        });

    std::stringstream out;

    out << "SharedBlockRow {";

    if (!entries.empty())
      out << "\n";

    for (const auto &[index, drel] : entries)
    {
      out << "  "
          << blockIndexToString(index)
          << " -> "
          << drelToString(drel)
          << "\n";
    }

    out << "}";

    return out.str();
  }

  std::string to_string(
      const SharedBlockCacheKey &key)
  {
    std::stringstream out;

    out << "{ block="
        << qg::to_string(key.block_id)
        << ", boundary=";

    if (key.boundary)
      out << drelToString(*key.boundary);
    else
      out << "none";

    out << " }";

    return out.str();
  }

  // ---------------------------------------------------------------------------
  // Lazy execution metrics
  // ---------------------------------------------------------------------------

  std::string to_string(
      const SharedBlockExecutionMetrics &metrics)
  {
    std::stringstream out;

    out << "SharedBlockExecutionMetrics {\n";

    out << "  iteratorCreations: "
        << metrics.iteratorCreations
        << "\n";

    out << "  cacheEntriesCreated: "
        << metrics.cacheEntriesCreated
        << "\n";

    out << "  rowRequests: "
        << metrics.rowRequests
        << "\n";

    out << "  cachedRowsReturned: "
        << metrics.cachedRowsReturned
        << "\n";

    out << "  rowsProduced: "
        << metrics.rowsProduced
        << "\n";

    out << "  producerResumptions: "
        << metrics.producerResumptions
        << "\n";

    out << "  producerExhaustions: "
        << metrics.producerExhaustions
        << "\n";

    out << "  candidateRelationshipsVisited: "
        << metrics.candidateRelationshipsVisited
        << "\n";

    out << "  rejectedByUniqueness: "
        << metrics.rejectedByUniqueness
        << "\n";

    out << "  rejectedByAdjacency: "
        << metrics.rejectedByAdjacency
        << "\n";

    out << "  rejectedByPredicate: "
        << metrics.rejectedByPredicate
        << "\n";

    out << "  recursiveCallsAtDepth:\n";

    auto blocks =
        sortedStrongKeys<qg::BlockId>(
            metrics.recursiveCallsAtDepth);

    for (qg::BlockId block : blocks)
    {
      out << "    "
          << qg::to_string(block)
          << ": "
          << uintList(
                 metrics.recursiveCallsAtDepth.at(block))
          << "\n";
    }

    out << "}";

    return out.str();
  }

  std::string to_string(
      const QueryExecutionMetrics &metrics)
  {
    std::stringstream out;

    out << "QueryExecutionMetrics {\n";

    out << "  recursiveCallsAtDepth: "
        << uintList(metrics.recursiveCallsAtDepth)
        << "\n";

    out << "  completeMatches: "
        << metrics.completeMatches
        << "\n";

    out << "  regularCandidatesVisited: "
        << metrics.regularCandidatesVisited
        << "\n";

    out << "  regularRejectedByUniqueness: "
        << metrics.regularRejectedByUniqueness
        << "\n";

    out << "  regularRejectedByAdjacency: "
        << metrics.regularRejectedByAdjacency
        << "\n";

    out << "  regularRejectedByPredicate: "
        << metrics.regularRejectedByPredicate
        << "\n";

    out << "  sharedBlockRowsVisited: "
        << metrics.sharedBlockRowsVisited
        << "\n";

    out << "  sharedBlockRowsRejectedByOuterUniqueness: "
        << metrics.sharedBlockRowsRejectedByOuterUniqueness
        << "\n";

    out << "  matchingOrder: [";

    for (size_t i = 0; i < metrics.matchingOrder.size(); ++i)
    {
      if (i != 0)
        out << ", ";

      out << collapsedIndexToString(
          metrics.matchingOrder[i]);
    }

    out << "]\n";
    out << "}";

    return out.str();
  }

  std::string to_string(
      const QueryExecutionResult &result,
      const ExecutionPrintOptions &options)
  {
    std::stringstream out;

    out << "QueryExecutionResult {\n";

    out << "  matchCount: "
        << result.matches.size()
        << "\n";

    out << "  metrics:\n"
        << indent(
               to_string(result.metrics),
               4)
        << "\n";

    out << "  matches:\n";

    size_t shown =
        std::min(
            result.matches.size(),
            options.maxEmbeddings);

    for (size_t i = 0; i < shown; ++i)
    {
      out << "    [" << i << "]\n"
          << indent(
                 to_string(result.matches[i]),
                 6)
          << "\n";
    }

    if (shown < result.matches.size())
    {
      out << "    ... "
          << result.matches.size() - shown
          << " more embeddings\n";
    }

    out << "}";

    return out.str();
  }

  // ---------------------------------------------------------------------------
  // DFS helpers
  // ---------------------------------------------------------------------------

  std::string to_string(
      const dfs_helper::BoundConstraint &constraint)
  {
    std::stringstream out;

    out << "{ adjacency="
        << common::to_string(constraint.adjacency)
        << ", other="
        << drelToString(constraint.otherDRel)
        << " }";

    return out.str();
  }

  // ---------------------------------------------------------------------------
  // Candidate sets
  // ---------------------------------------------------------------------------

  std::string to_string(
      const CandidateSet &candidateSet,
      const ExecutionPrintOptions &options)
  {
    std::stringstream out;

    out << "CandidateSet {\n";

    out << "  size: "
        << candidateSet.candidates.size()
        << "\n";

    out << "  membershipSize: "
        << candidateSet.membership.size()
        << "\n";

    out << "  candidates: "
        << drelList(
               candidateSet.candidates,
               options.maxListItems)
        << "\n";

    out << "}";

    return out.str();
  }

  std::string to_string(
      const CandidateAdjacencyKey &key)
  {
    std::stringstream out;

    out << "("
        << predicateToString(key.sourcePredicate)
        << " -> "
        << predicateToString(key.targetPredicate)
        << ", "
        << common::to_string(key.adjacency)
        << ")";

    return out.str();
  }

  std::string to_string(
      const CandidateAdjacency &adjacency,
      const ExecutionPrintOptions &options)
  {
    std::stringstream out;

    uint64_t edgeCount = 0;

    for (const auto &[_, neighbors] : adjacency.neighbors)
      edgeCount += neighbors.size();

    out << "CandidateAdjacency {\n";

    out << "  sources: "
        << adjacency.neighbors.size()
        << "\n";

    out << "  edges: "
        << edgeCount
        << "\n";

    out << "  neighbors:\n";

    auto sources =
        sortedStrongKeys<k::DRelId>(
            adjacency.neighbors);

    size_t shown =
        std::min(
            sources.size(),
            options.maxMapEntries);

    for (size_t i = 0; i < shown; ++i)
    {
      k::DRelId source =
          sources[i];

      out << "    "
          << drelToString(source)
          << " -> "
          << drelList(
                 adjacency.neighbors.at(source),
                 options.maxListItems)
          << "\n";
    }

    if (shown < sources.size())
    {
      out << "    ... "
          << sources.size() - shown
          << " more sources\n";
    }

    out << "}";

    return out.str();
  }

  std::string to_string(
      const QueryCandidateSpace &space)
  {
    std::vector<std::pair<ib::GlobalRelId, ib::PredicateId>> entries(
        space.predicateOfRelationship.begin(),
        space.predicateOfRelationship.end());

    std::sort(
        entries.begin(),
        entries.end(),
        [](const auto &lhs, const auto &rhs)
        {
          return lhs.first.val < rhs.first.val;
        });

    std::stringstream out;

    out << "QueryCandidateSpace {\n";

    for (const auto &[relationship, predicate] : entries)
    {
      out << "  "
          << globalRelToString(relationship)
          << " -> "
          << predicateToString(predicate)
          << "\n";
    }

    out << "}";

    return out.str();
  }

  // ---------------------------------------------------------------------------
  // Full candidate space
  // ---------------------------------------------------------------------------

  std::string to_string(
      const CandidateSpace &space,
      const ExecutionPrintOptions &options)
  {
    std::stringstream out;

    out << "CandidateSpace {\n";

    // -------------------------------------------------------------------------
    // Candidate sets
    // -------------------------------------------------------------------------

    out << "  candidateSets ("
        << space.candidatesBySignature_.size()
        << "):\n";

    auto predicates =
        sortedStrongKeys<ib::PredicateId>(
            space.candidatesBySignature_);

    for (ib::PredicateId predicate : predicates)
    {
      const CandidateSet &set =
          space.candidatesBySignature_.at(predicate);

      out << "    "
          << predicateToString(predicate)
          << ": size="
          << set.candidates.size()
          << ", values="
          << drelList(
                 set.candidates,
                 options.maxListItems)
          << "\n";
    }

    // -------------------------------------------------------------------------
    // Query mappings
    // -------------------------------------------------------------------------

    out << "  queries ("
        << space.queries_.size()
        << "):\n";

    for (size_t i = 0; i < space.queries_.size(); ++i)
    {
      out << "    query[" << i << "]:\n"
          << indent(
                 to_string(space.queries_[i]),
                 6)
          << "\n";
    }

    // -------------------------------------------------------------------------
    // Candidate adjacency
    // -------------------------------------------------------------------------

    uint64_t totalAdjacencyEdges = 0;

    for (const auto &[_, adjacency] : space.adjacency_)
    {
      for (const auto &[__, neighbors] : adjacency.neighbors)
        totalAdjacencyEdges += neighbors.size();
    }

    out << "  candidateAdjacencies ("
        << space.adjacency_.size()
        << ", edges="
        << totalAdjacencyEdges
        << "):\n";

    std::vector<
        std::pair<
            CandidateAdjacencyKey,
            const CandidateAdjacency *>>
        adjacencyEntries;

    adjacencyEntries.reserve(
        space.adjacency_.size());

    for (const auto &[key, adjacency] : space.adjacency_)
    {
      adjacencyEntries.push_back(
          {
              key,
              &adjacency,
          });
    }

    std::sort(
        adjacencyEntries.begin(),
        adjacencyEntries.end(),
        [](const auto &lhs, const auto &rhs)
        {
          const auto &a = lhs.first;
          const auto &b = rhs.first;

          if (a.sourcePredicate.val != b.sourcePredicate.val)
            return a.sourcePredicate.val < b.sourcePredicate.val;

          if (a.targetPredicate.val != b.targetPredicate.val)
            return a.targetPredicate.val < b.targetPredicate.val;

          return static_cast<int>(a.adjacency) <
                 static_cast<int>(b.adjacency);
        });

    size_t shown =
        std::min(
            adjacencyEntries.size(),
            options.maxMapEntries);

    for (size_t i = 0; i < shown; ++i)
    {
      const auto &[key, adjacency] =
          adjacencyEntries[i];

      uint64_t edgeCount = 0;

      for (const auto &[_, neighbors] : adjacency->neighbors)
        edgeCount += neighbors.size();

      out << "    "
          << to_string(key)
          << ": sources="
          << adjacency->neighbors.size()
          << ", edges="
          << edgeCount
          << "\n";
    }

    if (shown < adjacencyEntries.size())
    {
      out << "    ... "
          << adjacencyEntries.size() - shown
          << " more adjacency relations\n";
    }

    out << "}";

    return out.str();
  }

  // ---------------------------------------------------------------------------
  // Eager executor
  // ---------------------------------------------------------------------------

  std::string to_string(
      EagerOrderingStrategy strategy)
  {
    switch (strategy)
    {
    case EagerOrderingStrategy::SmallestSetFirst:
      return "SmallestSetFirst";

    case EagerOrderingStrategy::ConnectedGreedy:
      return "ConnectedGreedy";

    case EagerOrderingStrategy::LazyGreedy:
      return "LazyGreedy";
    }

    throw std::logic_error(
        "Unknown EagerOrderingStrategy");
  }

  std::string to_string(
      const EagerExecutorOptions &options)
  {
    std::stringstream out;

    out << "EagerExecutorOptions {\n";

    out << "  signatureLevel: "
        << options.signatureLevel
        << "\n";

    out << "  ordering: "
        << to_string(options.ordering)
        << "\n";

    out << "}";

    return out.str();
  }

  std::string to_string(
      const EagerExecutionMetrics &metrics)
  {
    std::stringstream out;

    out << "EagerExecutionMetrics {\n";

    out << "  recursiveCallsAtDepth: "
        << uintList(metrics.recursiveCallsAtDepth)
        << "\n";

    out << "  candidatesVisited: "
        << metrics.candidatesVisited
        << "\n";

    out << "  rejectedByUniqueness: "
        << metrics.rejectedByUniqueness
        << "\n";

    out << "  rejectedByAdjacency: "
        << metrics.rejectedByAdjacency
        << "\n";

    out << "  completeMatches: "
        << metrics.completeMatches
        << "\n";

    out << "  matchingOrder: [";

    for (size_t i = 0; i < metrics.matchingOrder.size(); ++i)
    {
      if (i != 0)
        out << ", ";

      out << collapsedIndexToString(
          metrics.matchingOrder[i]);
    }

    out << "]\n";

    out << "}";

    return out.str();
  }

} // namespace execution