#include <hypercomm/utilities/backstage_pass.hpp>
#include <hypercomm/core/generic_locality.hpp>
#include <hypercomm/messaging/interceptor.hpp>
#include <hypercomm/sections/imprintable.hpp>
#include <hypercomm/utilities/macros.hpp>

CkpvExtern(CkCoreState*, _coreState);

namespace hypercomm {

CkpvDeclare(CProxy_interceptor, interceptor_);

namespace detail {
void delete_value_(const void*, CkDataMsg* msg) {
  auto* buf = (CkNcpyBuffer*)(msg->data);
  auto* ptr = (std::shared_ptr<void>*)buf->getRef();
  buf->deregisterMem();
  CkFreeMsg(msg);
  delete ptr;
}

message* pack_deferrable_(const entry_port_ptr& port, value_ptr&& uniq) {
  std::shared_ptr<hyper_value> value(uniq.release());
  std::vector<serdes::deferred_type> deferred;
  // find the size of the value, and extract any
  // buffers to be deferred (and sent via RDMA)
  packer pkr((char*)nullptr);
  auto pupSize = ([&](void) {
    sizer s(true);
    s.reset_source(value);
    value->pup_buffer(s, true);
    s.get_deferred(deferred);
    pkr.acquire(s);  // steals records to avoid recreating ptrs!
    return s.size();
  })();
  auto hasDeferred = !deferred.empty();
  auto msgSize = pupSize;
  if (hasDeferred) {
    // add an offset for the std::vector's size
    msgSize += sizeof(std::size_t);
  } else {
    // otherwise, drop the overhead of encapsulation
    msgSize -= ptr_record::instance_size;
  }
  // reserve memory for a buncha' buffers
  std::vector<CkNcpyBuffer> buffers;
  buffers.reserve(deferred.size());
  // for every deferred block of memory
  for (auto& tup : deferred) {
    // create a (shared) pointer to the memory
    auto* ptr = new std::shared_ptr<void>(std::move(std::get<2>(tup)));
    auto& size = std::get<1>(tup);
    // create a callback to delete the value (on this PE)
    CkCallback cb((CkCallbackFn)&delete_value_, nullptr);
    // set up the CkNcpyBuffer
    buffers.emplace_back(ptr->get(), size, cb, CK_BUFFER_REG);
    auto& buffer = buffers.back();
    buffer.setRef(ptr);
    // and "PUP" it
    msgSize += PUP::size(buffer);
  }
  // set up the message
  auto msg = message::make_message(msgSize, port);
  msg->set_zero_copy(hasDeferred);
  // PUP the buffers first (so the receiver can
  // access them without offsets)
  PUP::toMem p(msg->payload);
  if (hasDeferred) {
    p | buffers;
  }
  // then PUP the value (with appropriate deference)
  pkr.reset(p.get_current_pointer());
  pkr.deferrable = hasDeferred;
  value->pup_buffer(pkr, hasDeferred);
  CkAssertMsg((p.size() + pkr.size()) == msgSize, "size mismatch!");
  return msg;
}

// TODO ( it would be good to rename this at some point )
static message* repack_to_port(const entry_port_ptr& port, value_ptr&& value) {
  if (value) {
    auto flags = value->flags;
    auto* msg = pack_deferrable_(port, std::move(value));
    auto* env = UsrToEnv(msg);
    env->setRef(flags | env->getRef());
    return msg;
  } else {
    auto* msg = message::make_null_message(port);
    CkAssert(port == msg->dst);
    return msg;
  }
}
}  // namespace detail

CkMessage* deliverable::to_message(deliverable&& dev) {
  CkAssert((bool)dev && dev.kind != kDeferred);
  if (dev.kind == deliverable::kMessage) {
    auto* msg = dev.release<CkMessage>();
    dev.endpoint().export_to(msg);
    return msg;
  } else {
    // pack the value with its destination port to form a message
    value_ptr value(dev.release<hyper_value>());
    auto* msg =
        detail::repack_to_port(std::move(dev.ep_.port_), std::move(value));
    UsrToEnv(msg)->setEpIdx(dev.ep_.idx_);
    CkAssertMsg(!dev.ep_.is_demux() || msg->dst, "invalid port copy!");
    return msg;
  }
}

namespace messaging {
void initialize(void) {
  // register the messaging module (on rank 0)
  if (CkMyRank() == 0) {
    _registermessaging();
  }
  // register the handler for delivery
  delivery::handler();
  interceptor::deliver_handler();
  // zero the per-pe interceptor proxy
  CkpvInitialize(CProxy_interceptor, interceptor_);
  CkAssert(((CkGroupID)CkpvAccess(interceptor_)).isZero());
}
}  // namespace messaging

// registers delivery::handler_ as a converse handler
const int& delivery::handler(void) {
  return CmiAutoRegister(delivery::handler_);
}

// registers delivery::handler_ as a converse handler
const int& interceptor::deliver_handler(void) {
  return CmiAutoRegister(interceptor::deliver_handler_);
}

static inline void prep_array_msg_(CkMessage* msg, const CkArrayID& aid) {
  auto* amsg = (CkArrayMessage*)msg;
  auto* env = UsrToEnv(msg);
  env->setMsgtype(ForArrayEltMsg);
  env->setArrayMgr(aid);
  env->getsetArraySrcPe() = CkMyPe();
  env->setRecipientID(ck::ObjID(0));
  env->getsetArrayHops() = 0;
  amsg->array_setIfNotThere(CkArray_IfNotThere_buffer);
}

static inline CkArray* lookup_or_buffer_(const CkArrayID& aid, envelope* env) {
  auto gid = (CkGroupID)aid;
  auto* ck = CkpvAccess(_coreState);
  CmiImmediateLock(CkpvAccess(_groupTableImmLock));
  IrrGroup* obj = ck->localBranch(gid);
  if (obj == nullptr) {
    ck->getGroupTable()->find(gid).enqMsg(env);
  }
  CmiImmediateUnlock(CkpvAccess(_groupTableImmLock));
  return dynamic_cast<CkArray*>(obj);
}

bool interceptor::send_fallback(const CkArrayID& aid, const CkArrayIndex& idx,
                                CkMessage* msg, const int& opts) {
#if CMK_VERBOSE
  CkError("warning> delivery through interceptor failed... falling back!\n");
#endif
  auto* arr = lookup_or_buffer_(aid, UsrToEnv(msg));
  if (arr == nullptr) {
    return false;
  } else {
    auto queuing = (opts & CK_MSG_INLINE) ? CkDeliver_inline : CkDeliver_queue;
    prep_array_msg_(msg, aid);
    ((CkArray*)arr)->deliver(msg, idx, queuing, opts & (~CK_MSG_INLINE));
    return true;
  }
}

void interceptor::deliver_handler_(void* raw) {
  auto* imsg = (interceptor_msg_*)raw;
  auto* env = (envelope*)raw;
  auto* msg = (CkMessage*)EnvToUsr(env);

  auto aid = imsg->aid;
  auto idx = imsg->idx;
  auto hdr = imsg->hdr;

  if (imsg->packed()) {
    auto* prev = msg;
    msg = (CkMessage*)_msgTable[hdr.msgIdx]->unpack(prev);
    CkAssert(msg == prev);
    hdr.packed = false;
  }

  auto* loc = local_branch();
  auto* arr = loc ? nullptr : lookup_or_buffer_(aid, env);

  if (arr || loc) {
    std::fill((char*)env, (char*)env + sizeof(envelope), '\0');
    hdr.export_to(env);
  } else {
    // ensure imsg has most up-to-date status
    imsg->set_packed(hdr.packed);
  }

  if (loc == nullptr) {
    if (arr != nullptr) {
      prep_array_msg_(msg, aid);
      ((CkArray*)arr)->deliver(msg, idx, CkDeliver_queue);
    }
  } else {
    loc->deliver(aid, idx, msg);
  }
}

static bool has_valid_endpoint(envelope* env, CkMessage* msg) {
  auto is_msg = env->getMsgIdx() == message::index();
  auto is_demux =
      env->getEpIdx() == CkIndex_locality_base_::idx_demux_CkMessage();
  return (!(is_msg && is_demux) || ((message*)msg)->dst);
}

void interceptor::send_to_branch(const int& pe, const CkArrayID& aid,
                                 const CkArrayIndex& idx, CkMessage* msg) {
  auto* env = UsrToEnv(msg);

  // this catches invalid deliveries on the sender-side
  // (where it's debuggable!)
  CkAssert(has_valid_endpoint(env, msg));

  if (pe == CkMyPe()) {
    if (env->isPacked()) {
      CkUnpackMessage(&env);
    }

    interceptor::send_async(aid, idx, msg);
  } else {
    auto* imsg = (interceptor_msg_*)env;
    interceptor_msg_::header_ hdr(env);

    std::fill((char*)env, (char*)env + sizeof(envelope), '\0');

    imsg->hdr = hdr;
    imsg->aid = aid;
    imsg->idx = idx;

    auto& packer = _msgTable[hdr.msgIdx]->pack;
    imsg->set_packed(packer && (hdr.packed || (CkNodeOf(pe) != CkMyNode())));
    if (imsg->packed() && !hdr.packed) {
      auto prev = msg;
      msg = (CkMessage*)packer(msg);
      CkAssert(msg == prev);
    }

    CmiSetHandler(imsg, interceptor::deliver_handler());
    CmiSyncSendAndFree(pe, hdr.totalSize, (char*)imsg);
  }
}

// locally delivers the payload to the interceptor with immediacy
void delivery::handler_(delivery* msg) {
  auto* local = interceptor::local_branch();
  local->deliver(msg->aid, msg->idx, std::move(msg->payload), true);
  delete msg;
}

// try to send any messages buffered for a given idx
void interceptor::resync_queue(const CkArrayID& aid, const CkArrayIndex& idx) {
  auto& queue = this->queued_[aid];
  auto search = queue.find(idx);
  if (search != std::end(queue)) {
    // take the buffer from the map then erase it
    // (preventing multiple send attempts of the same msg)
    auto buffer = std::move(search->second);
    queue.erase(search);
    // then try to redeliver each buffered message
    for (auto& msg : buffer) {
      this->deliver(aid, idx, msg);
      QdProcess(1);  // processing (1) to offset QdCreate
    }
  }
}

// recursively unravel forwarding requests until the destination is found
const CkArrayIndex& interceptor::dealias(const CkArrayID& aid,
                                         const CkArrayIndex& idx) const {
  if (this->fwdReqs_.empty()) {
    return idx;
  } else {
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
}

// create a forwarding record for "from" to "to" at the home pe of "from"
void interceptor::forward(const CkArrayID& aid, const CkArrayIndex& from,
                          const CkArrayIndex& to) {
  auto* locMgr = CProxy_ArrayBase(aid).ckLocMgr();
  auto homePe = locMgr->homePe(from);
  if (homePe == CkMyPe()) {
    auto& reqMap = this->fwdReqs_[aid];
    auto search = reqMap.find(from);
    if (search == std::end(reqMap)) {
#if CMK_VERBOSE
      CkPrintf("%d> forwarding from %d to %d.\n", CkMyPe(), *(from.data()),
               *(to.data()));
#endif  // CMK_VERBOSE
      reqMap.emplace(from, to);
      // resync the send queue in case it has messages to "from"
      this->resync_queue(aid, from);
    } else {
      CkAbort("duplicate forwarding request");
    }
  } else {
    thisProxy[homePe].forward(aid, from, to);
  }
}

// delete any forwarding records for the given idx, at its home pe
void interceptor::stop_forwarding(const CkArrayID& aid,
                                  const CkArrayIndex& idx) {
  auto* locMgr = CProxy_ArrayBase(aid).ckLocMgr();
  auto homePe = locMgr->homePe(idx);
  if (homePe == CkMyPe()) {
    auto& reqMap = this->fwdReqs_[aid];
    auto search = reqMap.find(idx);
    if (search != std::end(reqMap)) {
      reqMap.erase(search);
    }
    // TODO ( should this resync the send queue for idx? )
  } else {
    thisProxy[homePe].stop_forwarding(aid, idx);
  }
}

namespace detail {
CkLocCache* CkLocMgr::*get(backstage_pass);

// Explicitly instantiating the class generates the fn declared above.
template class access_bypass<CkLocCache * CkLocMgr::*, &CkLocMgr::cache,
                             backstage_pass>;
}  // namespace detail

inline std::pair<int, int> lookup_or_update_(CkLocMgr* locMgr,
                                             const CkArrayIndex& idx,
                                             const CmiUInt8& id) {
  auto cache = locMgr->*detail::get(detail::backstage_pass());
  auto& entry = cache->getLocationEntry(id);

  auto homePe = -1;
  auto lastPe = entry.pe;
  auto badLookup = lastPe == homePe;
  if ((lastPe == CkMyPe()) || badLookup) {
    homePe = locMgr->homePe(idx);
    if (badLookup) {
      CkLocEntry e;
      e.id = id;
      e.pe = lastPe = homePe;
      e.epoch = entry.epoch + 1;
      cache->updateLocation(e);
    }
  }

  return std::make_pair(lastPe, homePe);
}

inline std::pair<int, int> lookup_fallback_(CkLocMgr* locMgr,
                                            const CkArrayIndex& idx) {
  auto homePe = locMgr->homePe(idx);
  return std::make_pair(homePe, homePe);
}

void interceptor::deliver(const CkArrayID& aid, const CkArrayIndex& pre,
                          deliverable&& payload, const bool& immediate) {
  CmiUInt8 id;
  auto* arr = CProxy_ArrayBase(aid).ckLocalBranch();
  auto* locMgr = arr->getLocMgr();
  auto& post = this->dealias(aid, pre);
  auto validId = locMgr->lookupID(post, id);
  auto* elt = validId ? (ArrayElement*)arr->getEltFromArrMgr(id) : nullptr;
  // if the elt is locally available
  if (elt != nullptr) {
    // process it with appropriate immediacy
    delivery::process(elt, std::move(payload), immediate);
  } else {
    auto mine = CkMyPe();
    auto* msg = deliverable::to_message(std::move(payload));
    auto lastHome = validId ? lookup_or_update_(locMgr, post, id)
                            : lookup_fallback_(locMgr, post);
    // if we are the elt's home (and its last known loc)
    if ((mine == lastHome.first) && (lastHome.first == lastHome.second)) {
      // buffer it and create (1) to delay QD
      // TODO ( buffer the payload here, don't release to message )
      this->queued_[aid][post].push_back(msg);
      QdCreate(1);
    } else {
      // if we lost an elt, the home pe will know its location
      // (or at least buffer it)
      send_to_branch(lastHome.first, aid, post, msg);
    }
  }
}

void delivery::process(ArrayElement* elt, deliverable&& dev, bool immediate) {
  auto* cast = static_cast<generic_locality_*>(elt);
  if (immediate) {
    cast->update_context();
    cast->receive(std::move(dev));
  } else {
    auto& aid = elt->ckGetArrayID();
    auto& idx = elt->ckGetArrayIndex();
#if CMK_VERBOSE
    CkPrintf("pe%d> pushing a message/value onto the queue for %s.\n", CkMyPe(),
             utilities::idx2str(elt->ckGetArrayIndex()).c_str());
#endif
    CmiPushPE(CkMyRank(), new delivery(aid, idx, std::move(dev)));
  }
}

void interceptor::send_to_root(
    const CkArrayID& aid, const std::shared_ptr<imprintable_base_>& section,
    CkMessage* msg) {
  auto* root = section->pick_root(aid);
  if (root) {
    interceptor::send_async(aid, *root, msg);
  } else {
    NOT_IMPLEMENTED;
  }
}
}  // namespace hypercomm

#include <hypercomm/messaging/messaging.def.h>
