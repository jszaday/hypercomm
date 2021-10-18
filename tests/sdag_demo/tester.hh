#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include <hypercomm/components/mailbox.hpp>
#include <hypercomm/core/persistent_port.hpp>
#include <hypercomm/components/sentinel.hpp>

#include "tester.decl.h"

using namespace hypercomm;

template<typename... Ts>
struct test_component : public component<std::tuple<Ts...>, std::tuple<>> {
  using parent_t = component<std::tuple<Ts...>, std::tuple<>>;
  using in_set = typename parent_t::in_set;

  test_component(const id_t& id_): parent_t(id_) {}

  virtual std::tuple<> action(in_set& set) override {
#if CMK_VERBOSE
    CkPrintf("com%lu> i was invoked\n", this->id);
#endif
    return {};
  }
};

#endif
