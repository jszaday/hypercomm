#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/typed_value.hpp>
#include <hypercomm/core/persistent_port.hpp>
#include <hypercomm/components/mailbox.hpp>
#include <hypercomm/components/multistate_component.hpp>

#include "tester.decl.h"

using server_type = hypercomm::state_server<hypercomm::varstack>;

// 1 input port (int) and 0 outputs
struct accumulator_two_com
    : public hypercomm::multistate_component<int, std::tuple<>> {
  using parent_t = hypercomm::multistate_component<int, std::tuple<>>;
  using in_set_t = typename parent_t::in_set;

  accumulator_two_com(hypercomm::component_id_t id,
                      const std::shared_ptr<server_type>& server)
      : parent_t(id) {
    this->set_server(server);
  }

  virtual std::tuple<> action(in_set_t& in_set) override;
};

// find values matching a predicate
template <typename T>
struct screener : public hypercomm::immediate_action<bool(
                      const hypercomm::typed_value_ptr<T>&)> {
  T goal;

  screener(T goal_) : goal(goal_) {}

  virtual bool action(const hypercomm::typed_value_ptr<T>& val) override {
    return (this->goal == val->value());
  }

  virtual void __pup__(hypercomm::serdes& s) override { s | this->goal; }
};

#endif
