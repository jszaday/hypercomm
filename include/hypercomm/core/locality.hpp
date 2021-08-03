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
#include "../messaging/interceptor.hpp"

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

template <typename Index>
class indexed_locality_ : public generic_locality_ {
 public:
  using section_ptr = typename section_identity<Index>::section_ptr;
  using section_type = typename section_ptr::element_type;

  using identity_type = identity<Index>;
  using identity_ptr = std::shared_ptr<identity_type>;

  using action_type =
      std::shared_ptr<immediate_action<void(indexed_locality_<Index>*)>>;

  using generic_action_type =
      std::shared_ptr<immediate_action<void(generic_locality_*)>>;

  virtual std::shared_ptr<proxy> __gencon__(void) const = 0;

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
  using this_ptr_ = locality_bridge_<Base>*;

 public:
  using base_index_type = CkArrayIndex;

  inline const CkArrayIndex& __base_index__(void) const {
    return this->ckGetArrayIndex();
  }

  inline collective_ptr<CkArrayIndex> __proxy__(void) const {
    return hypercomm::make_proxy(const_cast<this_ptr_>(this)->thisProxy);
  }

  inline element_ptr<CkArrayIndex> __element__(void) const {
    return hypercomm::make_proxy(
        (const_cast<this_ptr_>(this)->thisProxy)[this->__base_index__()]);
  }
};

template <typename Base, typename Index>
class vil : public locality_bridge_<Base>,
            public indexed_locality_<Index>,
            public future_manager_ {
  using idx_locality_ = indexed_locality_<Index>;

 public:
  using index_type = Index;
  using base_index_type = typename locality_bridge_<Base>::base_index_type;

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

  // NOTE ( generic collective proxy accessor method )
  virtual std::shared_ptr<proxy> __gencon__(void) const {
    return std::static_pointer_cast<proxy>(this->__proxy__());
  }

  // NOTE ( this is a mechanism for remote task invocation )
  virtual void execute(CkMessage* _1) override {
    this->update_context();

    auto msg = utilities::wrap_message(_1);
    auto s = serdes::make_unpacker(msg, utilities::get_message_buffer(msg));

    bool typed;
    s | typed;

    if (typed) {
      typename indexed_locality_<Index>::action_type action{};
      s | action;
      this->receive_action(action);
    } else {
      typename indexed_locality_<Index>::generic_action_type action{};
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

  using imprintable_ptr = std::shared_ptr<imprintable<Index>>;

  void broadcast(const imprintable_ptr&, hypercomm_msg*);

  inline void broadcast(const imprintable_ptr& section, const int& epIdx,
                        hypercomm_msg* msg) {
    UsrToEnv(msg)->setEpIdx(epIdx);
    this->broadcast(section, msg);
  }

  template <typename T>
  void local_contribution(const T& which, component::value_type&& value,
                          const combiner_ptr& fn, const callback_ptr& cb) {
    local_contribution(which->imprint(this), std::move(value), fn, cb);
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
        auto downIdx = conv2idx<base_index_type>(down);
        auto fwd = forward_to(this->thisProxy[downIdx], theirs);
        this->connect(rdcr, count++, std::move(fwd));
      }
    }

    this->activate_component(rdcr);
    this->components[rdcr]->receive_value(0, std::move(value));
  }
};

template <typename T, typename Enable = void>
struct get_argument;

template <typename T>
struct get_argument<immediate_action<void(T*)>> {
  using type = T;
};

template <typename T>
struct get_argument<T, typename std::enable_if<is_base_of_template<
                           T, immediate_action>::value>::type> {
  template <typename A>
  static A get_argument_impl(const immediate_action<void(A)>*);
  using ptr_type = decltype(get_argument_impl(std::declval<T*>()));
  using type = typename std::remove_pointer<ptr_type>::type;
};

template <typename T>
struct get_argument<std::shared_ptr<T>> {
  using type = typename get_argument<T>::type;
};

