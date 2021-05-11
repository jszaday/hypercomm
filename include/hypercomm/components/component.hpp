#ifndef __HYPERCOMM_COMPONENTS_COMPONENT_HPP__
#define __HYPERCOMM_COMPONENTS_COMPONENT_HPP__

#include <charm++.h>

#include <map>
#include <memory>
#include <vector>

#include "../core.hpp"
#include "identifiers.hpp"

namespace hypercomm {
namespace components {
class manager;

struct component {
  using value_t = std::shared_ptr<CkMessage>;
  using id_t = component_id_t;

  friend class manager;

  virtual value_t action(void) = 0;
  virtual int num_expected(void) const;

  virtual void receive_invalidation(const port_id_t&);
  virtual void receive_value(const port_id_t&, value_t&&);

  int num_available(void) const;
  bool ready(void) const;
  bool collectible(void) const;

  port_id_t open_in_port(void);
  port_id_t open_out_port(const callback_ptr&);

 protected:
  virtual void send(value_t&& msg);
  void connection(const id_t& from, const id_t& to);
  void invalidation(const id_t& from);
  void erase_incoming(const id_t& from);

 public:
  static bool is_placeholder(const component::id_t& id);
  static void send_value(const id_t& from, const id_t& to, value_t&& msg);
  static void send_invalidation(const id_t& from, const id_t& to);
  static void send_connection(const placeholder&, const id_t&);
  static void connect(const placeholder&, const std::shared_ptr<component>&);
  static void connect(const std::shared_ptr<component>& from,
                      const std::shared_ptr<component>& to);

  static void activate(std::shared_ptr<component>&&);
  static void generate_identity(component::id_t& id);

  id_t port_authority = std::numeric_limits<id_t>::max() << (sizeof(id_t) / 2);
  std::vector<id_t> incoming;
  std::vector<id_t> outgoing;
  std::vector<value_t> accepted;
  std::map<id_t, value_t> inbox;
  std::map<id_t, value_t> outbox;
  bool alive;
  id_t id;
};
}
}

#endif
