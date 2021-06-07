#ifndef __HYPERCOMM_CORE_LOCALITY_BASE_HPP__
#define __HYPERCOMM_CORE_LOCALITY_BASE_HPP__

#include "../sections.hpp"
#include "../components.hpp"
#include "../reductions.hpp"
#include "../utilities.hpp"

#include "../messaging/messaging.hpp"

#include "../core/forwarding_callback.hpp"

#include "future.hpp"
#include "generic_locality.hpp"

namespace hypercomm {

using proxy_ptr = std::shared_ptr<hypercomm::proxy>;

template <typename Index>
using element_ptr = std::shared_ptr<hypercomm::element_proxy<Index>>;

template <typename Index>
using collective_ptr = std::shared_ptr<hypercomm::collective_proxy<Index>>;

template <typename Key>
using message_queue_t = comparable_map<Key, std::deque<component::value_type>>;

template <typename Index>
struct common_functions_ {
  virtual element_ptr<Index> __element__(void) const = 0;
  virtual collective_ptr<Index> __proxy__(void) const = 0;

  // TODO this should probably be renamed "true_index"
  virtual const Index& __index_max__(void) const = 0;
};

template <typename Index>
struct locality_base : public generic_locality_, public virtual common_functions_<CkArrayIndex> {
  message_queue_t<entry_port_ptr> port_queue;

  future_id_t future_authority = 0;
  component_id_t component_authority = 0;

  const Index& __index__(void) const {
    return reinterpret_index<Index>(this->__index_max__());
  }

  using action_type =
      typename std::shared_ptr<immediate_action<void(locality_base<Index>*)>>;
  using impl_index_type = array_proxy::index_type;

  using identity_t = section_identity<Index>;
  using section_ptr = typename identity_t::section_ptr;
  using section_type = typename section_ptr::element_type;

  using identity_ptr = std::shared_ptr<identity_t>;
  using identity_map_t = comparable_map<section_ptr, identity_ptr>;
  identity_map_t identities;

  using array_proxy_ptr = std::shared_ptr<array_proxy>;

  // TODO make this more generic
  static void send_action(const collective_ptr<CkArrayIndex>& p,
                          const CkArrayIndex& i, const action_type& a);
  static void send_future(const future& f, component::value_type&& value);

  future make_future(void) {
    const auto next = ++this->future_authority;
    return future{.source = this->__element__(), .id = next};
  }

  void request_future(const future& f, const callback_ptr& cb);

  void receive_action(const action_type& ptr) { ptr->action(this); }

  void broadcast(const section_ptr&, hypercomm_msg*);

  void receive_value(const entry_port_ptr& port,
                     component::value_type&& value) {
    auto search = this->entry_ports.find(port);
    if (search == std::end(this->entry_ports)) {
      QdCreate(1);
      port_queue[port].push_back(std::move(value));
    } else {
      CkAssert(search->first && search->first->alive &&
               "entry port must be alive");
      this->try_send(search->second, std::move(value));
      // this->try_collect(search);
    }
  }

  /* TODO consider introducing a simplified connection API that
   *      utilizes "port authorities", aka port id counters, to
   *      remove src/dstPort for trivial, unordered connections
   */

  inline void connect(const component_ptr& src,
                      const components::port_id_t& srcPort,
                      const component_ptr& dst,
                      const components::port_id_t& dstPort) {
    src->update_destination(srcPort, this->make_connector(dst, dstPort));
  }

  inline void connect(const component_ptr& src,
                      const components::port_id_t& srcPort,
                      const callback_ptr& cb) {
    src->update_destination(srcPort, cb);
  }

  inline void connect(const entry_port_ptr& srcPort, const component_ptr& dst,
                      const components::port_id_t& dstPort) {
    dst->add_listener(srcPort);

    this->open(srcPort, std::make_pair(dst->id, dstPort));
  }

  using entry_port_iterator = typename decltype(entry_ports)::iterator;

  // void try_collect(entry_port_iterator& it) {
  //   const auto& port = it->first;
  //   port->alive = port->keep_alive();
  //   if (!port->alive) {
  //     this->entry_ports.erase(it);
  //   }
  // }

  void try_collect(const component_id_t& which) {
    this->try_collect(this->components[which]);
  }

