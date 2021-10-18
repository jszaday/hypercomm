#ifndef __HYPERCOMM_COMPONENTS_BASE_HPP__
#define __HYPERCOMM_COMPONENTS_BASE_HPP__

#define HYPERCOMM_ERROR_CHECKING 1

#include <typeindex>

#include "identifiers.hpp"
#include "../core/config.hpp"
#include "../core/value.hpp"

namespace hypercomm {
namespace components {
enum status_ { kCompletion, kInvalidation };

class base_ {
 public:
  const std::size_t id;
  const std::size_t n_inputs;
  const std::size_t n_outputs;

  using accept_fn_t = void (*)(base_*, deliverable&&);
  using status_listener_fn_t = void (*)(const base_*, status_, void*);

  // TODOs:
  // - add_listener
  // - update_destination(callback)
  // - ret inv values

 protected:
  bool active;
  bool activated;
  bool persistent;
  struct listener_;

 private:
  // effectively vtables of typed acceptors for each port
  const accept_fn_t* acceptors;
  std::list<listener_> listeners_;

#if HYPERCOMM_ERROR_CHECKING
  const std::type_index* in_types;
#endif

 protected:
#if HYPERCOMM_ERROR_CHECKING
  base_(std::size_t id_, std::size_t n_inputs_, std::size_t n_outputs_,
        const accept_fn_t* acceptors_, const std::type_index* in_types_)
      : id(id_),
        n_inputs(n_inputs_),
        n_outputs(n_outputs_),
        active(false),
        activated(false),
        persistent(false),
        acceptors(acceptors_),
        in_types(in_types_) {}
#endif

 public:
  using listener_type = typename decltype(listeners_)::iterator;

  virtual ~base_() {}

  virtual void activate(void) = 0;
  virtual void deactivate(status_ status) = 0;
  virtual bool collectible(void) const = 0;

  template <typename... Args>
  inline listener_type add_listener(Args... args) {
    this->listeners_.emplace_front(std::forward<Args>(args)...);
    return std::begin(this->listeners_);
  }

  inline void remove_listener(const listener_type& it) {
    if (it != std::end(this->listeners_)) {
      this->listeners_.erase(it);
    }
  }

  inline bool is_active(void) const { return this->active; }

  // we need a way to accept arbitrary messages
  inline void accept(std::size_t port, deliverable&& dev) {
    CkAssertMsg(port < this->n_inputs, "port out of range!");
    (this->acceptors[port])(this, std::move(dev));
  }

#if HYPERCOMM_ERROR_CHECKING
  // checks whether a given port can accept a value with the type
  // given by idx, this only used in strict/error-checking modes
  inline bool accepts(std::size_t port, const std::type_index& idx) const {
    return (port < this->n_inputs) && (idx == this->in_types[port]);
  }
#endif
 protected:
  inline void notify_listeners(status_ signal) {
    while (!this->listeners_.empty()) {
      auto& l = this->listeners_.back();
      l(this, signal);
      this->listeners_.pop_back();
    }
  }

  struct listener_ {
    using fn_t = status_listener_fn_t;
    using deleter_t = void (*)(void*);

    fn_t fn;
    void* arg;
    deleter_t deleter;

    listener_(listener_&&) = delete;
    listener_(const listener_&) = delete;
    listener_(const fn_t& fn_, void* arg_ = nullptr,
              const deleter_t& deleter_ = nullptr)
        : fn(fn_), arg(arg_), deleter(deleter_) {}

    ~listener_() {
      if (this->arg) this->deleter(this->arg);
    }

    inline void operator()(const base_* self, status_ signal) {
      fn(self, signal, this->arg);
      this->arg = nullptr;
    }
  };
};
}  // namespace components
}  // namespace hypercomm

#endif
