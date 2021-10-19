#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/persistent_port.hpp>

#include "tester.decl.h"

using namespace hypercomm;

template<typename T>
struct gen_values : public hypercomm::component<std::tuple<>, std::tuple<T, T, T>> {
  using parent_t = hypercomm::component<std::tuple<>, std::tuple<T, T, T>>;
  using in_set = typename parent_t::in_set;
  using out_set = typename parent_t::out_set;

  std::size_t n;
  int selfIdx;

  gen_values(const id_t& _1, const int& _2, const std::size_t& _3) : parent_t(_1), selfIdx(_2), n(_3) {}

  virtual out_set action(in_set&) override;
};

template<typename T>
struct add_values : public hypercomm::component<std::tuple<T, T>, T> {
  using parent_t = hypercomm::component<std::tuple<T, T>, T>;
  using in_set = typename parent_t::in_set;
  using out_set = typename parent_t::out_set;

  add_values(const id_t& _1) : parent_t(_1) {}

  virtual out_set action(in_set&) override;
};

template<typename T>
struct print_values : public hypercomm::component<std::tuple<T>, std::tuple<>> {
  using parent_t = hypercomm::component<std::tuple<T>, std::tuple<>>;

  int selfIdx;

  print_values(const id_t& _1, const int& _2) : parent_t(_1), selfIdx(_2) {}

  virtual std::tuple<> action(std::tuple<typed_value_ptr<T>>&) override;
};

#endif
