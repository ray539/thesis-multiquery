#pragma once

#include <bits/stdc++.h>

namespace common
{
  template <typename T>
  inline void hashCombine(
      size_t &seed,
      const T &value)
  {
    seed ^= std::hash<T>{}(value) +
            0x9e3779b97f4a7c15ULL +
            (seed << 6) +
            (seed >> 2);
  }

  enum class Direction
  {
    Outgoing,
    Incoming,
  };

  enum class Endpoint
  {
    Start,
    End
  };

  // (my endpoint), (neighbor endpoint)
  enum class Adjacency
  {
    StartStart,
    StartEnd,
    EndStart,
    EndEnd,

    // Special collapsed-edge-graph execution dependency.
    BoundaryToSB
  };

  std::string to_string(Direction direction);
  std::string to_string(Endpoint endpoint);
  std::string to_string(Adjacency adjacency);

  inline Adjacency adjacencyOf(
      Endpoint mine,
      Endpoint theirs)
  {
    if (
        mine == Endpoint::Start &&
        theirs == Endpoint::Start)
    {
      return Adjacency::StartStart;
    }

    if (
        mine == Endpoint::Start &&
        theirs == Endpoint::End)
    {
      return Adjacency::StartEnd;
    }

    if (
        mine == Endpoint::End &&
        theirs == Endpoint::Start)
    {
      return Adjacency::EndStart;
    }

    return Adjacency::EndEnd;
  }

  /**
   * Return first item of adjacency:
   *
   * myEndpoint(StartStart) = Start
   * myEndpoint(StartEnd)   = Start
   * myEndpoint(EndStart)   = End
   * myEndpoint(EndEnd)     = End
   *
   * BoundaryToSB is not an ordinary relationship adjacency.
   */
  inline Endpoint myEndpoint(
      Adjacency adjacency)
  {
    switch (adjacency)
    {
    case Adjacency::StartStart:
    case Adjacency::StartEnd:
      return Endpoint::Start;

    case Adjacency::EndStart:
    case Adjacency::EndEnd:
      return Endpoint::End;

    case Adjacency::BoundaryToSB:
      break;
    }

    assert(false);
    return Endpoint::End;
  }

  /**
   * Return second item of adjacency:
   *
   * neighborEndpoint(StartStart) = Start
   * neighborEndpoint(EndStart)   = Start
   * neighborEndpoint(StartEnd)   = End
   * neighborEndpoint(EndEnd)     = End
   *
   * BoundaryToSB is not an ordinary relationship adjacency.
   */
  inline Endpoint neighborEndpoint(
      Adjacency adjacency)
  {
    switch (adjacency)
    {
    case Adjacency::StartStart:
    case Adjacency::EndStart:
      return Endpoint::Start;

    case Adjacency::StartEnd:
    case Adjacency::EndEnd:
      return Endpoint::End;

    case Adjacency::BoundaryToSB:
      break;
    }

    assert(false);
    return Endpoint::End;
  }

  /**
   * If neighborEndpoint(adjacency) == Start:
   *   Outgoing
   *
   * otherwise:
   *   Incoming
   */
  inline Direction neighborDirection(
      Adjacency adjacency)
  {
    return neighborEndpoint(adjacency) ==
                   Endpoint::Start
               ? Direction::Outgoing
               : Direction::Incoming;
  }

  inline Adjacency reverseAdjacency(
      Adjacency adjacency)
  {
    switch (adjacency)
    {
    case Adjacency::StartStart:
      return Adjacency::StartStart;

    case Adjacency::StartEnd:
      return Adjacency::EndStart;

    case Adjacency::EndStart:
      return Adjacency::StartEnd;

    case Adjacency::EndEnd:
      return Adjacency::EndEnd;

    case Adjacency::BoundaryToSB:
      break;
    }

    // BoundaryToSB is deliberately one-way.
    assert(false);
    return Adjacency::BoundaryToSB;
  }

