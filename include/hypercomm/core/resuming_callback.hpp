#ifndef __HYPERCOMM_CORE_RESUMER_HPP__
#define __HYPERCOMM_CORE_RESUMER_HPP__

#include "callback.hpp"
#include "typed_value.hpp"

namespace hypercomm {
template <typename T>
struct resuming_callback : public core::callback {
  using type = T;
  using value_type = core::callback::value_type;
  using result_type = typed_value<type>;

  CthThread th;
  std::shared_ptr<result_type> result;

  resuming_callback(PUP::reconstruct) {}

  resuming_callback(const CthThread& _1) : th(_1) {}

  virtual void send(value_type&& value) override {
    this->result = value2typed<type>(std::move(value));

    if (th) {
      CthAwaken(th);
    }
  }

  inline bool ready(void) const { return (bool)this->result; }

  inline type& value(void) { return this->result->value(); }

  inline const type& value(void) const { return this->result->value(); }

  virtual void __pup__(serdes& s) override {
    throw std::runtime_error("not yet implemented");
  }
};
}  // namespace hypercomm

#endif
