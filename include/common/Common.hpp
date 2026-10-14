#pragma once
#include <bits/stdc++.h>

namespace common
{
  template <typename T>
  inline void hashCombine(size_t &seed, const T &value)
  {
    seed ^= std::hash<T>{}(value) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
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
    BoundaryToSB // boundary node to a shared block. One way edge which occurs when using shared blocks
  };

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
   * return first item of adjacency
   * - myEndpoint(Start Start|End) = Start
   * - myEndpoint(End   Start|End) = End
   *
   */
  inline Endpoint myEndpoint(Adjacency adjacency)
  {
    switch (adjacency)
    {
    case Adjacency::StartStart:
    case Adjacency::StartEnd:
      return Endpoint::Start;

    case Adjacency::EndStart:
    case Adjacency::EndEnd:
      return Endpoint::End;
    }
    assert(false);
    return Endpoint::End;
  }

  /**
   * return second item of adjacency
   * - myEndpoint(Start|End Start) = Start
   * - myEndpoint(Start|End End) = End
   */
  inline Endpoint neighborEndpoint(Adjacency adjacency)
  {
    switch (adjacency)
    {
    case Adjacency::StartStart:
    case Adjacency::EndStart:
      return Endpoint::Start;

    case Adjacency::StartEnd:
    case Adjacency::EndEnd:
      return Endpoint::End;
    }
    assert(false);
    return Endpoint::End;
  }

  /**
   * If neighborEndpoint(adjancency) == Start then Outgoing else Incoming
   */
  inline Direction neighborDirection(Adjacency adjacency)
  {
    return neighborEndpoint(adjacency) == Endpoint::Start
               ? Direction::Outgoing
               : Direction::Incoming;
  }

  inline Adjacency reverseAdjacency(Adjacency x)
  {
    switch (x)
    {
    case Adjacency::StartStart:
      return Adjacency::StartStart;

    case Adjacency::StartEnd:
      return Adjacency::EndStart;

    case Adjacency::EndStart:
      return Adjacency::StartEnd;

    case Adjacency::EndEnd:
      return Adjacency::EndEnd;
    }
    assert(false);
    return Adjacency::StartStart;
  }
} // namespace common;

// strong typing
template <typename Tag, typename IntType>
struct IntStrong {
  using underlying_type = IntType;
  IntType val;
  explicit IntStrong(IntType val) : val(val) {}

  bool operator==(const IntStrong&) const = default;
  auto operator<=>(const IntStrong&) const = default;

  IntStrong& operator+=(IntStrong rhs) {
    this->val += rhs.val;
    return *this;
  }

  IntStrong& operator-=(IntStrong rhs) {
    this->val -= rhs.val;
    return *this;
  }

  friend IntStrong operator+(IntStrong lhs, IntStrong rhs) {
    lhs += rhs;
    return lhs;
  }

  friend IntStrong operator-(IntStrong lhs, IntStrong rhs) {
    lhs -= rhs;
    return lhs;
  }
};

struct IntStrongHash {
  template <typename T>
  std::size_t operator() (const T& x) const noexcept
  {
    return std::hash<typename T::underlying_type>(x.val);
  }
};

namespace query_graph {
  struct ExprId_Tag {};
  struct VariableId_Tag {};
  struct BlockId_Tag {};
  using ExprId = IntStrong<ExprId_Tag, int64_t>;
  using VariableId = IntStrong<VariableId_Tag, int64_t>;
  using BlockId = IntStrong<BlockId_Tag, int64_t>;

};

namespace kernel {
  struct DNodeId_Tag {};
  struct DRelId_Tag {};
  using DNodeId = IntStrong<DNodeId_Tag, int64_t>;
  using DRelId = IntStrong<DRelId_Tag, int64_t>;
} // namespace kernel

namespace ir_builder {
  struct GlobalNodeId_Tag {};
  struct GlobalRelId_Tag {};
  struct PredicateId_Tag {};
  using GlobalNodeId = IntStrong<GlobalNodeId_Tag, int64_t>;
  using GlobalRelId = IntStrong<GlobalRelId_Tag, int64_t>;
  using PredicateId = IntStrong<PredicateId_Tag, int64_t>;
};