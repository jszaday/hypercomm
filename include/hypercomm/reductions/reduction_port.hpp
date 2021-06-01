
#ifndef __HYPERCOMM_REDUCTIONS_REDN_PORT_HPP__
#define __HYPERCOMM_REDUCTIONS_REDN_PORT_HPP__

#include "reducer.hpp"
#include "../core/entry_port.hpp"

namespace hypercomm {

template <typename Index>
struct reduction_port : public virtual entry_port {
  reduction_id_t id;
  Index index;

  reduction_port(PUP::reconstruct) {}

  reduction_port(const reduction_id_t& _1, const Index& _2)
      : id(_1), index(_2) {}

  virtual bool equals(const std::shared_ptr<comparable>& other) const override  {
    auto theirs = std::dynamic_pointer_cast<reduction_port<Index>>(other);
    return this->id == theirs->id && this->index == theirs->index;
  }

  virtual std::string to_string(void) const override {
    std::stringstream ss;
    ss << "reduction_port(id=" << this->id << ",idx=" << this->idx << ")";
    return ss.str();
  }

  virtual hash_code hash(void) const override  {
    return hash_combine(hash_code(id), hash_code(index));
  }

  virtual void __pup__(serdes& s) override {
    s | id;
    s | index;
  }
};

}

#endif
