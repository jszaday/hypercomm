#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include <hypercomm/components/mailbox.hpp>
#include <hypercomm/core/persistent_port.hpp>

#include "tester.decl.h"

using namespace hypercomm;

template<typename T>
struct matcher : public mailbox<T>::predicate_type::element_type {
  T goal;

  matcher(const T& _1): goal(_1) {}

  virtual bool action(const T& value) override {
    return value == goal;
  }

  virtual void __pup__(serdes& s) override {
    s | goal;
  }
};

struct nil_callback : public callback {
  nil_callback(void) = default;

  virtual void send(value_type&& value) override {}

  virtual void __pup__(serdes& s) override {}

};

#endif
