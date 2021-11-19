#ifndef __HYPERCOMM_COMPONENTS_STATE_SERVER_HPP__
#define __HYPERCOMM_COMPONENTS_STATE_SERVER_HPP__

#include <map>
#include <memory>

namespace hypercomm {
template <typename T>
class state_server {
  using ptr_type = std::unique_ptr<T>;
  using map_type = std::map<std::size_t, ptr_type>;

  map_type avail_;
  bool is_inserting_;

 public:
  using iterator = typename map_type::iterator;
  using state_type = std::pair<std::size_t, ptr_type>;

  state_server(void) : is_inserting_(false) {}

  inline bool done(void) const {
    return this->avail_.empty() && !this->is_inserting_;
  }

  inline void done_inserting(void) { this->is_inserting_ = false; }

  template <typename... Args>
  inline iterator put_state(Args&&... args) {
    auto ins = this->avail_.emplace(std::forward<Args>(args)...);
    this->is_inserting_ = ins.second;
    CkAssertMsg(this->is_inserting_, "insertion did not occur!");
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
}  // namespace hypercomm

#endif
