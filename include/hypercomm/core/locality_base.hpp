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
#include "../components/comproxy.hpp"

namespace hypercomm {

using proxy_ptr = std::shared_ptr<hypercomm::proxy>;

template <typename Index>
using element_ptr = std::shared_ptr<hypercomm::element_proxy<Index>>;

template <typename Index>
using collective_ptr = std::shared_ptr<hypercomm::collective_proxy<Index>>;

template <typename Index>
struct common_functions_ {
  virtual element_ptr<Index> __element__(void) const = 0;
  virtual collective_ptr<Index> __proxy__(void) const = 0;

  // TODO this should probably be renamed "true_index"
  virtual const Index& __index_max__(void) const = 0;
};

struct future_manager_ {
  virtual future make_future(void) = 0;

  virtual void request_future(const future& f, const callback_ptr& cb) = 0;
};

template <typename Index>
struct locality_base : public generic_locality_,
                       public future_manager_,
                       public virtual common_functions_<CkArrayIndex> {
  future_id_t future_authority = 0;
  component_id_t component_authority = 0;

  const Index& __index__(void) const {
    return reinterpret_index<Index>(this->__index_max__());
  }

  using action_type =
      typename std::shared_ptr<immediate_action<void(locality_base<Index>*)>>;
  using generic_action_type =
      typename std::shared_ptr<immediate_action<void(generic_locality_*)>>;

  using impl_index_type = array_proxy::index_type;

  using identity_type = identity<Index>;
  using section_ptr = typename section_identity<Index>::section_ptr;
  using section_type = typename section_ptr::element_type;

  using identity_ptr = std::shared_ptr<identity_type>;
  using identity_map_t = comparable_map<section_ptr, identity_ptr>;
  identity_map_t identities;

  using array_proxy_ptr = std::shared_ptr<array_proxy>;

  // TODO make this more generic
  static void send_action(const collective_ptr<CkArrayIndex>& p,
                          const CkArrayIndex& i, const action_type& a);
  static void send_action(const collective_ptr<CkArrayIndex>& p,
                          const CkArrayIndex& i, const generic_action_type& a);

  virtual future make_future(void) override {
    const auto next = ++this->future_authority;
    return future{.source = this->__element__(), .id = next};
  }

  virtual void request_future(const future& f, const callback_ptr& cb) override;

  template <typename Action>
  inline void receive_action(const Action& ptr) {
    ptr->action(this);
  }

  void broadcast(const section_ptr&, hypercomm_msg*);

  inline void broadcast(const section_ptr& section, const int& epIdx,
                        hypercomm_msg* msg) {
    UsrToEnv(msg)->setEpIdx(epIdx);
    this->broadcast(section, msg);
  }

  void receive_message(hypercomm_msg* msg);

  void try_collect(const component_id_t& which) {
    this->try_collect(this->components[which]);
  }

  using component_type = typename decltype(components)::mapped_type;

  void try_collect(const component_type& com) {
    if (com && com->collectible()) {
      this->components.erase(com->id);
    }
  }

  virtual void try_send(const destination_& dest,
                        component::value_type&& value) override {
    switch (dest.type) {
      case destination_::type_::kCallback: {
        const auto& cb = dest.cb();
        CkAssert(cb && "callback must be valid!");
        cb->send(std::move(value));
        break;
      }
      case destination_::type_::kComponentPort:
        this->try_send(dest.port(), std::move(value));
        break;
      default:
        CkAbort("unknown destination type");
    }
  }

  void try_send(const component_port_t& port, component::value_type&& value) {
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

  template <typename T>
  void local_contribution(const T& which, component::value_type&& value,
                          const combiner_ptr& fn, const callback_ptr& cb) {
    local_contribution(this->identity_for(which), std::move(value), fn, cb);
  }

 protected:
  void local_contribution(const identity_ptr& ident,
                          component::value_type&& value, const combiner_ptr& fn,
                          const callback_ptr& cb) {
    auto next = ident->next_reduction();
    auto ustream = ident->upstream();
    auto dstream = ident->downstream();

    const auto& rdcr = this->emplace_component<reducer>(
         next, fn, ustream.size() + 1, dstream.empty() ? 1 : dstream.size());

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
      auto theirs =
          std::make_shared<reduction_port<Index>>(next, ident->mine());

      count = 0;
      for (const auto& down : dstream) {
        auto down_idx = conv2idx<impl_index_type>(down);
        this->connect(rdcr, count++, std::make_shared<forwarding_callback>(
                                         (*collective)[down_idx], theirs));
      }
    }

    this->activate_component(rdcr);
    this->components[rdcr]->receive_value(0, std::move(value));
  }

 public:
  template <typename T, typename... Args>
  comproxy<T> emplace_component(Args... args) {
    auto next = this->component_authority++;
    auto inst = new T(next, std::move(args)...);
    this->components.emplace(next, inst);
    return ((component*)inst)->id;
  }

  void activate_component(const component_id_t& id) {
    auto search = this->components.find(id);
    if (search != std::end(this->components)) {
      if (this->invalidated(id)) {
        this->components.erase(search);
      } else {
        auto& com = search->second;
        com->activate();
        this->try_collect(com);
      }
    } else {
      CkAbort("fatal> unable to find com%lu.\n", id);
    }
  }

  const identity_ptr& identity_for(const section_ptr& which) {
    auto search = identities.find(which);
    if (search == identities.end()) {
      auto mine = this->__index__();
      auto iter = identities.emplace(
          which, std::make_shared<section_identity<Index>>(which, mine));
      CkAssert(iter.second && "section should be unique!");
      return (iter.first)->second;
    } else {
      return search->second;
    }
  }

  inline const identity_ptr& identity_for(const section_type& src) {
    return this->identity_for(std::move(src.clone()));
  }

  inline const identity_ptr& identity_for(const std::vector<Index>& src) {
    return this->identity_for(vector_section<Index>(src));
  }
};
}

#endif
