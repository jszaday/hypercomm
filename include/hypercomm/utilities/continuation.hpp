#ifndef __HYPERCOMM_UTLITIES_CONTINUATION_HPP__
#define __HYPERCOMM_UTLITIES_CONTINUATION_HPP__

namespace hypercomm {
namespace detail {
template <typename Ret, typename... Args>
using fn_pointer_t = Ret (*)(Args...);

template <typename T>
struct continuation;

template <typename Ret, typename... Args>
struct continuation<fn_pointer_t<Ret, void*, Args...>> {
  using fn_t = fn_pointer_t<Ret, void*, Args...>;
  using deleter_t = void (*)(void*);

  fn_t fn;
  void* arg;
  deleter_t deleter;

  continuation(continuation&&) = delete;
  continuation(const continuation&) = delete;
  continuation(const fn_t& fn_, void* arg_ = nullptr,
               const deleter_t& deleter_ = nullptr)
      : fn(fn_), arg(arg_), deleter(deleter_) {}

  ~continuation() {
    // TODO ( print warning if no deleter? )
    if (this->arg && this->deleter) {
      this->deleter(this->arg);
    }
  }

  inline void operator()(Args&&... args) {
    fn(this->arg, std::forward<Args>(args)...);
    this->arg = nullptr;
  }
};
}  // namespace detail
}  // namespace hypercomm

#endif
