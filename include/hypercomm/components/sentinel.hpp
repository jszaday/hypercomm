#ifndef __HYPERCOMM_COMPONENTS_SENTINEL_HPP__
#define __HYPERCOMM_COMPONENTS_SENTINEL_HPP__

#include "../core/common.hpp"

namespace hypercomm {

// NOTE this could be made into a component if it makes sense to
//      do so, but it doesn't really have i/o so i'm hesitant
class sentinel : public component::status_listener,
                 public std::enable_shared_from_this<sentinel> {
 public:
  using group_type = std::shared_ptr<std::vector<component::id_t>>;

 private:
  std::map<component::id_t, group_type> groups_;
  CthThread sleeper_ = nullptr;
  std::size_t n_expected_ = 0;
  component::id_t id_;

 public:

  sentinel(const component::id_t& _1) : id_(_1) {}

  virtual void on_completion(const component& com) override {
    const auto& id = com.id;
    auto search = this->groups_.find(id);
    if (search != std::end(this->groups_)) {
      auto& group = *(search->second);
      for (const auto& peer : group) {
        if (peer != id) {
          access_context_()->invalidate_component(peer);
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
  template <typename T>
  inline void all_helper(const T& t) {
    this->n_expected_ += 1;

    (access_context_()->components[t])->add_listener(this->shared_from_this());
  }

  template <typename T>
  inline component::id_t any_helper(T& t) {
    component::id_t id = t;
    (access_context_()->components[id])->add_listener(this->shared_from_this());
    return id;
  }

 public:
  template <typename... Ts>
  void expect_all(const Ts&... ts) {
    using dummy = int[];
    (void)dummy{0, (all_helper(ts), 0)...};
  }

  template <typename... Ts>
  void expect_any(const Ts&... ts) {
    std::vector<component::id_t> base{{any_helper(ts)...}};
    auto group =
        std::make_shared<std::vector<component::id_t>>(std::move(base));

    for (const auto& id : *group) {
      this->groups_[id] = group;
    }

    this->n_expected_ += 1;
  }
};
}

#endif
