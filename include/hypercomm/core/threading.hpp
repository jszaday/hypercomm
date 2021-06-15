#ifndef __HYPERCOMM_CORE_THREADING_HPP__
#define __HYPERCOMM_CORE_THREADING_HPP__

#include "../utilities/hash.hpp"
#include <memory-isomalloc.h>

PUPbytes(CmiObjId);

namespace hypercomm {
namespace thread {

using type = CthThread;
using id_t = CmiObjId;
using base_listener = CthThreadListener;
}

namespace utilities {
template <>
struct hash<thread::id_t> {
  std::size_t operator()(const thread::id_t& tid) const {
    return hash_iterable(tid.id);
  }
};
}

namespace thread {
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

struct manager_ {
  using thread_map = std::unordered_map<id_t, CthThread, utilities::hash<id_t>>;

 private:
  thread_map threads_;

  struct manager_listener_ : public listener {
    CmiObjId tid;

    manager_listener_(manager_* _1, CmiObjId* _2) : listener(_1), tid(*_2) {}

    virtual bool free(void) override {
      auto man = (manager_*)this->data;
      man->threads_.erase(tid);
      return true;
    }
  };

  friend class manager_listener_;

 public:
  inline void put(type th) {
    auto l = new manager_listener_(this, CthGetThreadID(th));
    this->threads_.emplace(l->tid, th);
    CthAddListener(th, l);
  }

  inline type find(const CmiObjId& tid) {
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

template <typename T>
using member_fn = void (T::*)(CkMessage*);

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
inline type create(T* obj, const member_fn<T>& fn, CkMessage* msg) {
  return CthCreate((CthVoidFn)launch_pack<T>::action,
                   new launch_pack<T>(obj, fn, msg), 0);
}
}
}

#endif
