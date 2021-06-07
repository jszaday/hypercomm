#ifndef __HYPERCOMM_ENTRY_PORT_HPP__
#define __HYPERCOMM_ENTRY_PORT_HPP__

#include "../components.hpp"

namespace hypercomm {

struct entry_port;

inline void locally_invalidate_(entry_port&);

struct entry_port : public virtual polymorph,
                    public virtual comparable,
                    public virtual impermanent,
                    public virtual component::status_listener {
  virtual std::string to_string(void) const = 0;

  virtual void on_completion(const component&) override {
    locally_invalidate_(*this);
  }

  virtual void on_invalidation(const component&) override {
    locally_invalidate_(*this);
  }
};

using entry_port_ptr = std::shared_ptr<entry_port>;
}

#endif
