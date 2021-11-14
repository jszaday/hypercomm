#ifndef __HYPERCOMM_CORE_LOCALITY_HPP__
#define __HYPERCOMM_CORE_LOCALITY_HPP__

#include "../sections.hpp"
#include "../components.hpp"
#include "../utilities.hpp"
#include "../reductions.hpp"

#include "generic_locality.hpp"
#include "locality_map.hpp"

#include "future.hpp"
#include "entry_port.hpp"
#include "broadcaster.hpp"
#include "port_opener.hpp"
#include "typed_value.hpp"

#include "../messaging/packing.hpp"
#include "../messaging/messaging.hpp"
#include "../messaging/interceptor.hpp"

#include "../components/comproxy.hpp"
#include "../core/forwarding_callback.hpp"

namespace hypercomm {

template <typename Index>
class indexed_locality_ : public generic_locality_ {
 public:
  using identity_type = identity<Index>;
  using identity_ptr = std::shared_ptr<identity_type>;

  using imprintable_type = imprintable<Index>;
  using imprintable_ptr = std::shared_ptr<imprintable_type>;

  using section_ptr = typename section_identity<Index>::section_ptr;
  using section_type = typename section_identity<Index>::section_type;

  using identity_map_t = comparable_map<imprintable_ptr, identity_ptr>;
  identity_map_t identities;

  using action_type =
      std::shared_ptr<immediate_action<void(indexed_locality_<Index>*)>>;

  using generic_action_type =
      std::shared_ptr<immediate_action<void(generic_locality_*)>>;

  virtual const Index& __index__(void) const = 0;

  virtual std::shared_ptr<generic_element_proxy> __element_at__(
      const Index&) const = 0;

  inline const identity_ptr& emplace_identity(const imprintable_ptr& which,
                                              const reduction_id_t& seed) {
    auto ins = this->identities.emplace(which, which->imprint(this, seed));
    CkAssertMsg(ins.second, "insertion did not occur");
    return (ins.first)->second;
  }

  inline const identity_ptr& identity_for(const imprintable_ptr& which) {
    auto search = this->identities.find(which);
    if (search != std::end(this->identities)) {
      return search->second;
    } else {
      return this->emplace_identity(which, {});
    }
  }

  inline const identity_ptr& identity_for(const section_type& src) {
    return this->identity_for(std::move(src.clone()));
  }

  inline const identity_ptr& identity_for(const std::vector<Index>& src) {
    return this->identity_for(vector_section<Index>(src));
  }
};

struct future_manager_ {
  future_id_t future_authority = 0;

  virtual future make_future(void) = 0;

  virtual void request_future(const future& f, const callback_ptr& cb) = 0;

  virtual bool check_future(const future&) const = 0;
};

template <typename Base, typename Index>
class vil : public detail::base_<Base, Index>, public future_manager_ {
  using idx_locality_ = indexed_locality_<Index>;

 public:
  using index_type = Index;
  using base_index_type = typename locality_base_::base_index_type;

  using section_ptr = typename idx_locality_::section_ptr;
  using identity_ptr = typename idx_locality_::identity_ptr;
  using imprintable_ptr = typename idx_locality_::imprintable_ptr;

  template <typename T, typename... Args>
  comproxy<T> emplace_component(Args... args) {
    auto next = ++(this->component_authority);
    auto inst = new T(next, std::move(args)...);
    this->components.emplace(next, inst);
    return static_cast<components::base_*>(inst)->id;
  }

  // NOTE ( generic collective proxy accessor method )
  virtual std::shared_ptr<generic_element_proxy> __element_at__(
      const Index& idx) const override {
    return make_proxy(CProxyElement_locality_base_(
        this->ckGetArrayID(), conv2idx<base_index_type>(idx)));
  }

