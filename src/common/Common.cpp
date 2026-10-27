#include <common/Common.hpp>

namespace common
{
  std::string to_string(
      Direction direction)
  {
    switch (direction)
    {
    case Direction::Outgoing:
      return "Outgoing";

    case Direction::Incoming:
      return "Incoming";
    }

    std::unreachable();
  }

  std::string to_string(
      Endpoint endpoint)
  {
    switch (endpoint)
    {
    case Endpoint::Start:
      return "Start";

    case Endpoint::End:
      return "End";
    }

    std::unreachable();
  }

  std::string to_string(
      Adjacency adjacency)
  {
    switch (adjacency)
    {
    case Adjacency::StartStart:
      return "StartStart";

    case Adjacency::StartEnd:
      return "StartEnd";

    case Adjacency::EndStart:
      return "EndStart";

    case Adjacency::EndEnd:
      return "EndEnd";

    case Adjacency::BoundaryToSB:
      return "BoundaryToSB";
    }

    std::unreachable();
  }

} // namespace common

namespace query_graph
{
  std::string to_string(ExprId id)
  {
    return "e" + std::to_string(id.val);
  }

  std::string to_string(VariableId id)
  {
    return "v" + std::to_string(id.val);
  }

  std::string to_string(
      BlockId id)
  {
    return "B" + std::to_string(id.val);
  }

} // namespace query_graph

namespace kernel
{
  std::string to_string(DNodeId id)
  {
    return "dn" + std::to_string(id.val);
  }

  std::string to_string(DRelId id)
  {
    return "dr" + std::to_string(id.val);
  }

} // namespace kernel

namespace ir_builder
{
  std::string to_string(GlobalNodeId id)
  {
    return "gn" + std::to_string(id.val);
  }

  std::string to_string(GlobalRelId id)
  {
    return "gr" + std::to_string(id.val);
  }

  std::string to_string(PredicateId id)
  {
    return "p" + std::to_string(id.val);
  }

} // namespace ir_builder