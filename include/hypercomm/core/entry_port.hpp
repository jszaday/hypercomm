#ifndef __HYPERCOMM_ENTRY_PORT_HPP__
#define __HYPERCOMM_ENTRY_PORT_HPP__

#include "../components/status_listener.hpp"

#include "value.hpp"
#include "comparable.hpp"
#include "impermanent.hpp"

namespace hypercomm {

struct entry_port_base : public virtual polymorph,
                         public virtual comparable,
                         public virtual impermanent,
                         public virtual value_source,
                         public virtual components::status_listener {
  virtual std::string to_string(void) const = 0;
};

using entry_port_ptr = std::shared_ptr<entry_port_base>;

extern void locally_invalidate_(const entry_port_ptr&);

template <typename T>
struct entry_port : public entry_port_base,
                    public std::enable_shared_from_this<T> {
  virtual void on_completion(const component&) override {
    locally_invalidate_(this->shared_from_this());
  }

  virtual void on_invalidation(const component&) override {
    locally_invalidate_(this->shared_from_this());
  }

  // implemented in generic_locality.hpp
  virtual void take_back(std::shared_ptr<hyper_value>&& value) override;
};
}

#endif
