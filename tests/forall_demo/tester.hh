#ifndef __TESTER_HH__
#define __TESTER_HH__

#include <hypercomm/core/locality.hpp>
#include <hypercomm/core/typed_value.hpp>
#include <hypercomm/core/persistent_port.hpp>
#include <hypercomm/components/link.hpp>
#include <hypercomm/components/mailbox.hpp>
#include <hypercomm/components/multistate_component.hpp>

#include "tester.decl.h"

using server_type = hypercomm::state_server<hypercomm::microstack_base>;

struct test_multistate_component
    : public hypercomm::multistate_component<int, std::tuple<>> {
  using parent_t = hypercomm::multistate_component<int, std::tuple<>>;
  using in_set_t = typename parent_t::in_set;

  test_multistate_component(hypercomm::component_id_t id,
                            const std::shared_ptr<server_type>& server)
      : parent_t(id) {
    this->set_server(server);
  }

  virtual std::tuple<> action(in_set_t& in_set) override { return {}; }
};

struct test_component : public hypercomm::component<int, std::tuple<>> {
  using parent_t = hypercomm::component<int, std::tuple<>>;
  using in_set_t = typename parent_t::in_set;

  test_component(hypercomm::component_id_t id) : parent_t(id) {}

  virtual std::tuple<> action(in_set_t& in_set) override { return {}; }
};

#endif
