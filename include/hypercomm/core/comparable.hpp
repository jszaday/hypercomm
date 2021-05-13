#ifndef __HYPERCOMM_COMPARABLE_HPP__
#define __HYPERCOMM_COMPARABLE_HPP__

#include <unordered_map>

#include "math.hpp"
#include "../serialization/polymorph.hpp"

namespace hypercomm {

struct hashable : public virtual polymorph::trait {
  virtual hash_code hash(void) const = 0;
};

struct comparable : public virtual hashable {
  virtual bool equals(const std::shared_ptr<comparable>& other) const = 0;
};

struct hashable_hasher {
  inline hash_code operator()(const std::shared_ptr<hashable>& value) const {
    if (value) {
      return hash_combine(typeid(*value).hash_code(), value->hash());
    } else {
      return (hash_code)0;
    }
  }
};

struct comparable_comparator {
  inline bool operator()(const std::shared_ptr<comparable>& lhs,
                         const std::shared_ptr<comparable>& rhs) const {
    return (typeid(*lhs) == typeid(*rhs)) && lhs->equals(rhs);
  }
};

template <typename Key, typename Value>
using comparable_map =
    std::unordered_map<Key, Value, hashable_hasher, comparable_comparator>;

}

#endif
