#ifndef __HYPERCOMM_COMPONENTS_SENTINEL_HPP__
#define __HYPERCOMM_COMPONENTS_SENTINEL_HPP__

#include "component.hpp"

namespace hypercomm {

struct sentinel: public component::status_listener,
                 public std::enable_shared_from_this<sentinel> {

 private:
  std::size_t n_expected_ = 0;
  CthThread sleeper_;

 public:
  virtual void on_completion(const component&) override {
    this->n_expected_ -= 1;

    if (this->n_expected_ == 0 && this->sleeper_) {
      CthAwaken(this->sleeper_);
    }
  }

  virtual void on_invalidation(const component&) override {

  }

  void suspend(void) {
    if (this->n_expected_ > 0) {
      CkPrintf("suspending while expecting %lu values\n", this->n_expected_);
      this->sleeper_ = CthSelf();
      CthSuspend();
    }
  }

 private:
  template<typename T>
  void helper(const T& t) {
    this->n_expected_ += 1;

    t->add_listener(this->shared_from_this());
  }

 public:
  template<typename... Ts>
  void expect_all(const Ts&... ts) {
    using dummy = int[];
    (void)dummy { 0, (helper(ts), 0)... };
  }
};

}

#endif
