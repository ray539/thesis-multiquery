#pragma once
#include <bits/stdc++.h>

namespace common {
template <typename T>
inline void hashCombine(size_t& seed, const T& value) {
  seed ^= std::hash<T>{}(value)
        + 0x9e3779b97f4a7c15ULL
        + (seed << 6)
        + (seed >> 2);
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
  EndEnd
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
inline Endpoint myEndpoint(Adjacency adjacency) {
  switch (adjacency) {
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
inline Endpoint neighborEndpoint(Adjacency adjacency) {
  switch (adjacency) {
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
inline Direction neighborDirection(Adjacency adjacency) {
  return neighborEndpoint(adjacency)
      == Endpoint::Start
      ? Direction::Outgoing
      : Direction::Incoming;
}


inline Adjacency reverseAdjacency(Adjacency x) {
  switch (x) {
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


}
