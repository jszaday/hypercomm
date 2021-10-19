#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/typed_value.hpp>
#include <hypercomm/core/null_combiner.hpp>
#include <hypercomm/core/persistent_port.hpp>

#include "tester.decl.h"

using namespace hypercomm;

template<typename T>
struct entry_method_like : public hypercomm::component<std::tuple<T>, std::tuple<>> {
  using parent_t = hypercomm::component<std::tuple<T>, std::tuple<>>;
  entry_method_like(const id_t& _1) : parent_t(_1) {
    this->persistent = true;
  }
};

struct say_hello : public entry_method_like<deliverable> {
  say_hello(const id_t& _1) : entry_method_like(_1) {}
  virtual std::tuple<> action(std::tuple<deliverable>&) override;
};

struct my_redn_com : public entry_method_like<std::tuple<array_proxy::index_type, int>> {
  locality* self;
  my_redn_com(const id_t& _1, locality* _2) : entry_method_like(_1), self(_2) {}
  using in_set = std::tuple<typed_value_ptr<std::tuple<array_proxy::index_type, int>>>;
  virtual std::tuple<> action(in_set&) override;
};

#endif
