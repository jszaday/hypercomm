#ifndef __HYPERCOMM_COMPONENTS_STATE_SERVER_HPP__
#define __HYPERCOMM_COMPONENTS_STATE_SERVER_HPP__

#include <map>
#include <memory>

#include "base.hpp"
#include "../utilities/unshared.hpp"
#include "../utilities/continuation.hpp"

namespace hypercomm {
template <typename T>
class state_server {
  using ptr_type = utilities::unshared_ptr<T>;
  using avail_map_t = std::map<std::size_t, ptr_type>;
  using subscriber_map_t = std::vector<component_id_t>;

  subscriber_map_t subscribers_;
  avail_map_t avail_;
  bool is_inserting_;

 public:
  using iterator = typename avail_map_t::iterator;
  using state_type = std::pair<std::size_t, ptr_type>;

 private:
  using continuation_fn_t = detail::fn_pointer_t<void, void*, state_type&&>;
  using continuation_t = detail::continuation<continuation_fn_t>;
  using continuation_map_t = std::map<std::size_t, continuation_t>;
  continuation_map_t continuations_;

 public:
  state_server(void) : is_inserting_(false) {}

  template <typename... Args>
  inline void emplace_subscriber(Args&&... args) {
    this->subscribers_.emplace_back(std::forward<Args>(args)...);
  }

  inline bool erase_subscriber(component_id_t com) {
    auto search = std::find(std::begin(this->subscribers_),
                            std::end(this->subscribers_), com);
    if (search == std::end(this->subscribers_)) {
      return false;
    } else {
      this->subscribers_.erase(search);
      return true;
    }
  }

  inline bool done(void) const {
    return this->avail_.empty() && !this->is_inserting_;
  }

  inline void done_inserting(void) {
    this->is_inserting_ = false;
    this->check_status_();
  }

  template <typename... Args>
  inline iterator put_state(Args&&... args) {
    auto ins = this->avail_.emplace(std::forward<Args>(args)...);
    this->is_inserting_ = ins.second;
    CkAssertMsg(this->is_inserting_, "insertion did not occur!");
    return ins.first;
  }

  inline iterator find_state(std::size_t key) { return this->avail_.find(key); }

  inline state_type acquire_state(component_id_t com, const iterator& it) {
    auto val = std::move(*it);
    this->avail_.erase(it);
    // component will check if it's done
    // so exclude it from notification
    this->check_status_(com);
    return std::move(val);
  }

  inline bool valid_state(const iterator& it) {
    return it != std::end(this->avail_);
  }

  inline void release_state(state_type&& val) {
    // seek a continuation for this tag
    auto search = this->continuations_.find(val.first);
    // if one was found...
    if (search != std::end(this->continuations_)) {
      // pass it along
      (search->second)(std::move(val));
      // then erase the continuation
      this->continuations_.erase(search);
    }
  }

  template <typename... Args>
  inline void put_continuation(std::size_t tag, Args&&... args) {
    auto ins = this->continuations_.emplace(
        std::piecewise_construct, std::forward_as_tuple(tag),
        std::forward_as_tuple(std::forward<Args>(args)...));
    CkAssert(ins.second);
  }

 private:
  inline void check_status_(component_id_t exclude = 0) {
    // if we exhausted all state:
    if (this->done()) {
      auto it = std::begin(this->subscribers_);
      // invalidate all downstream components...
      // (except the excluded one...)
      while (it != std::end(this->subscribers_)) {
        auto com = *it;
        // we erase first because...
        it = this->subscribers_.erase(it);
        if (exclude != com) {
          // invalidating the component would erase it
          access_context_()->invalidate_component(com);
        }
      }
    }
  }
};
}  // namespace hypercomm

#endif
