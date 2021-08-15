#ifndef __HYPERCOMM_MESSAGING_INTERCEPTOR_MSG_HPP__
#define __HYPERCOMM_MESSAGING_INTERCEPTOR_MSG_HPP__

#include "messaging.hpp"

#include "../core/entry_port.hpp"
#include "../core/value.hpp"

namespace hypercomm {

extern CProxy_interceptor interceptor_;

class interceptor : public CBase_interceptor {
  using queue_type =
      std::unordered_map<CkArrayIndex, std::vector<CkMessage*>, IndexHasher>;
  using req_map_type =
      std::unordered_map<CkArrayIndex, CkArrayIndex, IndexHasher>;

  std::unordered_map<CkArrayID, req_map_type, ArrayIDHasher> fwdReqs_;
  std::unordered_map<CkArrayID, queue_type, ArrayIDHasher> queued_;

 public:
  interceptor(void) = default;

  // try to send any messages buffered for a given idx
  void resync_queue(const CkArrayID& aid, const CkArrayIndex& idx);

  // look up the home PE and last known PE of the given index
  inline std::pair<int, int> lastKnown(const CkArrayID& aid,
                                       const CkArrayIndex& idx) {
    auto locMgr = CProxy_ArrayBase(aid).ckLocMgr();
    return std::make_pair(locMgr->homePe(idx), locMgr->whichPe(idx));
  }

  // recursively unravel forwarding requests until the destination is found
  const CkArrayIndex& dealias(const CkArrayID& aid,
                              const CkArrayIndex& idx) const;

 public:
  // create a forwarding record for "from" to "to" at the home pe of "from"
  void forward(const CkArrayID& aid, const CkArrayIndex& from,
               const CkArrayIndex& to);

  // delete any forwarding records for the given idx, at its home pe
  void stop_forwarding(const CkArrayID& aid, const CkArrayIndex& idx);

  inline void deliver(const CkArrayID& aid, const CkArrayIndex& raw,
                      CkMarshalledMessage&& msg) {
    this->deliver(aid, raw, msg.getMessage());
  }

  void deliver(const CkArrayID& aid, const CkArrayIndex& raw, CkMessage* msg);

  inline static void send_async(const CkArrayID& aid, const CkArrayIndex& idx, const entry_port_ptr& port, value_ptr&& value) {
    interceptor::send_async(aid, idx, repack_to_port(port, std::move(value)));
  }

  inline static void send_async(const CProxyElement_ArrayElement& proxy, const entry_port_ptr& port, value_ptr&& value) {
    interceptor::send_async(proxy.ckGetArrayID(), proxy.ckGetIndex(), port, std::move(value));
  }

  // asynchronously send a message to the specified element
  inline static void send_async(
      const std::shared_ptr<generic_element_proxy>& proxy, CkMessage* msg) {
    // TODO this could be made more generic
    interceptor::send_async((const CProxyElement_ArrayElement&)proxy->c_proxy(),
                            msg);
  }

  // asynchronously send a message to the specified element
  inline static void send_async(const CProxyElement_ArrayElement& proxy,
                                CkMessage* msg) {
    interceptor::send_async(proxy.ckGetArrayID(), proxy.ckGetIndex(), msg);
  }

  // asynchronously send a message to the specified index of aid
  inline static void send_async(const CkArrayID& aid, const CkArrayIndex& idx,
                                CkMessage* msg) {
    if (((CkGroupID)interceptor_).isZero()) {
#if CMK_VERBOSE
      CkError("warning> unable to deliver through interceptor.\n");
#endif
      // TODO ( is there a better function for this? )
      CProxyElement_ArrayBase::ckSendWrapper(aid, idx, msg,
                                             UsrToEnv(msg)->getEpIdx(), 0);
    } else {
      interceptor_[CkMyPe()].deliver(aid, idx, CkMarshalledMessage(msg));
    }
  }

  static void send_to_root(const CkArrayID&,
                           const std::shared_ptr<imprintable_base_>&,
                           CkMessage*);

  // get the local branch of interceptor_
  inline static interceptor* local_branch() {
    if (((CkGroupID)interceptor_).isZero()) {
      return nullptr;
    } else {
      return interceptor_.ckLocalBranch();
    }
  }
};

// a mainchare that initializes the singleton instance of interceptor
class interceptor_initializer_ : public CBase_interceptor_initializer_ {
 public:
  interceptor_initializer_(CkArgMsg* m) {
    interceptor_ = CProxy_interceptor::ckNew();
    delete this;
  }

  interceptor_initializer_(CkMigrateMessage* m)
      : CBase_interceptor_initializer_(m) {
    delete m;
  }
};
}  // namespace hypercomm

#endif
