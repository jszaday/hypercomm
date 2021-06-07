#ifndef __HYPERCOMM_COMPONENTS_SENTINEL_HPP__
#define __HYPERCOMM_COMPONENTS_SENTINEL_HPP__

#include "component.hpp"

namespace hypercomm {

class sentinel: public component::status_listener,
                 public std::enable_shared_from_this<sentinel> {

 public:
  using group_type = std::shared_ptr<std::vector<component::id_t>>;

 private:
  std::map<component::id_t, group_type> groups_;
  std::size_t n_expected_ = 0;
  CthThread sleeper_;

 public:
  virtual void on_completion(const component& com) override {
    const auto& id = com.id;
    auto search = this->groups_.find(id);
    if (search != std::end(this->groups_)) {
      auto& group = *(search->second);
      for (const auto& peer : group) {
        if (peer != id) {
          // TODO force invalidation
        }
      }

      this->groups_.erase(id);
    }

    this->n_expected_ -= 1;

    if (this->n_expected_ == 0 && this->sleeper_) {
      CthAwaken(this->sleeper_);
    }
  }

  virtual void on_invalidation(const component& com) override {
    const auto& id = com.id;
    auto search = this->groups_.find(id);
    if (search == std::end(this->groups_)) {
      // TODO enable on_failure?
      CkAbort("unexpected invalidation of dependent");
    } else {
      auto& group = *(search->second);
      auto within = std::find(std::begin(group), std::end(group), id);
      group.erase(within);
      this->groups_.erase(search);
    }
  }

  void suspend(void) {
    if (this->n_expected_ > 0) {
      this->sleeper_ = CthSelf();
      CthSuspend();
    }
  }

 private:
  template<typename T>
  inline void helper(const T& t) {
    this->n_expected_ += 1;

    t->add_listener(this->shared_from_this());
  }

 public:
  template<typename... Ts>
  void expect_all(const Ts&... ts) {
    using dummy = int[];
    (void)dummy { 0, (helper(ts), 0)... };
  }

  // TODO add expect any
};

}

#endif
