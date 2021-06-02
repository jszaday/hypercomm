#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/persistent_port.hpp>

#include "tester.decl.h"

using namespace hypercomm;

void forward(const component_ptr& src, const proxy_ptr& proxy, const entry_port_ptr& port) {
  auto fwd = std::make_shared<forwarding_callback>(proxy, port);
  auto out = src->open_out_port(fwd);
}

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
  virtual int num_expected(void) const override { return 0; }
  virtual value_type action(void) override;
};

template<typename T>
struct add_values : public hypercomm::component {
  add_values(const id_t& _1) : component(_1) {}
  virtual int num_expected(void) const override { return 2; }
  virtual value_type action(void) override;
};

template<typename T>
struct print_values : public hypercomm::component {
  int selfIdx;

  print_values(const id_t& _1, const int& _2) : component(_1), selfIdx(_2) {}
  virtual int num_expected(void) const override { return 1; }
  virtual value_type action(void) override;
};

#endif
