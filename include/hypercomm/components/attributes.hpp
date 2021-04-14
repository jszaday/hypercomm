#ifndef __HYPERCOMM_COMPONENTS_ATTRIBUTES_HPP__
#define __HYPERCOMM_COMPONENTS_ATTRIBUTES_HPP__

#include "component.hpp"

namespace hypercomm {
namespace components {

struct threaded_component : public virtual component {};

template<int N>
struct n_input_component : public virtual component {
  virtual int num_expected(void) const { return N; }
};

struct passthru_component : public virtual n_input_component<1> {
  virtual std::shared_ptr<CkMessage> action(void) override {
    std::shared_ptr<CkMessage> msg = std::move(this->accepted[0]);
    this->accepted.clear();
    return msg;
  }
};

}
}

#endif