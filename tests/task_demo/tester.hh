#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
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
struct gen_random : public hypercomm::component {
  std::size_t n;
  gen_random(const id_t& _1, const std::size_t& _2) : component(_1), n(_2) {}
  virtual int num_expected(void) const override { return 0; }
  virtual value_t action(void) override;
};

template<typename T>
struct triple_values : public hypercomm::component {
  triple_values(const id_t& _1) : component(_1) {}
  virtual int num_expected(void) const override { return 1; }
  virtual value_t action(void) override;
};

template<typename T>
struct print_values : public hypercomm::component {
  print_values(const id_t& _1) : component(_1) {}
  virtual int num_expected(void) const override { return 1; }
  virtual value_t action(void) override;
};

#endif
