#ifndef __HYPERCOMM_MESSAGING_INTERCEPTOR_MSG_HPP__
#define __HYPERCOMM_MESSAGING_INTERCEPTOR_MSG_HPP__

#include "intercept_msg.hpp"

namespace hypercomm {

class interceptor : public CBase_interceptor {
  using queue_type =
      std::unordered_map<CkArrayIndex, std::vector<CkMessage*>, IndexHasher>;
  using req_map_type =
      std::unordered_map<CkArrayIndex, CkArrayIndex, IndexHasher>;

  std::unordered_map<CkArrayID, req_map_type, ArrayIDHasher> fwdReqs_;
  std::unordered_map<CkArrayID, queue_type, ArrayIDHasher> queued_;

#if CMK_SMP
  CmiNodeLock lock_;
  std::vector<int> lockCounts_;
#endif

 public:
  interceptor(void)
#if CMK_SMP
      : lock_(CmiCreateLock()),
        lockCounts_(CkMyNodeSize())
#endif
  {
  }

 private:
  ~interceptor() {
#if CMK_SMP
    CmiDestroyLock(this->lock_);
#endif
  }

  inline void lock(void) {
#if CMK_SMP
    auto& count = this->lockCounts_[CkMyRank()];
    if ((count++) == 0) {
      CmiLock(this->lock_);
    }
#endif
  }

  inline void unlock(void) {
#if CMK_SMP
    auto& count = this->lockCounts_[CkMyRank()];
    if ((--count) == 0) {
      CmiUnlock(this->lock_);
    }
#endif
  }

  void resync_queue(const CkArrayID& aid, const CkArrayIndex& idx) {
    auto& queue = this->queued_[aid];
    auto search = queue.find(idx);
    if (search != std::end(queue)) {
      auto before = search->second.size();
      auto buffer = std::move(search->second);
      queue.erase(search);
      auto after = buffer.size();
      CkAssert(before == after);

      for (auto& msg : buffer) {
        this->deliver(aid, idx, msg);
        QdProcess(1);
      }
    }
  }

  inline std::pair<int, int> lastKnown(const CkArrayID& aid,
                                       const CkArrayIndex& idx) {
    auto locMgr = CProxy_ArrayBase(aid).ckLocMgr();
    auto homePe = locMgr->homePe(idx);
    if (CkNodeOf(homePe) == CkMyNode()) {
      locMgr = CProxy_ArrayBase(aid)
                   .ckLocalBranchOther(CkRankOf(homePe))
                   ->getLocMgr();
    }
    return std::make_pair(homePe, locMgr->whichPe(idx));
  }

  const CkArrayIndex& dealias(const CkArrayID& aid,
                              const CkArrayIndex& idx) const {
    auto findAid = this->fwdReqs_.find(aid);
    if (findAid != std::end(this->fwdReqs_)) {
      auto& reqMap = findAid->second;
      auto findIdx = reqMap.find(idx);
      if (findIdx != std::end(reqMap)) {
        return this->dealias(aid, findIdx->second);
      }
    }
    return idx;
  }

 public:
  void forward(const CkArrayID& aid, const CkArrayIndex& from,
               const CkArrayIndex& to) {
    this->lock();
    auto* locMgr = CProxy_ArrayBase(aid).ckLocMgr();
    auto homeNode = CkNodeOf(locMgr->homePe(from));
    if (homeNode == CkMyNode()) {
      auto& reqMap = this->fwdReqs_[aid];
      auto search = reqMap.find(from);
      if (search == std::end(reqMap)) {
#if CMK_VERBOSE
        CkPrintf("%d> forwarding from %d to %d.\n", CkMyNode(), *(from.data()),
                 *(to.data()));
#endif  // CMK_VERBOSE
        reqMap.emplace(from, to);
        this->resync_queue(aid, from);
      } else {
        CkAbort("duplicate forwarding request");
      }
    } else {
      thisProxy[homeNode].forward(aid, from, to);
    }
    this->unlock();
  }

  inline void redeliver(intercept_msg* msg) {
    this->deliver(msg->aid, msg->idx, msg->msg);
  }

  inline void deliver(const CkArrayID& aid, const CkArrayIndex& raw,
                      CkMarshalledMessage&& msg) {
    this->deliver(aid, raw, msg.getMessage());
  }

  void deliver(const CkArrayID& aid, const CkArrayIndex& raw, CkMessage* msg) {
    auto* env = UsrToEnv(msg);
    env->setMsgtype(ForArrayEltMsg);
    auto& numHops = env->getsetArrayHops();

    this->lock();

    auto& idx = this->dealias(aid, raw);
    auto pair = this->lastKnown(aid, idx);
    auto& homePe = pair.first;
    auto& lastPe = (pair.second >= 0) ? pair.second : homePe;
    auto homeNode = CkNodeOf(homePe);
    auto lastNode = CkNodeOf(lastPe);

    if (lastNode == CkMyNode()) {
      auto* arr = CProxy_ArrayBase(aid).ckLocalBranchOther(CkRankOf(lastPe));
      auto* elt = arr->lookup(idx);

      if (elt != nullptr) {
        if (lastPe == CkMyPe()) {
          elt->ckInvokeEntry(UsrToEnv(msg)->getEpIdx(), msg, true);
        } else {
          CmiPushPE(CkRankOf(lastPe), new intercept_msg(aid, idx, msg));
        }

        this->unlock();
        return;
      } else if (homeNode == CkMyNode()) {
        this->queued_[aid][idx].push_back(msg);

#if CMK_ERROR_CHECKING
        auto& reqMap = this->fwdReqs_[aid];
        if (reqMap.find(idx) != std::end(reqMap)) {
          CkAbort("%d> refusing to create (1) for %d.\n", CkMyNode(),
                  *(idx.data()));
        }
#endif  // CMK_ERROR_CHECKING

        QdCreate(1);

        this->unlock();
        return;
      }
    }

    this->unlock();

    int destNode;
    if (numHops > 1 && (homeNode != CkMyNode())) {
      numHops = 0;
      destNode = homeNode;
    } else {
      numHops += 1;
      destNode = lastNode;
    }

    thisProxy[destNode].deliver(aid, idx, CkMarshalledMessage(msg));
  }

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
      interceptor_[CkMyNode()].deliver(aid, idx, CkMarshalledMessage(msg));
    }
  }

  inline static interceptor* local_branch() {
    if (((CkGroupID)interceptor_).isZero()) {
      return nullptr;
    } else {
      return interceptor_.ckLocalBranch();
    }
  }
};

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
}

#endif
