#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/persistent_port.hpp>

#include "tester.decl.h"

using namespace hypercomm;

template<typename T>
struct connector: public callback {
  const locality_base<T>* self;
  const component_port_t dst;

  connector(const locality_base<T>* _1, const component_port_t& _2): self(_1), dst(_2) {}

  virtual return_type send(argument_type&& value) override {
    const_cast<locality_base<T>*>(self)->try_send(dst, std::move(value));
  }

  virtual void __pup__(serdes& s) override {
    CkAbort("don't send me");
  }
};

template<typename T>
void connect(const locality_base<T>* self, const component_ptr& src, const component_ptr& dst) {
  auto in = dst->open_in_port();
  auto conn = std::make_shared<connector<T>>(self, std::make_pair(dst->id, in));
  auto out = src->open_out_port(conn);
}

template<typename T>
void connect(const locality_base<T>* self, const entry_port_ptr& port, const component_ptr& dst) {
  auto in = dst->open_in_port();
  const_cast<locality_base<T>*>(self)->open(port, std::make_pair(dst->id, in));
}

template<typename T>
void forward(const locality_base<T>* self, const component_ptr& src, const proxy_ptr& proxy, const entry_port_ptr& port) {
  auto fwd = std::make_shared<forwarding_callback>(proxy, port);
  auto out = src->open_out_port(fwd);
}

template<typename T>
void unpack_array(const std::shared_ptr<CkMessage>& _1, std::size_t** n, T** arr) {
  auto msg = std::static_pointer_cast<message>(_1);
  *n = (std::size_t*)msg->payload;
  *arr = (T*)(msg->payload + sizeof(n));
}

template<typename T>
struct gen_values : public hypercomm::component {
  std::size_t n;
  int selfIdx;

  gen_values(const id_t& _1, const int& _2, const std::size_t& _3) : component(_1), selfIdx(_2), n(_3) {}
  virtual int num_expected(void) const override { return 0; }
  virtual value_t action(void) override;
};

template<typename T>
struct add_values : public hypercomm::component {
  add_values(const id_t& _1) : component(_1) {}
  virtual int num_expected(void) const override { return 2; }
  virtual value_t action(void) override;
};

template<typename T>
struct print_values : public hypercomm::component {
  int selfIdx;

  print_values(const id_t& _1, const int& _2) : component(_1), selfIdx(_2) {}
  virtual int num_expected(void) const override { return 1; }
  virtual value_t action(void) override;
};

#endif
