#include <bits/stdc++.h>
#include <common/IRBuilder.hpp>

// convert
// - AtomicRelPredicate
// - BaseRelPredicate
// - OtherRelPredicate
// to a canonical string representation used for checking equality
namespace ir_builder
{
  namespace qg = query_graph;

  namespace predicate_key
  {
    static std::string encodeString(const std::string &s)
    {
      return std::to_string(s.size()) + ":" + s;
    }

    static std::string atomicRelPredicateKey(const AtomicRelPredicate &predicate)
    {
      if (auto *p = std::get_if<RelPropertyEquals>(&predicate))
      {
        return "RP|" + encodeString(p->prop) + "|" + std::to_string(p->val);
      }
      if (auto *p = std::get_if<RelTypeEquals>(&predicate))
      {
        return "RT|" + encodeString(p->relType);
      }
      if (auto *p = std::get_if<EndpointPropertyEquals>(&predicate))
      {
        return "EP|" +
               std::to_string((int)p->endpoint) +
               "|" +
               encodeString(p->prop) +
               "|" +
               std::to_string(p->val);
      }
      if (auto *p = std::get_if<EndpointHasLabel>(&predicate))
      {
        return "EL|" +
               std::to_string((int)p->endpoint) +
               "|" +
               encodeString(p->label);
      }
      if (auto *p = std::get_if<EndpointsEqual>(&predicate))
      {
        return "EPE|";
      }

      throw CanonicalizationError("invalid variant type");
    }

    // sort atomic rel predicates based on their keys
    // r.x == 1 AND r.y == 2,  r.y==2ANDr.x==1 should receive same keys
    static void canonicalizeAtoms(
        std::vector<AtomicRelPredicate> &atoms)
    {

      // sort by canonical atomic predicate key
      std::sort(
          atoms.begin(),
          atoms.end(),
          [](const auto &a, const auto &b)
          {
            return atomicRelPredicateKey(a) < atomicRelPredicateKey(b);
          });

      // p AND p == p.
      atoms.erase(
          std::unique(
              atoms.begin(),
              atoms.end(),
              [](const auto &a, const auto &b)
              {
                return atomicRelPredicateKey(a) == atomicRelPredicateKey(b);
              }),
          atoms.end());
    }

    std::string predicateKey(const RelPredicate &predicate)
    {
      std::string ans;
      if (const auto *p = std::get_if<BaseRelPredicate>(&predicate))
      {
        
        auto atomicPredicatesCopy = p->atomicPredicates;
        canonicalizeAtoms(atomicPredicatesCopy);
        ans = "BASE|";
        for (const auto &atom : atomicPredicatesCopy)
        {
          ans += encodeString(atomicRelPredicateKey(atom));
        }
        return ans;
      }
      if (const auto *p = std::get_if<OtherRelPredicate>(&predicate))
      {
        ans = "OTHER|" + std::to_string(p->previousId.val) + "|";
        // adjacencyRequirements are already sorted canonically when constructed.
        for (const auto &req : p->adjacencyRequirements)
        {
          ans += std::to_string(static_cast<int>(req.adjacency)) + "," +
                 std::to_string(req.predId.val) + "," +
                 std::to_string(req.multiplicity) + ";";
        }
        return ans;
      }
      throw CanonicalizationError("invalid variant type");
    }
  }


  // ---------------------- PREDICATE REGISTRY ---------------------------
  PredicateId PredicateRegistry::intern(RelPredicate predicate)
  {
    std::string key = predicate_key::predicateKey(predicate);

    if (auto it = byKey.find(key);
        it != byKey.end())
    {
      return it->second;
    }

    // generate new id for the predicate
    PredicateId new_id{static_cast<int64_t>(predicates.size())};

    predicates.push_back(std::move(predicate));
    byKey.emplace(std::move(key), new_id);

    return new_id;
  }

  size_t PredicateRegistry::size() const
  {
    return byKey.size();
  }

  const RelPredicate &PredicateRegistry::operator[](PredicateId id) const
  {
    return predicates.at(
        static_cast<size_t>(id.val));
  }

  const std::vector<RelPredicate> &PredicateRegistry::entries() const
  {
    return predicates;
  }

}