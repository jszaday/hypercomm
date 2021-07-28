#ifndef __HYPERCOMM_CORE_LOCALITY_HPP__
#define __HYPERCOMM_CORE_LOCALITY_HPP__

#include "../sections.hpp"
#include "../components.hpp"
#include "../utilities.hpp"
#include "../reductions.hpp"

#include "generic_locality.hpp"
#include "locality_map.hpp"

#include "broadcaster.hpp"
#include "port_opener.hpp"

#include "../messaging/packing.hpp"
#include "../messaging/messaging.hpp"

#include "../components/comproxy.hpp"
#include "../core/forwarding_callback.hpp"

struct locality_base_ : public CBase_locality_base_ {
  locality_base_(void) {}

  virtual void demux(hypercomm_msg* msg) {
    throw std::runtime_error("you should never see this!");
  }

  virtual void execute(CkMessage* msg) {
    throw std::runtime_error("you should never see this!");
  }
};

namespace hypercomm {

template <typename BaseIndex, typename Index>
class indexed_locality_ : public generic_locality_ {
 public:
  using index_type = Index;
  using base_index_type = BaseIndex;

  using section_ptr = typename section_identity<Index>::section_ptr;
  using section_type = typename section_ptr::element_type;

  using identity_type = identity<Index>;
  using identity_ptr = std::shared_ptr<identity_type>;

  using action_type = typename std::shared_ptr<
      immediate_action<void(indexed_locality_<BaseIndex, Index>*)>>;

  using generic_action_type =
      typename std::shared_ptr<immediate_action<void(generic_locality_*)>>;

  static void send_action(const collective_ptr<BaseIndex>& p,
                          const BaseIndex& i, const action_type& a);

  static void send_action(const collective_ptr<BaseIndex>& p,
                          const BaseIndex& i, const generic_action_type& a);

  virtual element_ptr<BaseIndex> __element__(void) const = 0;
  virtual collective_ptr<BaseIndex> __proxy__(void) const = 0;

  virtual const identity_ptr& identity_for(const section_ptr& which) = 0;

  inline const identity_ptr& identity_for(const section_type& src) {
    return this->identity_for(std::move(src.clone()));
  }

