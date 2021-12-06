#ifndef __HYPERCOMM_COMPARABLE_HPP__
#define __HYPERCOMM_COMPARABLE_HPP__

#include "math.hpp"
#include "../components/identifiers.hpp"

#include "../serialization/polymorph.hpp"

namespace hypercomm {

struct hashable : public virtual polymorph::trait {
  virtual hash_code hash(void) const = 0;
};

struct comparable : public virtual hashable {
  virtual bool equals(const std::shared_ptr<comparable>& other) const = 0;
};
}  // namespace hypercomm

#include "../utilities.hpp"

namespace hypercomm {
template <typename T>
struct comparable_comparator {
  inline bool operator()(const T& lhs, const T& rhs) const {
    auto lhsRaw = lhs.get(), rhsRaw = rhs.get();
    return (lhsRaw == rhsRaw) || (lhsRaw && rhsRaw && lhsRaw->equals(rhs));
  }
};

template <typename Key, typename Value>
using comparable_map =
    hash_map<Key, Value, utilities::hash<Key>, comparable_comparator<Key>>;
}  // namespace hypercomm

#endif
