#ifndef __HYPERCOMM_COMPONENTS_SENTINEL_HPP__
#define __HYPERCOMM_COMPONENTS_SENTINEL_HPP__

#include "../core/common.hpp"
#include "../utilities/weak_ref.hpp"

namespace hypercomm {

// NOTE this could be made into a component if it makes sense to
//      do so, but it doesn't really have i/o so i'm hesitant
class sentinel {
 public:
  using group_type = std::shared_ptr<std::vector<component_id_t>>;
  using weak_ref_t = utilities::weak_ref<sentinel>;

 private:
  std::map<component_id_t, group_type> groups_;
  std::shared_ptr<weak_ref_t> weak_;
  CthThread sleeper_ = nullptr;
  std::size_t n_expected_ = 0;
  component_id_t id_;

 public:
  sentinel(const component_id_t& _1) : id_(_1), weak_(new weak_ref_t(this)) {}

  ~sentinel() { weak_->reset(nullptr); }

  void on_completion(const components::base_* com) {
    const auto& id = com->id;
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

  void on_invalidation(const components::base_* com) {
    const auto& id = com->id;
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
  void add_listener(const T& t) {
    (access_context_()->components[t])
        ->add_listener(
            &on_status_change, new std::shared_ptr<weak_ref_t>(this->weak_),
            [](void* value) { delete (std::shared_ptr<weak_ref_t>*)value; });
  }

  template <typename T>
  inline void all_helper(const T& t) {
    this->n_expected_ += 1;
    this->add_listener(t);
  }

  template <typename T>
  inline component_id_t any_helper(T& t) {
    component_id_t id = t;
    this->add_listener(id);
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
    std::vector<component_id_t> base{{any_helper(ts)...}};
    auto group = std::make_shared<std::vector<component_id_t>>(std::move(base));

    for (const auto& id : *group) {
      this->groups_[id] = group;
    }

    this->n_expected_ += 1;
  }

 private:
  using listener_type = decltype(weak_);

  static void on_status_change(const components::base_* com,
                               components::status_ status, void* arg) {
    auto* listener = (listener_type*)arg;
    auto& self = *listener;

    if (self) {
      switch (status) {
        case components::status_::kCompletion:
          (*self)->on_completion(com);
          break;
        case components::status_::kInvalidation:
          (*self)->on_invalidation(com);
          break;
        default:
          break;
      }
    }

    delete listener;
  }
};
}  // namespace hypercomm

#endif
