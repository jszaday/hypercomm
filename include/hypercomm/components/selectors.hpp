#ifndef __HYPERCOMM_COMPONENTS_SELECTORS_HPP__
#define __HYPERCOMM_COMPONENTS_SELECTORS_HPP__

#include "attributes.hpp"

namespace hypercomm {
namespace components {

struct mux_component : public virtual passthru_component {
  virtual bool select(const value_type&) const = 0;

  virtual void receive_value(const port_id_t& dst, value_type&& msg) override {
    if (this->select(msg)) {
      component::receive_value(dst, std::move(msg));
    }
  }
};

struct demux_component : public virtual passthru_component {
  virtual port_id_t select(const value_type&) const = 0;

  virtual void send(value_type&& msg) override {
    auto chosen = this->select(msg);
    bool found = false;

    for (const auto& dst : this->outgoing) {
      if (dst.first == chosen) {
        this->try_send(dst, std::move(msg));
      } else {
        this->try_send(dst, std::move(value_type{}));
      }
    }

    this->outgoing.clear();
  }
};

}
}

#endif