template <typename Action>
void send_action(const std::shared_ptr<generic_element_proxy>& p,
                 const Action& a) {
  using arg_type = typename get_argument<Action>::type;
  constexpr auto is_act =
      is_base_of_template<arg_type, indexed_locality_>::value;
  constexpr auto is_gen = std::is_base_of<generic_locality_, arg_type>::value;
  static_assert(is_act || is_gen, "unrecognized action type");

  auto& c_proxy = const_cast<CProxy&>(p->c_proxy());
  auto msg = hypercomm::pack(is_act, a);

  (static_cast<CProxyElement_locality_base_&>(c_proxy)).execute(msg);
}

template <typename BaseIndex, typename Action>
void send_action(const collective_ptr<BaseIndex>& p, const BaseIndex& i,
                 const Action& a) {
  send_action((*p)[i], a);
}

template <typename Index>
void deliver(const element_proxy<Index>& proxy, message* msg) {
  const auto& base =
      static_cast<const CProxyElement_locality_base_&>(proxy.c_proxy());
  UsrToEnv(msg)->setEpIdx(CkIndex_locality_base_::demux(nullptr));
  interceptor::send_async(base.ckGetArrayID(), base.ckGetIndex(), msg);
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
  UsrToEnv(msg)->setEpIdx(CkIndex_locality_base_::demux(nullptr));
  interceptor::send_async(aid, idx, msg);
}

template <typename Proxy,
          typename = typename std::enable_if<std::is_base_of<
              CProxyElement_locality_base_, Proxy>::value>::type>
inline void send2port(const Proxy& proxy, const entry_port_ptr& port,
                      component::value_type&& value) {
  auto* msg = repack_to_port(port, std::move(value));
  UsrToEnv(msg)->setEpIdx(CkIndex_locality_base_::demux(nullptr));
  interceptor::send_async(proxy.ckGetArrayID(), proxy.ckGetIndex(), msg);
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

template <typename Index>
inline void broadcast_to(const proxy_ptr& proxy,
                         const std::shared_ptr<imprintable<Index>>& section,
                         const int& epIdx, hypercomm_msg* msg) {
  UsrToEnv(msg)->setEpIdx(epIdx);
  broadcast_to(proxy, section, msg);
}

template <typename Index>
inline void broadcast_to(const proxy_ptr& proxy,
                         const std::shared_ptr<imprintable<Index>>& section,
                         hypercomm_msg* msg) {
  // TODO ( do not assume array index )
  using base_index_type = CkArrayIndex;
  auto root = section->pick_root(proxy);
  auto action =
      std::make_shared<broadcaster<base_index_type, Index>>(root, section, msg);
  auto collective =
      std::dynamic_pointer_cast<collective_proxy<base_index_type>>(proxy);
  send_action(collective, conv2idx<base_index_type>(root), action);
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
void vil<Base, Index>::broadcast(const imprintable_ptr& section,
                                 hypercomm_msg* msg) {
  auto proxy = this->__proxy__();
  auto mine = this->__index__();
  auto root = section->pick_root(proxy, &mine);
  auto action =
      std::make_shared<broadcaster<base_index_type, Index>>(root, section, msg);

  if (root == mine) {
    this->receive_action(action);
  } else {
    auto rootIdx = conv2idx<base_index_type>(root);
    send_action(proxy, rootIdx, action);
  }
}

template <typename Base, typename Index>
void vil<Base, Index>::request_future(const future& f, const callback_ptr& cb) {
  auto ourPort = std::make_shared<future_port>(f);
  auto ourElement = this->__element__();

  this->open(ourPort, cb);

  auto home = std::dynamic_pointer_cast<generic_element_proxy>(f.source);
  if (home && !home->equals(*ourElement)) {
    // open a remote port that forwards to this locality
    auto fwd = forward_to(std::move(ourElement), ourPort);
    auto opener = std::make_shared<port_opener>(ourPort, std::move(fwd));
    send_action(home, opener);
  }
}

template <typename Index>
void forwarding_callback<Index>::send(callback::value_type&& value) {
  send2port(this->proxy, this->port, std::move(value));
}
}

#include <hypercomm/core/locality.def.h>

#endif
