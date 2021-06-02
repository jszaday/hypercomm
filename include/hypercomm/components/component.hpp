#ifndef __HYPERCOMM_COMPONENTS_COMPONENT_HPP__
#define __HYPERCOMM_COMPONENTS_COMPONENT_HPP__

#include <charm++.h>

#include <map>
#include <memory>
#include <vector>

#include "../core.hpp"
#include "../core/impermanent.hpp"

#include "identifiers.hpp"

namespace hypercomm {
namespace components {
class manager;

struct component: virtual public impermanent {
  using value_type = core::value_ptr;
  using id_t = component_id_t;

 protected:
  std::vector<port_id_t> incoming;
  std::map<port_id_t, callback_ptr> outgoing;

  std::vector<value_type> accepted;
  std::map<port_id_t, value_type> inbox;
  std::map<port_id_t, value_type> outbox;

  port_id_t port_authority = 0;

  component(const id_t& _1): id(_1) {}

 public:
  id_t id;

  friend class manager;

  virtual value_type action(void) = 0;

  // NOTE -- for correct status updates, overrides should
  //         call the parent
  virtual void receive_invalidation(const port_id_t&);
  virtual void receive_value(const port_id_t&, value_type&&);

  int num_available(void) const;
  virtual int num_expected(void) const;

  bool ready(void) const;
  bool collectible(void) const;

  port_id_t open_in_port(void) {
    auto id = this->port_authority++;
    this->incoming.push_back(id);
    return id;
  }

  port_id_t open_out_port(const callback_ptr& cb) {
    auto id = ++this->port_authority;
    this->outgoing.emplace(id, cb);
    return id;
  }

  void update_destination(const port_id_t& port, const callback_ptr& cb) {
    auto s1 = this->outgoing.find(port);
    if (s1 == this->outgoing.end()) {
      auto s2 = this->outbox.find(port);
      CkAssert((s2 != this->outbox.end()) &&
               "cannot update cb of non-existent port");
      cb->send(std::move(s2->second));
      this->outbox.erase(s2);
    } else {
      s1->second = cb;
    }
  }

  // TODO decide whether to sync outbox?
  void resync_queues(void) {
    for (auto& in : this->inbox) {
      auto search = std::find(std::begin(this->incoming),
                              std::end(this->incoming), in.first);
      if (search != std::end(this->incoming)) {
        QdProcess(1);
        this->receive_value(in.first, std::move(in.second));
        this->inbox.erase(in.first);
      }
    }
  }

  // TODO take threading into consideration when launching
  void resync_status(void) {
    if (this->ready()) {
      QdProcess(this->accepted.size());
      auto msg = this->action();
      this->alive = this->keep_alive();
      this->send(std::move(msg));
    }
  }

 protected:
  virtual void send(value_type&& msg);

  void erase_incoming(const port_id_t&);

  void try_send(const decltype(outgoing)::value_type& dst, value_type&& msg) {
    if (dst.second) {
      dst.second->send(std::forward<value_type>(msg));
    } else {
      this->outbox.emplace(dst.first, std::forward<value_type>(msg));
    }
  }
};
}

using component = components::component;
using component_ptr = std::shared_ptr<components::component>;
}

#endif
