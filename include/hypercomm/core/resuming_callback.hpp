#ifndef __HYPERCOMM_CORE_RESUMER_HPP__
#define __HYPERCOMM_CORE_RESUMER_HPP__

#include "callback.hpp"
#include "typed_value.hpp"

namespace hypercomm {
template <typename T>
struct resuming_callback : public core::callback {
  using type = T;

  CthThread th;
  typed_value_ptr<T> result;

  resuming_callback(void) : th(nullptr) {}

  using value_type = core::callback::value_type;
  virtual void send(value_type&& value) override {
    this->result = dev2typed<type>(std::move(value));
    if (this->th) {
      CthAwaken(this->th);
      this->th = nullptr;
    }
  }

  void wait(void) {
    if (!this->ready()) {
      CkAssert(this->th == nullptr);
      this->th = CthSelf();
      CkAssert(!CthIsMainThread(this->th));
      CthSuspend();
    }
  }

  inline bool ready(void) const { return (bool)this->result; }

  inline type& value(void) { return this->result->value(); }

  inline const type& value(void) const { return this->result->value(); }

  virtual void __pup__(serdes& s) override { NOT_IMPLEMENTED; }
};
}  // namespace hypercomm

#endif
