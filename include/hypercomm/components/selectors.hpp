#ifndef __HYPERCOMM_COMPONENTS_SELECTORS_HPP__
#define __HYPERCOMM_COMPONENTS_SELECTORS_HPP__

#include "attributes.hpp"

namespace hypercomm {
namespace components {

struct mux_component : public virtual passthru_component {
  virtual bool screen(const std::shared_ptr<CkMessage>&) const = 0;

  virtual void accept(const component::id_t& from,
                      std::shared_ptr<CkMessage>&& msg) {
    if (this->screen(msg)) {
      component::accept(from, std::move(msg));
    } else if (this->incoming.empty()) {
      CkAbort("a multiplexer must accept at least one value");
    }
  }
};

struct demux_component : public virtual passthru_component {
  virtual id_t route(const std::shared_ptr<CkMessage>&) const = 0;

  virtual std::shared_ptr<CkMessage> action(void) override {
    auto msg = passthru_component::action();
    auto chosen = this->route(msg);
    bool found = false;

    while (!this->outgoing.empty()) {
      const component::id_t to = this->outgoing.back();
      this->outgoing.pop_back();

      if (chosen == to) {
        found = true;
      } else {
        component::send_invalidation(this->id, to);
      }
    }

    CkAssert(found && "did not find selected component among outgoing");
    this->outgoing.push_back(chosen);

    return msg;
  }
};

}
}

#endif