  inline const identity_ptr& identity_for(const std::vector<Index>& src) {
    return this->identity_for(vector_section<Index>(src));
  }
};

template <typename Base, typename Enable = void>
class locality_bridge_;

template <typename Base>
class locality_bridge_<Base,
                       typename std::enable_if<is_array_element<Base>()>::type>
    : public Base {
 public:
  const CkArrayIndex& __base_index__(void) const {
    return this->ckGetArrayIndex();
  }
};

template <typename Base, typename Index>
class vil : public locality_bridge_<Base>,
            public indexed_locality_<impl_index_t<Base>, Index>,
            public future_manager_ {
  using this_ptr_ = vil<Base, Index>*;
  using idx_locality_ = indexed_locality_<impl_index_t<Base>, Index>;

 public:
  using index_type = Index;
  using base_index_type = typename idx_locality_::base_index_type;

  using section_ptr = typename idx_locality_::section_ptr;
  using identity_ptr = typename idx_locality_::identity_ptr;

  using identity_map_t = comparable_map<section_ptr, identity_ptr>;
  identity_map_t identities;

  template <typename T, typename... Args>
  comproxy<T> emplace_component(Args... args) {
    auto next = this->component_authority++;
    auto inst = new T(next, std::move(args)...);
    this->components.emplace(next, inst);
    return ((component*)inst)->id;
  }

  virtual collective_ptr<base_index_type> __proxy__(void) const override {
    return hypercomm::make_proxy(const_cast<this_ptr_>(this)->thisProxy);
  }

  virtual element_ptr<base_index_type> __element__(void) const override {
    return hypercomm::make_proxy(
        (const_cast<this_ptr_>(this)->thisProxy)[this->__base_index__()]);
  }

  // NOTE ( this is a mechanism for remote task invocation )
  virtual void execute(CkMessage* _1) override {
    this->update_context();

    auto msg = utilities::wrap_message(_1);
    auto s = serdes::make_unpacker(msg, utilities::get_message_buffer(msg));

    bool typed;
    s | typed;

    if (typed) {
      typename indexed_locality_<base_index_type, Index>::action_type action{};
      s | action;
      this->receive_action(action);
    } else {
      typename indexed_locality_<base_index_type, Index>::generic_action_type
          action{};
      s | action;
      this->receive_action(action);
    }
  }

  /* NOTE ( this is a mechanism for demux'ing an incoming message
 *        to the appropriate entry port )
 */
  virtual void demux(hypercomm_msg* _1) override {
    this->update_context();
    auto msg = _1->is_null() ? std::shared_ptr<hyper_value>(
                                   nullptr, [_1](void*) { CkFreeMsg(_1); })
                             : msg2value(_1);
    this->receive_value(_1->dst, std::move(msg));
  }

  const Index& __index__(void) const {
    return reinterpret_index<Index>(this->__base_index__());
  }

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

  template <typename T>
  void local_contribution(const T& which, component::value_type&& value,
                          const combiner_ptr& fn, const callback_ptr& cb) {
    local_contribution(this->identity_for(which), std::move(value), fn, cb);
  }

  virtual const identity_ptr& identity_for(const section_ptr& which) override {
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

 protected:
  void local_contribution(const identity_ptr& ident,
                          component::value_type&& value, const combiner_ptr& fn,
                          const callback_ptr& cb) {
    auto next = ident->next_reduction();
    auto ustream = ident->upstream();
    auto dstream = ident->downstream();

    const auto& rdcr = this->emplace_component<hypercomm::reducer>(
        next, fn, ustream.size() + 1, dstream.empty() ? 1 : dstream.size());

    auto count = 0;
    for (const auto& up : ustream) {
      auto ours = std::make_shared<reduction_port<Index>>(next, up);
      this->connect(ours, rdcr, ++count);
    }

    if (dstream.empty()) {
      this->connect(rdcr, 0, cb);
    } else {
      auto theirs =
          std::make_shared<reduction_port<Index>>(next, ident->mine());

      count = 0;
      for (const auto& down : dstream) {
        auto fwd = forward_to(this->thisProxy[conv2idx<base_index_type>(down)], theirs);
        this->connect(rdcr, count++, std::move(fwd));
      }
    }

    this->activate_component(rdcr);
    this->components[rdcr]->receive_value(0, std::move(value));
  }
};

template <typename BaseIndex, typename Index>
/* static */ void indexed_locality_<BaseIndex, Index>::send_action(
    const collective_ptr<BaseIndex>& p, const BaseIndex& i,
    const action_type& a) {
  const auto& thisCollection =
      static_cast<const CProxy_locality_base_&>(p->c_proxy());
  auto msg = hypercomm::pack(true, a);
  (const_cast<CProxy_locality_base_&>(thisCollection))[i].execute(msg);
}

template <typename BaseIndex, typename Index>
/* static */ void indexed_locality_<BaseIndex, Index>::send_action(
    const collective_ptr<BaseIndex>& p, const BaseIndex& i,
    const generic_action_type& a) {
  const auto& thisCollection =
      static_cast<const CProxy_locality_base_&>(p->c_proxy());
  auto msg = hypercomm::pack(false, a);
  (const_cast<CProxy_locality_base_&>(thisCollection))[i].execute(msg);
}

template <typename Index>
void deliver(const element_proxy<Index>& proxy, message* msg) {
  const auto& base =
      static_cast<const CProxyElement_locality_base_&>(proxy.c_proxy());
  const_cast<CProxyElement_locality_base_&>(base).demux(msg);
}

message* repack_to_port(const entry_port_ptr& port,
                        component::value_type&& value) {
  auto msg = value ? static_cast<message*>(value->release())
                   : hypercomm_msg::make_null_message(port);
  auto env = UsrToEnv(msg);
  auto msgIdx = env->getMsgIdx();
  if (msgIdx == message::__idx) {
    // TODO should this be a move?
    msg->dst = port;

    return msg;
  } else {
    // TODO repack to hypercomm in this case (when HYPERCOMM_NO_COPYING is
    // undefined)
    CkAbort("expected a hypercomm msg, but got %s instead\n",
            _msgTable[msgIdx]->name);
  }
}

void generic_locality_::loopback(message* msg) {
  CkArrayID aid = UsrToEnv(msg)->getArrayMgr();
  auto id = ((CkArrayMessage*)msg)->array_element_id();
  auto idx = aid.ckLocalBranch()->getLocMgr()->lookupIdx(id);
  (CProxy_locality_base_(aid))[idx].demux(msg);
}

template <typename Proxy, typename = typename std::enable_if<std::is_base_of<CProxyElement_locality_base_, Proxy>::value>::type>
inline void send2port(const Proxy& proxy, const entry_port_ptr& port, component::value_type&& value) {
  auto* msg = repack_to_port(port, std::move(value));
  const_cast<CProxyElement_locality_base_&>(proxy).demux(msg);
}

// NOTE this should always be used for invalidations
template <typename Index>
inline void send2port(const element_ptr<Index>& proxy,
                      const entry_port_ptr& port,
                      component::value_type&& value) {
  const auto& base =
      static_cast<const CProxyElement_locality_base_&>(proxy->c_proxy());
  send2port(base, port, std::move(value));
}

inline void send2port(const std::shared_ptr<generic_element_proxy>& proxy,
                      const entry_port_ptr& port,
                      component::value_type&& value) {
  proxy->receive(repack_to_port(port, std::move(value)));
}

inline void send2future(const future& f, component::value_type&& value) {
  auto src = std::dynamic_pointer_cast<generic_element_proxy>(f.source);
  CkAssert(src && "future must be from a locality!");
  auto port = std::make_shared<future_port>(f);
  send2port(src, port, std::move(value));
}

template <typename Proxy, typename Index>
inline void broadcast_to(
    const Proxy& proxy,
    const std::shared_ptr<section<std::int64_t, Index>>& section,
    const int& epIdx, hypercomm_msg* msg) {
  UsrToEnv(msg)->setEpIdx(epIdx);

  broadcast_to(make_proxy(proxy), section, msg);
}

template <typename Index>
inline void broadcast_to(
    const proxy_ptr& proxy,
    const std::shared_ptr<section<std::int64_t, Index>>& section,
    hypercomm_msg* msg) {
  auto root = section->index_at(0);
  using base_index_type = array_proxy::index_type;
  auto action =
      std::make_shared<broadcaster<base_index_type, Index>>(section, msg);
  auto collective = std::dynamic_pointer_cast<array_proxy>(proxy);
  indexed_locality_<base_index_type, Index>::send_action(
      collective, conv2idx<base_index_type>(root), action);
}

void generic_locality_::receive_message(hypercomm_msg* msg) {
  auto* env = UsrToEnv(msg);
  auto idx = env->getEpIdx();

  if (idx == CkIndex_locality_base_::demux(nullptr)) {
    this->receive_value(msg->dst, msg2value(msg));
  } else {
    _entryTable[idx]->call(msg, dynamic_cast<CkMigratable*>(this));
  }
}

template <typename Base, typename Index>
void vil<Base, Index>::broadcast(const section_ptr& section,
                                 hypercomm_msg* msg) {
  auto root = section->index_at(0);
  auto action =
      std::make_shared<broadcaster<base_index_type, Index>>(section, msg);

  if (root == this->__index__()) {
    this->receive_action(action);
  } else {
    auto collective = std::dynamic_pointer_cast<array_proxy>(this->__proxy__());
    indexed_locality_<base_index_type, Index>::send_action(
        collective, conv2idx<base_index_type>(root), action);
  }
}

template <typename Base, typename Index>
void vil<Base, Index>::request_future(const future& f, const callback_ptr& cb) {
  auto ourPort = std::make_shared<future_port>(f);
  auto ourElement = this->__element__();

  this->open(ourPort, cb);

  auto& source = *f.source;
  if (!ourElement->equals(source)) {
    auto theirElement = dynamic_cast<element_proxy<base_index_type>*>(&source);
    auto fwd = std::make_shared<forwarding_callback<base_index_type>>(ourElement, ourPort);
    auto opener = std::make_shared<port_opener>(ourPort, fwd);
    indexed_locality_<base_index_type, Index>::send_action(
        theirElement->collection(), theirElement->index(), opener);
  }
}

template<typename Index>
void forwarding_callback<Index>::send(callback::value_type&& value) {
  send2port(this->proxy, this->port, std::move(value));
}
}

#include <hypercomm/core/locality.def.h>

#endif