  void try_collect(const component_ptr& ptr) {
    if (ptr && ptr->collectible()) {
      const auto& id = ptr->id;
#if CMK_VERBOSE
      const auto& uses = ptr.use_count();
      if (uses > 1) {
        CkError("warning> component %lu replicated %lu time(s)!\n", id,
                uses - 1);
      }
#endif
      this->components.erase(id);
    }
  }

  void try_send(const destination_& dest, component::value_type&& value) {
    switch (dest.type) {
      case destination_::type_::kCallback: {
        const auto& cb = dest.cb;
        CkAssert(cb && "callback must be valid!");
        cb->send(std::move(value));
        break;
      }
      case destination_::type_::kComponentPort:
        this->try_send(dest.port, std::move(value));
        break;
      default:
        CkAbort("unknown destination type");
    }
  }

  virtual void try_send(const component_port_t& port, component::value_type&& value) override {
    auto search = components.find(port.first);
#if CMK_ERROR_CHECKING
    if (search == std::end(components)) {
      std::stringstream ss;
      ss << "vil" << this->__index__()
         << "> recvd msg for invalid destination com" << port.first << ":"
         << port.second << "!";
      CkAbort("%s", ss.str().c_str());
    }
#endif

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
        // this->try_collect(it);
        buffer.pop_front();
        QdProcess(1);
      }
      if (buffer.empty()) {
        port_queue.erase(search);
      }
    }
  }

  template <typename Destination>
  void open(const entry_port_ptr& ours, const Destination& theirs) {
    ours->alive = true;
    auto pair = this->entry_ports.emplace(ours, theirs);
#if CMK_ERROR_CHECKING
    if (!pair.second) {
      std::stringstream ss;
      ss << "[";
      for (const auto& epp : this->entry_ports) {
        const auto& other_port = epp.first;
        if (comparable_comparator()(ours, other_port)) {
          ss << "{" << other_port->to_string() << "}, ";
        } else {
          ss << other_port->to_string() << ", ";
        }
      }
      ss << "]";

      CkAbort("fatal> adding non-unique port %s to:\n\t%s\n",
              ours->to_string().c_str(), ss.str().c_str());
    }
#endif
    this->resync_port_queue(pair.first);
  }

  template <typename T>
  void local_contribution(const T& which, std::shared_ptr<CkMessage>&& value,
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

    const auto& rdcr = this->emplace_component<reducer>(
        fn, ustream.size() + 1, dstream.empty() ? 1 : dstream.size());

    auto count = 0;
    for (const auto& up : ustream) {
      auto ours = std::make_shared<reduction_port<Index>>(next, up);
      this->connect(ours, rdcr, ++count);
    }

    if (dstream.empty()) {
      this->connect(rdcr, 0, cb);
    } else {
      auto collective =
          std::dynamic_pointer_cast<array_proxy>(this->__proxy__());
      CkAssert(collective && "locality must be a valid collective");
      auto theirs = std::make_shared<reduction_port<Index>>(next, ident->mine);

      count = 0;
      for (const auto& down : dstream) {
        impl_index_type down_idx;
        down_idx.dimension = 1;
        reinterpret_index<Index>(down_idx) = down;
        this->connect(rdcr, count++, std::make_shared<forwarding_callback>(
                                         (*collective)[down_idx], theirs));
      }
    }

    this->activate_component(rdcr);
    rdcr->receive_value(0, msg2value(std::move(value)));
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
    which->activate();

    this->try_collect(which);
  }

  const identity_ptr& identity_for(const section_ptr& which) {
    auto search = identities.find(which);
    if (search == identities.end()) {
      auto mine = this->__index__();
      auto iter = identities.emplace(
          which,
          std::make_shared<typename identity_ptr::element_type>(which, mine));
      CkAssert(iter.second && "section should be unique!");
      return (iter.first)->second;
    } else {
      return search->second;
    }
  }

  const identity_ptr& identity_for(const section_type& _1) {
    section_ptr which(const_cast<section_type*>(&_1), [](section_type*) {});
    auto search = identities.find(which);
    if (search == identities.end()) {
      auto mine = this->__index__();
      auto ident =
          std::make_shared<typename identity_ptr::element_type>(*which, mine);
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
