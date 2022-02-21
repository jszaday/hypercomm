#ifndef HYPERCOMM_TASKING_WORKGROUP_HPP
#define HYPERCOMM_TASKING_WORKGROUP_HPP

#include "flex_pup.hpp"
#include "../utilities/backstage_pass.hpp"
#include <hypercomm/tasking/tasking.decl.h>

namespace hypercomm {
namespace detail {
std::vector<CkMigratable *> CkArray::*get(backstage_pass);

// Explicitly instantiating the class generates the fn declared above.
template class access_bypass<std::vector<CkMigratable *> CkArray::*,
                             &CkArray::localElemVec, backstage_pass>;
}  // namespace detail

namespace tasking {
using workgroup_proxy = CProxy_workgroup;

struct task_message : public CMessage_task_message {
  task_id tid;
  task_kind_t kind;
  char *data;

  static void *alloc(int msgnum, size_t sz, int *sizes, int pb,
                     GroupDepNum groupDepNum) {
    auto start = ALIGN_DEFAULT(sz);
    auto total_size = start + sizes[0];
    auto *msg = CkAllocMsg(msgnum, total_size, pb, groupDepNum);
    static_cast<task_message *>(msg)->data = (char *)msg + start;
    return msg;
  }

  static void *pack(void *raw) {
    auto *msg = static_cast<task_message *>(raw);
    msg->data = msg->data - (std::uintptr_t)msg;
    return raw;
  }

  static void *unpack(void *raw) {
    auto *msg = static_cast<task_message *>(raw);
    msg->data = (char *)msg + (std::uintptr_t)msg->data;
    return raw;
  }
};

struct task_payload {
  std::unique_ptr<CkMessage> src;
  std::size_t len;
  char *data;

  task_payload(task_message *msg)
      : src(msg),
        len(UsrToEnv(msg)->getTotalsize() - sizeof(task_message)),
        data(msg->data) {}

  task_payload(CkReductionMsg *msg, CkReduction::tupleElement *elt)
      : src(msg), len(elt->dataSize), data((char *)elt->data) {}

  task_payload(PUP::reconstruct) {}

  continuation_id_t id(void) const {
    return (continuation_id_t)CkGetRefNum(src.get());
  }

  void pup(PUP::er &p) {
    if (p.isUnpacking()) {
      void *msg;
      CkPupMessage(p, &msg);
      this->src.reset((CkMessage *)msg);
      p | this->len;
      std::uintptr_t offset;
      p | offset;
      this->data = (char *)msg + offset;
    } else {
      void *msg = this->src.get();
      CkPupMessage(p, &msg);
      CkAssert(msg == this->src.get());
      p | this->len;
      auto offset = (std::uintptr_t)this->data - (std::uintptr_t)msg;
      p | offset;
    }
  }
};

template <typename T>
struct task : public task_base_ {
  static task_kind_t kind_;

  using continuation_fn_t = void (T::*)(task_payload &&);

  task(void) : task_base_(kind_) {}

  template <continuation_fn_t Fn>
  void suspend(void) {
    this->continuation_ = id_for<Fn>();
  }

  inline void terminate(void) { this->active_ = false; }

  template <continuation_fn_t Fn, typename Data>
  void all_reduce(Data &data, CkReduction::reducerType type);

  template <continuation_fn_t Fn, typename Data>
  void reduce(Data &data, CkReduction::reducerType type, int index);

  template <typename Data>
  void reduce(Data &data, CkReduction::reducerType type, const CkCallback &cb);

  template <continuation_fn_t Fn, typename... Args>
  void send(int index, Args &&...args);

  template <continuation_fn_t Fn, typename... Args>
  void broadcast(Args &&...args);

  inline int index(void) const {
    return static_cast<ArrayElement1D *>(CkActiveObj())->thisIndex;
  }

  template <continuation_fn_t Fn>
  static constexpr continuation_id_t id_for(void);
};

template <typename T>
struct task_creation_helper_ {
  static task_base_ *create(task_payload &&payload) {
    return new T(std::move(payload));
  }
};

template <typename T>
struct task_puping_helper_ {
  static void pup(PUP::er &p, task_base_ *&task) {
    if (p.isUnpacking()) {
      task = new T(PUP::reconstruct{});
    }

    task->pup_base_(p);

    ((T *)task)->pup(p);
  }
};

template <typename T>
task_kind_t register_task_(void) {
  auto &records = get_task_records_();
  auto kind = records.size();
  records.emplace_back(&task_creation_helper_<T>::create,
                       &task_puping_helper_<T>::pup);
  return (task_kind_t)kind;
}

template <continuation_t Fn>
continuation_id_t register_continuation_(void) {
  auto &continuations = get_continuations_();
  auto id = continuations.size();
  continuations.emplace_back(Fn);
  return (continuation_id_t)id;
}

template <typename T>
task_kind_t task<T>::kind_ = register_task_<T>();

struct workgroup : public CBase_workgroup {
  template <typename T>
  friend struct task;

  workgroup(void) : last_task_(0) {}
  workgroup(CkMigrateMessage *) {}

  void create(task_message *);
  void resume(task_message *);
  void resume(CkReductionMsg *);
  bool resume(task_id, task_payload &, bool *active = nullptr);
  void pup(PUP::er &p);

