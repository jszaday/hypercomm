#ifndef __HYPERCOMM_PERSISTENT_PORT_HPP__
#define __HYPERCOMM_PERSISTENT_PORT_HPP__

#include "entry_port.hpp"
#include "../components.hpp"

namespace hypercomm {
struct persistent_port : public virtual entry_port {
  components::port_id_t id = 0;

  persistent_port(PUP::reconstruct) {}
  persistent_port(const decltype(id)& _1): id(_1) {}

  virtual std::string to_string(void) const override {
    std::stringstream ss;
    ss << "persistent_port(id=" << this->id << ")";
    return ss.str();
  }

  virtual bool keep_alive(void) const override { return true; }

  virtual bool equals(const std::shared_ptr<comparable>& other) const override {
    auto theirs = std::dynamic_pointer_cast<persistent_port>(other);
    return this->id == theirs->id;
  }

  virtual hash_code hash(void) const override  {
    return hash_code(id);
  }

  virtual void __pup__(serdes& s) override  {
    s | id;
  }
};
}

#endif