  // NOTE ( this is a mechanism for remote task invocation )
  virtual void execute(CkMessage* _1) override {
    this->update_context();

    auto msg = utilities::wrap_message(_1);
    unpacker s(msg, utilities::get_message_buffer(msg));

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

  virtual const Index& __index__(void) const override {
    return reinterpret_index<Index>(this->__base_index__());
  }

  virtual future make_future(void) override {
    const auto next = ++this->future_authority;
    return future{.source = {this->ckGetArrayID(), this->thisIndexMax},
                  .id = next};
  }

  virtual void request_future(const future& f, const callback_ptr& cb) override;

  virtual bool check_future(const future&) const override;

  template <typename Action>
  inline void receive_action(const Action& ptr) {
    ptr->action(this);
  }

  void broadcast(const imprintable_ptr&, message*);

  inline void broadcast(const imprintable_ptr& section, const int& epIdx,
                        message* msg) {
    UsrToEnv(msg)->setEpIdx(epIdx);
    this->broadcast(section, msg);
  }

  template <typename T>
  void local_contribution(const T& which, deliverable&& value,
                          const combiner_ptr& fn, const callback_ptr& cb) {
    local_contribution(this->identity_for(which), std::move(value), fn, cb);
  }

 protected:
  void local_contribution(const identity_ptr& ident, deliverable&& value,
                          const combiner_ptr& fn, const callback_ptr& cb) {
    auto next = ident->next_reduction();
    auto ustream = ident->upstream();
    auto dstream = ident->downstream();
    auto stamp = std::make_pair(ident->get_imprintable(), next);

    const auto& rdcr = this->emplace_component<reducer>(
        stamp, fn, ustream.size() + 1, dstream.empty() ? 1 : dstream.size());

    for (const auto& up : ustream) {
      auto remote = std::make_shared<reduction_port<Index>>(stamp, up);
      this->connect(remote, rdcr, 0);
    }

    auto local = std::make_shared<reduction_port<Index>>(stamp, ident->mine());
    if (dstream.empty()) {
      rdcr->template output_to<0>(cb);
    } else {
      for (const auto& down : dstream) {
        auto downIdx = conv2idx<base_index_type>(down);
        auto fwd = forward_to(this->thisProxy[downIdx], local);
        rdcr->template output_to<0>(std::move(fwd));
      }
    }

    this->activate_component(rdcr);
    auto contrib = make_typed_value<contribution>(std::move(value), fn, cb);
    // TODO ( uncertain whether source setting is correct, needs testing )
    contrib->source = endpoint(std::move(local));
    this->components[rdcr]->accept(0, std::move(contrib));
  }
};

inline bool future::ready(void) const {
  auto* ctx = dynamic_cast<future_manager_*>(access_context_());
  return ctx->check_future(*this);
}

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
CkMessage* pack_action(const Action& a) {
  using arg_type = typename get_argument<Action>::type;
  constexpr auto is_act =
      is_base_of_template<arg_type, indexed_locality_>::value;
  constexpr auto is_gen = std::is_base_of<generic_locality_, arg_type>::value;
  static_assert(is_act || is_gen, "unrecognized action type");
  auto* msg = hypercomm::pack(is_act, a);
  UsrToEnv(msg)->setEpIdx(CkIndex_locality_base_::idx_execute_CkMessage());
  return msg;
}

template <typename Index>
void deliver(const element_proxy<Index>& proxy, message* msg) {
  const auto& base =
      static_cast<const CProxyElement_locality_base_&>(proxy.c_proxy());
  UsrToEnv(msg)->setEpIdx(CkIndex_locality_base_::idx_demux_CkMessage());
  interceptor::send_async(base, msg);
}

void generic_locality_::loopback(const entry_port_ptr& port,
                                 deliverable&& value) {
  auto elt = dynamic_cast<ArrayElement*>(this);
  CkAssert(elt != nullptr);
  value.update_endpoint(port);
  interceptor::send_async(elt->ckGetArrayID(), elt->ckGetArrayIndex(),
                          std::move(value));
}

template <typename Proxy,
          typename = typename std::enable_if<std::is_base_of<
              CProxyElement_locality_base_, Proxy>::value>::type>
inline void send2port(const Proxy& proxy, const entry_port_ptr& port,
                      deliverable&& value) {
  interceptor::send_async(proxy, port, std::move(value));
}

// NOTE this should always be used for invalidations
template <typename Index>
inline void send2port(const element_ptr<Index>& proxy,
                      const entry_port_ptr& port, deliverable&& value) {
  const auto& base =
      static_cast<const CProxyElement_locality_base_&>(proxy->c_proxy());
  send2port(base, port, std::move(value));
}

template <typename Index>
inline void broadcast_to(const proxy_ptr& proxy,
                         const std::shared_ptr<imprintable<Index>>& section,
                         const int& epIdx, message* msg) {
  UsrToEnv(msg)->setEpIdx(epIdx);
  broadcast_to(proxy, section, msg);
}

template <typename Index>
inline void broadcast_to(const proxy_ptr& proxy,
                         const std::shared_ptr<imprintable<Index>>& section,
                         message* msg) {
  // TODO ( do not assume array index )
  using base_index_type = CkArrayIndex;
  auto action =
      std::make_shared<broadcaster<base_index_type, Index>>(section, msg);
  interceptor::send_to_root((const CProxy_ArrayBase&)proxy->c_proxy(), section,
                            pack_action(action));
}

template <typename Base, typename Index>
void vil<Base, Index>::broadcast(const imprintable_ptr& section, message* msg) {
  auto action =
      std::make_shared<broadcaster<base_index_type, Index>>(section, msg);
  interceptor::send_to_root(this->thisProxy, section, pack_action(action));
}

template <typename Base, typename Index>
void vil<Base, Index>::request_future(const future& f, const callback_ptr& cb) {
  auto ourElt = this->thisProxy[this->thisIndexMax];
  auto ourPort = std::make_shared<future_port>(f);

  // TODO ( check whether future immediately fulfilled! )
  this->open(ourPort, cb);

  auto& theirElt = f.source;
  if (theirElt && !(ourElt == theirElt)) {
    // open a remote port that forwards to this locality
    auto fwd = forward_to(std::move(ourElt), ourPort);
    auto opener = std::make_shared<port_opener>(ourPort, std::move(fwd));
    interceptor::send_async(theirElt, pack_action(opener));
  }
}

template <typename Base, typename Index>
bool vil<Base, Index>::check_future(const future& f) const {
  return this->has_value(std::make_shared<future_port>(f));
}

void forwarding_callback<CkArrayIndex>::send(callback::value_type&& value) {
  const auto& base =
      static_cast<const CProxyElement_locality_base_&>(this->proxy->c_proxy());
  value.update_endpoint(this->ep);
  interceptor::send_async(base, std::move(value));
}
}  // namespace hypercomm

#include "wait_any.hpp"

#endif
