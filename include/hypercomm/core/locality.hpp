#ifndef __HYPERCOMM_CORE_LOCALITY_HPP__
#define __HYPERCOMM_CORE_LOCALITY_HPP__

#include "../messaging/packing.hpp"

#include "broadcaster.hpp"
#include "port_opener.hpp"
#include "locality_map.hpp"

struct locality_base_
    : public CBase_locality_base_,
      public virtual hypercomm::common_functions_<CkArrayIndex> {
  using this_ptr = locality_base_ *;

  locality_base_(void) {}

  virtual void demux(hypercomm_msg *msg) {
    throw std::runtime_error("you should never see this!");
  }

  virtual void execute(CkMessage *msg) {
    throw std::runtime_error("you should never see this!");
  }

  virtual const CkArrayIndex &__index_max__(void) const override {
    return this->thisIndexMax;
  }

  virtual std::shared_ptr<hypercomm::collective_proxy<CkArrayIndex>>
  __proxy__(void) const override {
    return hypercomm::make_proxy(const_cast<this_ptr>(this)->thisProxy);
  }

  virtual std::shared_ptr<hypercomm::element_proxy<CkArrayIndex>>
  __element__(void) const override {
    return hypercomm::make_proxy(
        (const_cast<this_ptr>(this)->thisProxy)[this->ckGetArrayIndex()]);
  }
};

namespace hypercomm {

template <typename Base, typename Index>
class vil : public Base, public locality_base<Index> {
public:
  // NOTE ( this is a mechanism for remote task invocation )
  virtual void execute(CkMessage *_1) override {
    this->update_context();

    auto msg = utilities::wrap_message(_1);
    auto s = serdes::make_unpacker(msg, utilities::get_message_buffer(msg));

    bool typed;
    s | typed;

    if (typed) {
      typename locality_base<Index>::action_type action{};
      s | action;
      this->receive_action(action);
    } else {
      typename locality_base<Index>::generic_action_type action{};
      s | action;
      this->receive_action(action);
    }
  }

  /* NOTE ( this is a mechanism for demux'ing an incoming message
   *        to the appropriate entry port )
   */
  virtual void demux(hypercomm_msg *_1) override {
    this->update_context();
    auto msg = _1->is_null() ? std::shared_ptr<hyper_value>(
                                   nullptr, [_1](void *) { CkFreeMsg(_1); })
                             : msg2value(_1);
    this->receive_value(_1->dst, std::move(msg));
  }
};

template <typename Index>
/* static */ void
locality_base<Index>::send_action(const collective_ptr<CkArrayIndex> &p,
                                  const CkArrayIndex &i, const action_type &a) {
  const auto &thisCollection =
      static_cast<const CProxy_locality_base_ &>(p->c_proxy());
  auto msg = hypercomm::pack(true, a);
  (const_cast<CProxy_locality_base_ &>(thisCollection))[i].execute(msg);
}

template <typename Index>
/* static */ void
locality_base<Index>::send_action(const collective_ptr<CkArrayIndex> &p,
                                  const CkArrayIndex &i, const generic_action_type &a) {
  const auto &thisCollection =
      static_cast<const CProxy_locality_base_ &>(p->c_proxy());
  auto msg = hypercomm::pack(false, a);
  (const_cast<CProxy_locality_base_ &>(thisCollection))[i].execute(msg);
}

template<typename Index>
void deliver(const element_proxy<Index>& proxy, message* msg) {
  const auto &base =
      static_cast<const CProxyElement_locality_base_ &>(proxy.c_proxy());
  const_cast<CProxyElement_locality_base_ &>(base).demux(msg);
}

message *repack_to_port(const entry_port_ptr &port, component::value_type &&value) {
 auto msg =
      value
          ? static_cast<message *>(value->release())
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

// NOTE this should always be used for invalidations
template <typename Index>
inline void send2port(const element_ptr<Index> &proxy, const entry_port_ptr &port,
                      component::value_type &&value) {
  deliver(*proxy, repack_to_port(port, std::move(value)));
}

inline void send2port(const std::shared_ptr<generic_element_proxy> &proxy, const entry_port_ptr &port,
                      component::value_type &&value) {
  proxy->receive(repack_to_port(port, std::move(value)));
}

inline void send2future(const future& f, component::value_type &&value) {
  auto src = std::dynamic_pointer_cast<generic_element_proxy>(f.source);
  CkAssert(src && "future must be from a locality!");
  auto port = std::make_shared<future_port>(f);
  send2port(src, port, std::move(value));
}

template <typename Proxy, typename Index>
inline void broadcast_to(
    const Proxy& proxy,
    const typename locality_base<Index>::section_ptr& section,
    hypercomm_msg* msg) {
  broadcast_to(make_proxy(proxy), section, msg);
}

template <typename Index>
inline void broadcast_to(
    const proxy_ptr& proxy,
    const typename locality_base<Index>::section_ptr& section,
    hypercomm_msg* msg) {
  auto root = section->index_at(0);
  auto action = std::make_shared<broadcaster<Index>>(section, msg);
  auto collective = std::dynamic_pointer_cast<array_proxy>(proxy);
  using index_type = array_proxy::index_type;
  locality_base<Index>::send_action(collective, conv2idx<index_type>(root),
                                    action);
}

template <typename Index>
void locality_base<Index>::receive_message(hypercomm_msg* msg) {
  auto* env = UsrToEnv(msg);
  auto idx = env->getEpIdx();

  if (idx == CkIndex_locality_base_::demux(nullptr)) {
    this->receive_value(msg->dst, msg2value(msg));
  } else {
    _entryTable[idx]->call(msg, this);
  }
}

template <typename Index>
void locality_base<Index>::broadcast(const section_ptr& section,
                                     hypercomm_msg* msg) {
  auto root = section->index_at(0);
  auto action = std::make_shared<broadcaster<Index>>(section, msg);

  if (root == this->__index__()) {
    this->receive_action(action);
  } else {
    auto collective = std::dynamic_pointer_cast<array_proxy>(this->__proxy__());
    using index_type = array_proxy::index_type;
    send_action(collective, conv2idx<index_type>(root), action);
  }
}

template <typename Index>
void locality_base<Index>::request_future(const future &f,
                                          const callback_ptr &cb) {
  auto ourPort = std::make_shared<future_port>(f);
  auto ourElement = this->__element__();

  this->open(ourPort, cb);

  auto &source = *f.source;
  if (!ourElement->equals(source)) {
    auto theirElement = dynamic_cast<element_proxy<CkArrayIndex> *>(&source);
    auto fwd = std::make_shared<forwarding_callback>(ourElement, ourPort);
    auto opener = std::make_shared<port_opener>(ourPort, fwd);
    send_action(theirElement->collection(), theirElement->index(), opener);
  }
}

void forwarding_callback::send(callback::value_type &&value) {
  send2port(std::dynamic_pointer_cast<element_proxy<CkArrayIndex>>(this->proxy),
            this->port, std::move(value));
}
}

#include "locality.def.h"

#endif
