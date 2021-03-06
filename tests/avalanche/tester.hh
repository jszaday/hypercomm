#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/components/mailbox.hpp>
#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/persistent_port.hpp>

#include "tester.decl.h"

using namespace hypercomm;

template <typename T>
using mailbox_val_t = typename mailbox<T>::value_type;

template <typename T>
struct matcher : public mailbox<T>::predicate_type::element_type {
  T goal;

  matcher(const T& _1) : goal(_1) {}

  virtual bool action(const mailbox_val_t<T>& value) override {
    return this->goal == value->value();
  }

  virtual void __pup__(serdes& s) override { s | goal; }
};

struct nil_callback : public callback {
  nil_callback(void) = default;

  virtual void send(deliverable&& dev) override {}

  virtual void __pup__(serdes& s) override {}
};

#endif
