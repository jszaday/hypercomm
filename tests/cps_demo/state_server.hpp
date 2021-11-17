#ifndef __HYPERCOMM_COMPONENTS_STATE_SERVER_HPP__
#define __HYPERCOMM_COMPONENTS_STATE_SERVER_HPP__

#include <map>
#include <memory>

template <typename T>
class state_server_ {
  using avail_t = std::map<std::size_t, T>;
  avail_t avail_;

 public:
  using state_type = std::pair<std::size_t, T>;
  using iterator = typename avail_t::iterator;

  inline bool empty(void) const { return this->avail_.empty(); }

  template <typename... Args>
  inline iterator put_state(Args&&... args) {
    auto ins = this->avail_.emplace(std::forward<Args>(args)...);
    CkAssertMsg(ins.second, "insertion did not occur!");
    return ins.first;
  }

  inline iterator find_state(std::size_t key) { return this->avail_.find(key); }

  inline state_type acquire_state(const iterator& it) {
    auto val = std::move(*it);
    this->avail_.erase(it);
    return std::move(val);
  }

  inline bool valid_state(const iterator& it) {
    return it != std::end(this->avail_);
  }

  inline void release_state(state_type&& val) {}
};

template <typename T>
using shared_state_ = std::shared_ptr<state_server_<T>>;

#endif
