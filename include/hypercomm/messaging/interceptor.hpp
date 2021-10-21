#ifndef __HYPERCOMM_MESSAGING_INTERCEPTOR_MSG_HPP__
#define __HYPERCOMM_MESSAGING_INTERCEPTOR_MSG_HPP__

#include "messaging.hpp"
#include "delivery.hpp"

namespace hypercomm {

CkpvExtern(CProxy_interceptor, interceptor_);

struct interceptor_msg_ {
  char core[CmiMsgHeaderSizeBytes];

  struct header_ {
    std::size_t totalSize;
    CMK_REFNUM_TYPE refNum;
    UShort epIdx;
    UChar msgIdx;
    bool packed;

    header_(const envelope* env) {
      this->refNum = env->getRef();
      this->epIdx = env->getEpIdx();
      this->msgIdx = env->getMsgIdx();
      this->packed = env->isPacked();
      this->totalSize = env->getTotalsize();
    }

    inline void export_to(envelope* env) const {
      env->setRef(this->refNum);
      env->setEpIdx(this->epIdx);
      env->setMsgIdx(this->msgIdx);
      env->setPacked(this->packed);
      env->setTotalsize(this->totalSize);
    }
  };

  header_ hdr;
  CkArrayID aid;
  CkArrayIndex idx;

  inline void set_packed(const bool& _) { this->hdr.packed = _; }

  inline const bool& packed(void) const { return this->hdr.packed; }
};

static_assert(sizeof(interceptor_msg_) <= sizeof(envelope),
              "interceptor header cannot fit within message!?");

class interceptor : public CBase_interceptor {
  using queue_type =
      std::unordered_map<CkArrayIndex, std::vector<CkMessage*>, IndexHasher>;
  using req_map_type =
      std::unordered_map<CkArrayIndex, CkArrayIndex, IndexHasher>;

  std::unordered_map<CkArrayID, req_map_type, ArrayIDHasher> fwdReqs_;
  std::unordered_map<CkArrayID, queue_type, ArrayIDHasher> queued_;

  static void deliver_handler_(void*);

 public:
  interceptor(void) { CkpvAccess(interceptor_) = this->thisProxy; }

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

  // create a forwarding record for "from" to "to" at the home pe of "from"
  void forward(const CkArrayID& aid, const CkArrayIndex& from,
               const CkArrayIndex& to);

  // delete any forwarding records for the given idx, at its home pe
  void stop_forwarding(const CkArrayID& aid, const CkArrayIndex& idx);

  static const int& deliver_handler(void);

  inline void deliver(const CkArrayID& aid, const CkArrayIndex& raw,
                      CkMessage* msg) {
    // deliver with immediately payload processing, "inlining" the EP
    this->deliver(aid, raw, deliverable(msg), true);
  }

  void deliver(const CkArrayID& aid, const CkArrayIndex& raw, deliverable&&,
               const bool& immediate);

  static void send_to_branch(const int& pe, const CkArrayID& aid,
                             const CkArrayIndex& idx, CkMessage* msg);

  template <typename T, typename Value>
  inline static is_valid_endpoint_t<T> send_async(const CkArrayID& aid,
                                                  const CkArrayIndex& idx,
                                                  const T& ep, Value&& value) {
    deliverable dev(std::forward<Value>(value));
    dev.update_endpoint(ep);
    interceptor::send_async(aid, idx, std::move(dev));
  }

  template <typename T, typename Value>  // ^
  inline static is_valid_endpoint_t<T> send_async(
      const CProxyElement_ArrayElement& proxy, const T& ep, Value&& value) {
    interceptor::send_async(proxy.ckGetArrayID(), proxy.ckGetIndex(), ep,
                            std::move(value));
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

  inline static void send_async(const CkArrayID& aid, const CkArrayIndex& idx,
                                CkMessage* msg) {
    interceptor::send_async(aid, idx, deliverable(msg));
  }

  inline static void send_async(const CProxyElement_ArrayElement& proxy,
                                deliverable&& dev) {
    interceptor::send_async(proxy.ckGetArrayID(), proxy.ckGetIndex(),
                            std::move(dev));
  }

  // asynchronously send a message to the specified index of aid
  inline static void send_async(const CkArrayID& aid, const CkArrayIndex& idx,
                                deliverable&& payload) {
    if (((CkGroupID)CkpvAccess(interceptor_)).isZero()) {
      CkEnforce(
          send_fallback(aid, idx, deliverable::to_message(std::move(payload))));
    } else {
      auto* loc = spin_to_win();
      CkAssertMsg(loc, "unable to retrieve interceptor");
      loc->deliver(aid, idx, std::move(payload), false);
    }
  }

  static void send_to_root(const CkArrayID&,
                           const std::shared_ptr<imprintable_base_>&,
                           CkMessage*);

  // get the local branch of interceptor_
  inline static interceptor* local_branch() {
    if (((CkGroupID)CkpvAccess(interceptor_)).isZero()) {
      return nullptr;
    } else {
      return spin_to_win();
    }
  }

 protected:
  static bool send_fallback(const CkArrayID& aid, const CkArrayIndex& idx,
                            CkMessage* msg, const int& opts = 0);

  inline static interceptor* spin_to_win(void) {
    void* local = nullptr;
    while ((local = CkLocalBranch(CkpvAccess(interceptor_))) == nullptr) {
      if (CthIsMainThread(CthSelf())) {
        CsdScheduler(0);
      } else {
        CthYield();
      }
    }
    return (interceptor*)local;
  }
};

// a mainchare that initializes the singleton instance of interceptor
class interceptor_initializer_ : public CBase_interceptor_initializer_ {
 public:
  interceptor_initializer_(CkArgMsg* m) {
    CProxy_interceptor::ckNew();

    delete this;
  }

  interceptor_initializer_(CkMigrateMessage* m)
      : CBase_interceptor_initializer_(m) {
    delete m;
  }
};
}  // namespace hypercomm

#endif