  // ----------------------------------------
  // Printing helpers
  // ----------------------------------------
  std::string quoted(const std::string &value)
  {
    return "\"" + value + "\"";
  }
  template <typename T>
  std::string join(
      const std::vector<T> &items,
      const std::function<std::string(const T &)> &printer,
      std::string_view separator = ", ")
  {
    std::string result;
    for (size_t i = 0; i < items.size(); ++i)
    {
      if (i != 0)
      {
        result += separator;
      }
      result += printer(items[i]);
    }
    return result;
  }

  template <typename T>
  std::string commaSeparated(const std::vector<T> &values, auto printer)
  {
    std::stringstream out;

    for (size_t i = 0; i < values.size(); ++i)
    {
      if (i != 0)
        out << ", ";

      out << printer(values[i]);
    }

    return out.str();
  }

  std::string indent(const std::string &text, size_t spaces)
  {
    const std::string prefix(spaces, ' ');
    std::stringstream input{text};
    std::string line;
    std::string result;
    bool first = true;
    while (std::getline(input, line))
    {
      if (!first)
      {
        result += '\n';
      }
      first = false;
      result += prefix;
      result += line;
    }
    return result;
  }
} // namespace common

// -----------------------------------------------------------------------------
// Strong integer types
// -----------------------------------------------------------------------------

template <typename Tag, typename IntType>
struct IntStrong
{
  using underlying_type = IntType;

  IntType val;

  explicit IntStrong(IntType val)
      : val{val}
  {
  }
  bool operator==(
      const IntStrong &) const = default;

  auto operator<=>(
      const IntStrong &) const = default;

  IntStrong &operator+=(
      IntStrong rhs)
  {
    val += rhs.val;
    return *this;
  }

  IntStrong &operator-=(
      IntStrong rhs)
  {
    val -= rhs.val;
    return *this;
  }

  friend IntStrong operator+(
      IntStrong lhs,
      IntStrong rhs)
  {
    lhs += rhs;
    return lhs;
  }

  friend IntStrong operator-(
      IntStrong lhs,
      IntStrong rhs)
  {
    lhs -= rhs;
    return lhs;
  }
};

struct IntStrongHash
{
  template <typename T>
  std::size_t operator()(
      const T &x) const noexcept
  {
    return std::hash<
        typename T::underlying_type>{}(x.val);
  }
};

// -----------------------------------------------------------------------------
// Query graph IDs
// -----------------------------------------------------------------------------

namespace query_graph
{
  struct ExprId_Tag;
  struct VariableId_Tag;
  struct BlockId_Tag;
  using ExprId =
      IntStrong<ExprId_Tag, int64_t>;

  using VariableId =
      IntStrong<VariableId_Tag, int64_t>;

  using BlockId =
      IntStrong<BlockId_Tag, int64_t>;

  std::string to_string(ExprId id);
  std::string to_string(VariableId id);
  std::string to_string(BlockId id);

} // namespace query_graph

// -----------------------------------------------------------------------------
// Data graph IDs
// -----------------------------------------------------------------------------

namespace kernel
{
  struct DNodeId_Tag;
  struct DRelId_Tag;
  using DNodeId =
      IntStrong<DNodeId_Tag, int64_t>;

  using DRelId =
      IntStrong<DRelId_Tag, int64_t>;

  std::string to_string(DNodeId id);
  std::string to_string(DRelId id);

} // namespace kernel

// -----------------------------------------------------------------------------
// IR builder IDs
// -----------------------------------------------------------------------------

namespace ir_builder
{
  struct GlobalNodeId_Tag;
  struct GlobalRelId_Tag;
  struct PredicateId_Tag;
  using GlobalNodeId =
      IntStrong<GlobalNodeId_Tag, int64_t>;

  using GlobalRelId =
      IntStrong<GlobalRelId_Tag, int64_t>;

  using PredicateId =
      IntStrong<PredicateId_Tag, int64_t>;

  std::string to_string(GlobalNodeId id);
  std::string to_string(GlobalRelId id);
  std::string to_string(PredicateId id);
} // namespace ir_builder