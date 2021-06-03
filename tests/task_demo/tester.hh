#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/persistent_port.hpp>

#include "tester.decl.h"

using namespace hypercomm;

template<typename T>
void unpack_array(const component::value_type& _1, std::size_t** n, T** arr) {
  auto msg = (message*)std::dynamic_pointer_cast<plain_value>(_1)->msg;
  *n = (std::size_t*)msg->payload;
  *arr = (T*)(msg->payload + sizeof(n));
}

template<typename T>
struct gen_values : public hypercomm::component {
  std::size_t n;
  int selfIdx;

  gen_values(const id_t& _1, const int& _2, const std::size_t& _3) : component(_1), selfIdx(_2), n(_3) {}

  virtual std::size_t n_inputs(void) const override { return 0; }

  virtual std::size_t n_outputs(void) const override { return 3; }

  virtual value_set action(value_set&&) override;
};

template<typename T>
struct add_values : public hypercomm::component {
  add_values(const id_t& _1) : component(_1) {}

  virtual std::size_t n_inputs(void) const override { return 2; }

  virtual std::size_t n_outputs(void) const override { return 1; }

  virtual value_set action(value_set&&) override;
};

template<typename T>
struct print_values : public hypercomm::component {
  int selfIdx;

  print_values(const id_t& _1, const int& _2) : component(_1), selfIdx(_2) {}

  virtual std::size_t n_inputs(void) const override { return 1; }

  virtual std::size_t n_outputs(void) const override { return 0; }

  virtual value_set action(value_set&&) override;
};

#endif
