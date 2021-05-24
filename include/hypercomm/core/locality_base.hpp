#ifndef __HYPERCOMM_CORE_LOCALITY_BASE_HPP__
#define __HYPERCOMM_CORE_LOCALITY_BASE_HPP__

#include "forwarding_callback.hpp"
#include "immediate.hpp"

#include "../messaging/messaging.hpp"

#include "../sections.hpp"
#include "../components.hpp"
#include "../reductions.hpp"
#include "../utilities.hpp"

namespace hypercomm {

using proxy_ptr = std::shared_ptr<hypercomm::proxy>;

using component_port_t = std::pair<component_id_t, components::port_id_t>;
using entry_port_map_t = comparable_map<entry_port_ptr, component_port_t>;
using component_map_t = std::unordered_map<component_id_t, component_ptr>;

template <typename Key>
using message_queue_t =
    comparable_map<Key, std::deque<std::shared_ptr<CkMessage>>>;

struct common_functions_ {
  virtual proxy_ptr __proxy__(void) const = 0;

  virtual const CkArrayIndex& __index_max__(void) const = 0;
};

template <typename Index>
struct locality_base : public virtual common_functions_ {
  message_queue_t<entry_port_ptr> port_queue;
  entry_port_map_t entry_ports;
  component_map_t components;
  component_id_t component_authority = 0;

  const Index& __index__(void) const {
    return reinterpret_index<Index>(this->__index_max__());
  }

  using action_type = typename std::shared_ptr<immediate_action<void(locality_base<Index>*)>>;
  using impl_index_type = array_proxy::index_type;

  using identity_t = section_identity<Index>;
  using section_ptr = typename identity_t::section_ptr;
  using section_type = typename section_ptr::element_type;

  using identity_ptr = std::shared_ptr<identity_t>;
  using identity_map_t = comparable_map<section_ptr, identity_ptr>;
  identity_map_t identities;

  using array_proxy_ptr = std::shared_ptr<array_proxy>;
  static void send_action(const array_proxy_ptr& p, const Index& i, action_type&& a);

  void receive_action(const action_type& ptr) {
    ptr->action(this);
  }

  void broadcast(const section_ptr&, hypercomm_msg*);

  void receive_value(const entry_port_ptr& port,
                     std::shared_ptr<CkMessage>&& value) {
    auto search = this->entry_ports.find(port);
    if (search == std::end(this->entry_ports)) {
      QdCreate(1);
      port_queue[port].push_back(std::move(value));
    } else {
      CkAssert(search->first && search->first->alive && "entry port must be alive");
      this->try_send(search->second, std::move(value));
      this->try_collect(search);
    }
  }

  using entry_port_iterator = typename decltype(entry_ports)::iterator;

  void try_collect(entry_port_iterator& it) {
    const auto& port = it->first;
    port->alive = port->keep_alive();
    if (!port->alive) {
      this->entry_ports.erase(it);
    }
  }

  void try_collect(const component_id_t& which) {
    this->try_collect(this->components[which]);
  }

  void try_collect(const component_ptr& ptr) {
    if (ptr && ptr->collectible()) {
      const auto& id = ptr->id;
      const auto& uses = ptr.use_count();
      if (uses > 1) {
        CkError("warning> component %lu replicated %lu time(s)!\n",
                id, uses - 1);
      }
      this->components.erase(id);
    }
  }

  void try_send(const component_port_t& port,
                std::shared_ptr<CkMessage>&& value) {
    auto search = components.find(port.first);
    CkAssert((search != components.end()) &&
             "message received for nonexistent component");

    search->second->receive_value(port.second, std::move(value));

    this->try_collect(search->second);
  }

  void resync_port_queue(entry_port_iterator& it) {
    const auto entry_port = it->first;
    auto search = port_queue.find(entry_port);
    if (search != port_queue.end()) {
      auto& buffer = search->second;
      while (entry_port->alive && !buffer.empty()) {
        auto& msg = buffer.front();
        this->try_send(it->second, std::move(msg));
        this->try_collect(it);
        buffer.pop_front();
        QdProcess(1);
      }
      if (buffer.empty()) {
        port_queue.erase(search);
      }
    }
  }

  void open(const entry_port_ptr& ours, const component_port_t& theirs) {
    ours->alive = true;
    auto pair = entry_ports.emplace(ours, theirs);
    CkAssert(pair.second && "entry port must be unique");
    this->resync_port_queue(pair.first);
  }

  template<typename T>
  void local_contribution(const T& which,
                          std::shared_ptr<CkMessage>&& value,
                          const combiner_ptr& fn, const callback_ptr& cb) {
    local_contribution(this->identity_for(which), std::move(value), fn, cb);
  }

protected:
  void local_contribution(const identity_ptr& ident,
                          std::shared_ptr<CkMessage>&& value,
                          const combiner_ptr& fn, const callback_ptr& cb) {
    auto next = ident->next_reduction();
    auto ustream = ident->upstream();
    auto dstream = ident->downstream();

    const auto& rdcr = this->emplace_component<reducer>(fn, std::move(value));

    for (const auto& up : ustream) {
      auto ours = std::make_shared<reduction_port<Index>>(next, up);
      auto theirs = rdcr->open_in_port();

      this->open(ours, std::make_pair(rdcr->id, theirs));
    }

    if (dstream.empty()) {
      rdcr->open_out_port(cb);
    } else {
      auto collective =
          std::dynamic_pointer_cast<array_proxy>(this->__proxy__());
      CkAssert(collective && "locality must be a valid collective");
      auto theirs = std::make_shared<reduction_port<Index>>(next, ident->mine);
      for (const auto& down : dstream) {
        impl_index_type down_idx;
        down_idx.dimension = 1;
        reinterpret_index<Index>(down_idx) = down;
        rdcr->open_out_port(std::make_shared<forwarding_callback>(
            (*collective)[down_idx], theirs));
      }
    }

    this->activate_component(rdcr);
  }

public:
  const component_ptr& emplace_component(component_ptr&& which) {
    auto placed = this->components.emplace(which->id, std::move(which));
    CkAssert(placed.second && "component id must be unique");
    return placed.first->second;
  }

  template <typename T, typename... Args>
  const component_ptr& emplace_component(Args... args) {
    auto next = this->component_authority++;
    return this->emplace_component(
        std::make_shared<T>(next, std::move(args)...));
  }

  void activate_component(const component_ptr& which) {
    which->alive = true;

    which->resync_status();

    this->try_collect(which);
  }

  const identity_ptr& identity_for(const section_ptr& which) {
    auto search = identities.find(which);
    if (search == identities.end()) {
      auto mine = this->__index__();
      auto iter = identities.emplace(
          which, std::make_shared<typename identity_ptr::element_type>(which, mine));
      CkAssert(iter.second && "section should be unique!");
      return (iter.first)->second;
    } else {
      return search->second;
    }
  }

  const identity_ptr& identity_for(const section_type& _1) {
    section_ptr which(const_cast<section_type*>(&_1), [](section_type*){});
    auto search = identities.find(which);
    if (search == identities.end()) {
      auto mine = this->__index__();
      auto ident = std::make_shared<typename identity_ptr::element_type>(*which, mine);
      auto iter = identities.emplace(ident->sect, std::move(ident));
      CkAssert(iter.second && "section should be unique!");
      return (iter.first)->second;
    } else {
      return search->second;
    }
  }

  inline const identity_ptr& identity_for(const std::vector<Index>& _1) {
    return this->identity_for(vector_section<Index>(_1));
  }
};

}

#endif
