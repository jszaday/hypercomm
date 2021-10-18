
#ifndef __HYPERCOMM_REDUCTIONS_REDN_PORT_HPP__
#define __HYPERCOMM_REDUCTIONS_REDN_PORT_HPP__

#include "../core/entry_port.hpp"
#include "reducer.hpp"

namespace hypercomm {

template <typename Index>
class reduction_port : public entry_port {
 public:
  hash_code identity;  // TODO use something better here
  reduction_id_t count;
  Index index;

  reduction_port(PUP::reconstruct) {}

  reduction_port(const reducer::pair_type& _1, const Index& _2)
      : identity(utilities::hash<imprintable_ptr>()(_1.first)),
        count(_1.second),
        index(_2) {}

  // a port is affected by a stamp if it was issued at or after it
  bool affected_by(const stamp_type& stamp) const {
    auto fn = stamp.hash_function();
    for (const auto& entry : stamp) {
      if (this->identity == fn(entry.first)) {
        return this->count >= entry.second;
      }
    }
    return false;
  }

  virtual bool equals(const std::shared_ptr<comparable>& other) const override {
    auto theirs = std::dynamic_pointer_cast<reduction_port<Index>>(other);
    return theirs && this->identity == theirs->identity &&
           this->count == theirs->count && this->index == theirs->index;
  }

  virtual std::string to_string(void) const override {
    std::stringstream ss;
    ss << "reduction_port(count=" << this->count << ",idx=" << this->index
       << ")";
    return ss.str();
  }

  virtual hash_code hash(void) const override {
    auto hashId = utilities::hash<reduction_id_t>()(this->count);
    auto hashIdx = utilities::hash<Index>()(this->index);
    return hash_combine(this->identity, hash_combine(hashId, hashIdx));
  }

  virtual void __pup__(serdes& s) override {
    s | identity;
    s | count;
    s | index;
  }
};
}  // namespace hypercomm

#endif
