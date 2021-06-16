#ifndef __HYPERCOMM_CORE_THREADING_HPP__
#define __HYPERCOMM_CORE_THREADING_HPP__

#ifndef CMK_MAX_THREADS
#define CMK_MAX_THREADS 16
#endif

#include <charm++.h>
#include <memory-isomalloc.h>

namespace hypercomm {
namespace thread {

using type = CthThread;
using id_t = int;
using pair_type = std::pair<type, id_t>;
using base_listener = CthThreadListener;

namespace {
template <typename T>
using member_fn = void (*)(T*&, CkMessage*);
}

class listener : public base_listener {
 public:
  listener(void) {
    base_listener::free = listener_free_;
    base_listener::resume = listener_resume_;
    base_listener::suspend = listener_suspend_;
  }

  listener(void* data) : listener() { this->data = data; }

  virtual bool free(void) { return true; }
  virtual void resume(void) {}
  virtual void suspend(void) {}

 private:
  static void listener_free_(CthThreadListener* _1) {
    auto l = (listener*)_1;
    if (l->free()) delete l;
  }

  static void listener_resume_(CthThreadListener* l) {
    ((listener*)l)->resume();
  }

  static void listener_suspend_(CthThreadListener* l) {
    ((listener*)l)->suspend();
  }
};

namespace {
CpvDeclare(int, n_contexts_);

using free_pool_type = std::vector<int>;
CpvDeclare(free_pool_type, free_ids_);

CtvDeclare(void*, self_);
}

inline std::pair<CmiIsomallocContext, int> create_context(void);

template <typename T>
inline type create(CmiIsomallocContext ctx, T* &obj, const member_fn<T>& fn, CkMessage* msg);

struct manager {
  using thread_map = std::map<id_t, std::pair<type, std::intptr_t>>;

 private:
  thread_map threads_;
  void* owner_;

  struct manager_listener_ : public listener {
    id_t tid;

    manager_listener_(manager* _1, const id_t& _2) : listener(_1), tid(_2) {}

    virtual bool free(void) override {
      // this cannot be called until the final free!
      // ((manager*)this->data)->on_free(this->tid);

      return true;
    }
  };

  friend class manager_listener_;

 public:

  manager(void* owner): owner_(owner) {}

  inline void on_free(const id_t& tid) {
    // return the thread's id to the pool
    (&CpvAccess(free_ids_))->push_back(tid);
    // then erase it from our records
    this->threads_.erase(tid);
  }

  template <typename T>
  inline pair_type emplace(const member_fn<T>& fn, CkMessage* msg) {
    auto ctx = create_context();
    auto res = this->threads_.emplace(ctx.second, std::make_pair(nullptr, (std::intptr_t)this->owner_));
    auto th = create(ctx.first, *((T**)(&res.first->second.second)), fn, msg);
    res.first->second.first = th;
    auto l = new manager_listener_(this, ctx.second);
    CthAddListener(th, l);
    return std::make_pair(th, ctx.second);
  }

  inline type find(const id_t& tid) {
    auto search = this->threads_.find(tid);
    CkAssert(search != std::end(this->threads_) && "could not find thread");
    return search->second.first;
  }

  inline void set_owner(void* owner) { this->owner_ = owner; }

  inline void pup(PUP::er& p) {
    auto n = this->threads_.size();
    p | n;

    if (p.isUnpacking()) {
      for (auto i = 0; i < n; i += 1) {
        id_t tid;
        p | tid;
        std::intptr_t self_;
        p | self_;
        CthThread th;
        th = CthPup((pup_er)&p, th);
        this->threads_.emplace(tid, std::make_pair(th, self_));
        (void*&)self_ = this->owner_;
      }
    } else {
      for (auto& pair : this->threads_) {
        p | const_cast<id_t&>(pair.first);
        p | pair.second.second;
        pair.second.first = CthPup((pup_er)&p, pair.second.first);
      }
    }
  }
};

// initializes the thread module's isomalloc context pool
inline void setup_isomalloc(void) {
  CpvInitialize(int, n_contexts_);
  CpvAccess(n_contexts_) = CMK_MAX_THREADS * CkNumPes();

  CpvInitialize(free_pool_type, free_ids_);
  auto& pool = CpvAccess(free_ids_);
  new (&pool) free_pool_type(CMK_MAX_THREADS);

  int m_begin = CMK_MAX_THREADS * CkMyPe();
  std::iota(std::begin(pool), std::end(pool), m_begin);
}

inline std::pair<CmiIsomallocContext, int> create_context(void) {
  auto& pool = CpvAccess(free_ids_);
#if CMK_ERROR_CHECKING
  if (pool.empty()) {
    CkAbort(
        "unable to allocate a thread id, increase CMK_MAX_THREADS beyond %d",
        CMK_MAX_THREADS);
  }
#endif
  // pop a thread id from the pool
  auto id = pool.back();
  pool.pop_back();
  // create and configure a context using it
  auto ctx = CmiIsomallocContextCreate(id, CpvAccess(n_contexts_));
  CmiIsomallocContextEnableRandomAccess(ctx);  // required for heap interception
  // then return it
  return std::make_pair(ctx, id);
}

template <typename T>
struct launch_pack {
  member_fn<T> fn;
  CkMessage* msg;
  T* &obj;

  launch_pack(T* &_1, const member_fn<T>& _2, CkMessage* _3)
      : obj(_1), fn(_2), msg(_3) {}

  static void action(launch_pack<T>* pack) {
    auto fn = pack->fn;
    auto msg = pack->msg;
    T* self = pack->obj;
    pack->obj = self;
    delete pack;

    (*fn)(self, msg);
  }
};

template <typename T>
inline type create(CmiIsomallocContext ctx, T* &obj, const member_fn<T>& fn, CkMessage* msg) {
  return CthCreateMigratable(
            (CthVoidFn)launch_pack<T>::action,
            new launch_pack<T>(obj, fn, msg), 0, ctx);
}
}
}

#endif
