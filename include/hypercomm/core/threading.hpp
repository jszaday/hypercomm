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
using member_fn = void (T::*)(CkMessage*);
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
}

template <typename T>
inline pair_type create(T* obj, const member_fn<T>& fn, CkMessage* msg);

struct manager {
  using thread_map = std::map<id_t, type>;

 private:
  thread_map threads_;

  struct manager_listener_ : public listener {
    id_t tid;

    manager_listener_(manager* _1, const id_t& _2) : listener(_1), tid(_2) {}

    virtual bool free(void) override {
      ((manager*)this->data)->on_free(this->tid);

      return true;
    }
  };

  friend class manager_listener_;

 public:
  inline void on_free(const id_t& tid) {
    // return the thread's id to the pool
    (&CpvAccess(free_ids_))->push_back(tid);
    // then erase it from our records
    this->threads_.erase(tid);
  }

  inline void put(const pair_type& pair) { this->put(pair.second, pair.first); }

  inline void put(const id_t& tid, const CthThread& th) {
    auto l = new manager_listener_(this, tid);
    this->threads_.emplace(tid, th);
    CthAddListener(th, l);
  }

  template <typename T>
  inline type emplace(T* obj, const member_fn<T>& fn, CkMessage* msg) {
    auto pair = create(obj, fn, msg);
    this->put(pair);
    return pair.first;
  }

  inline type find(const id_t& tid) {
    auto search = this->threads_.find(tid);
    CkAssert(search != std::end(this->threads_) && "could not find thread");
    return search->second;
  }

  inline void pup(PUP::er& p) {
    auto n = this->threads_.size();
    p | n;

    if (p.isUnpacking()) {
      for (auto i = 0; i < n; i += 1) {
        id_t tid;
        p | tid;
        CthThread th;
        th = CthPup((pup_er)&p, th);
        this->threads_.emplace(tid, th);
      }
    } else {
      for (auto& pair : this->threads_) {
        p | const_cast<id_t&>(pair.first);
        pair.second = CthPup((pup_er)&p, pair.second);
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
  T* obj;

  launch_pack(T* _1, const member_fn<T>& _2, CkMessage* _3)
      : obj(_1), fn(_2), msg(_3) {}

  inline void run(void) { ((obj)->*(fn))(msg); }

  static void action(launch_pack<T>* pack) {
    pack->run();
    delete pack;
  }
};

template <typename T>
inline pair_type create(T* obj, const member_fn<T>& fn, CkMessage* msg) {
  auto ctx = create_context();
  return std::make_pair(
      CthCreateMigratable((CthVoidFn)launch_pack<T>::action,
                          new launch_pack<T>(obj, fn, msg), 0, ctx.first),
      ctx.second);
}
}
}

#endif