  task_id generate_id(void) {
    return task_id{.host_ = thisIndex, .task_ = this->last_task_++};
  }

 private:
  template <typename T>
  using task_map = std::unordered_map<task_id, T, task_id_hasher_>;
  using continuation_queue =
      std::unordered_map<continuation_id_t, std::deque<task_payload>>;

  using task_iterator =
      typename task_map<std::unique_ptr<task_base_>>::iterator;

  task_id active_;
  std::uint32_t last_task_;
  task_map<std::unique_ptr<task_base_>> tasks_;
  task_map<continuation_queue> buffers_;

  void buffer_(task_id tid, task_payload &&payload);
  void try_continue_(const task_iterator &search);
};

template <typename... Args>
task_message *pack(Args &&...args) {
  auto tuple = std::forward_as_tuple(args...);

  auto size = flex::pup_size(tuple);
  auto *msg = new (&size) task_message;
  flex::pup_pack(tuple, msg->data, size);

  return msg;
}

template <typename T, typename... Args>
task_id launch(const workgroup_proxy &group, Args &&...args) {
  auto *local = group.ckLocalBranch();
  auto *elems =
      local ? &(local->*detail::get(detail::backstage_pass())) : nullptr;
  CkAssert(elems && !elems->empty());

  auto *msg = pack(std::forward<Args>(args)...);
  auto tid = ((workgroup *)elems->front())->generate_id();

  msg->tid = tid;
  msg->kind = task<T>::kind_;

  const_cast<workgroup_proxy &>(group).create(msg);

  return tid;
}

template <typename T, typename task<T>::continuation_fn_t Fn>
struct continuation_helper_ {
  static continuation_id_t id_;

  static void resume(task_base_ *task, task_payload &&payload) {
    (((T *)task)->*Fn)(std::move(payload));
  }
};

template <typename T, typename task<T>::continuation_fn_t Fn>
continuation_id_t continuation_helper_<T, Fn>::id_ =
    register_continuation_<&continuation_helper_<T, Fn>::resume>();

template <typename T>
template <typename task<T>::continuation_fn_t Fn, typename Data>
void task<T>::all_reduce(Data &data, CkReduction::reducerType type) {
  auto *host = static_cast<ArrayElement *>(CkActiveObj());
  CkCallback cb(CkIndex_workgroup::resume((CkReductionMsg *)nullptr),
                host->ckGetArrayID());
  cb.setRefNum(id_for<Fn>());
  this->reduce(data, type, cb);
}

template <typename T>
template <typename task<T>::continuation_fn_t Fn, typename Data>
void task<T>::reduce(Data &data, CkReduction::reducerType type, int index) {
  auto *host = static_cast<ArrayElement *>(CkActiveObj());
  CkCallback cb(CkIndex_workgroup::resume((CkReductionMsg *)nullptr),
                CkArrayIndex1D(index), host->ckGetArrayID());
  cb.setRefNum(id_for<Fn>());
  this->reduce(data, type, cb);
}

template <typename T>
template <typename Data>
void task<T>::reduce(Data &data, CkReduction::reducerType type,
                     const CkCallback &cb) {
  auto size = flex::pup_size(data);
  char *buf = (char *)alloca(size);
  flex::pup_pack(data, buf, size);

  auto *host = (workgroup *)CkActiveObj();
  auto &tid = host->active_;

  CkReduction::tupleElement redn[] = {
      CkReduction::tupleElement(sizeof(task_id), &tid,
                                CkReduction::bitvec_and_int),
      CkReduction::tupleElement(size, buf, type)};

  auto *msg = CkReductionMsg::buildFromTuple(redn, 2);
  msg->setCallback(cb);

  host->contribute(msg);
}

template <typename T>
template <typename task<T>::continuation_fn_t Fn, typename... Args>
void task<T>::send(int index, Args &&...args) {
  auto *host = (workgroup *)CkActiveObj();
  auto *msg = pack(std::forward<Args>(args)...);
  msg->tid = host->active_;
  CkSetRefNum(msg, id_for<Fn>());
  host->thisProxy[index].resume(msg);
}

template <typename T>
template <typename task<T>::continuation_fn_t Fn, typename... Args>
void task<T>::broadcast(Args &&...args) {
  auto *host = (workgroup *)CkActiveObj();
  auto *msg = pack(std::forward<Args>(args)...);
  msg->tid = host->active_;
  CkSetRefNum(msg, id_for<Fn>());
  host->thisProxy.resume(msg);
}

template <typename T>
template <typename task<T>::continuation_fn_t Fn>
constexpr continuation_id_t task<T>::id_for(void) {
  return continuation_helper_<T, Fn>::id_;
}
}  // namespace tasking
}  // namespace hypercomm

namespace PUP {
template <>
struct ptr_helper<hypercomm::tasking::task_base_, false> {
  inline void operator()(PUP::er &p, hypercomm::tasking::task_base_ *&t) const {
    using namespace hypercomm::tasking;
    auto kind = p.isUnpacking() ? 0 : t->kind_;
    p | kind;
    auto puper = get_task_puper_(kind);
    puper(p, t);
  }
};
};  // namespace PUP

#endif
