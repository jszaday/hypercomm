
#ifndef __HYPERCOMM_REDUCTIONS_REDN_PORT_HPP__
#define __HYPERCOMM_REDUCTIONS_REDN_PORT_HPP__

#include "../core/entry_port.hpp"
#include "../components/identifiers.hpp"
#include "../serialization/pup.hpp"

namespace hypercomm {

template <typename Index>
struct reduction_port : public entry_port<reduction_port<Index>> {
  reduction_id_t id;
  Index index;

  reduction_port(PUP::reconstruct) {}

  reduction_port(const reduction_id_t& _1, const Index& _2)
      : id(_1), index(_2) {}

  virtual bool equals(const std::shared_ptr<comparable>& other) const override  {
    auto theirs = std::dynamic_pointer_cast<reduction_port<Index>>(other);
    return theirs && this->id == theirs->id && this->index == theirs->index;
  }

  virtual std::string to_string(void) const override {
    std::stringstream ss;
    ss << "reduction_port(id=" << this->id << ",idx=" << this->index << ")";
    return ss.str();
  }

  virtual hash_code hash(void) const override  {
    auto hashId = utilities::hash<reduction_id_t>()(this->id);
    auto hashIdx = utilities::hash<Index>()(this->index);
    return hash_combine(hashId, hashIdx);
  }

  virtual void __pup__(serdes& s) override {
    s | id;
    s | index;
  }
};

}

#endif
