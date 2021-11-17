#ifndef __HYPERCOMM_COMPONENTS_INBOX_HPP__
#define __HYPERCOMM_COMPONENTS_INBOX_HPP__

#include <tuple>

namespace hypercomm {
namespace components {
template <typename T>
struct inbox_;

template <typename... Ts>
struct inbox_<std::tuple<Ts...>> {
  using in_set = std::tuple<Ts...>;
  using incoming_type = std::deque<in_set>;
  using iterator = typename incoming_type::iterator;

 private:
  incoming_type incoming_;

 public:
  inline static bool is_ready(const in_set& set) {
    return is_ready_<(sizeof...(Ts) - 1)>(set);
  }

  inline iterator find_ready(void) {
    if (!this->incoming_.empty()) {
      for (auto it = std::begin(this->incoming_);
           it != std::end(this->incoming_); it++) {
        if (is_ready(*it)) {
          return it;
        }
      }
    }
    return std::end(this->incoming_);
  }

  template <std::size_t I>
  inline iterator find_gap(void) {
    if (!this->incoming_.empty()) {
      auto search = this->incoming_.rbegin();
      for (; search != this->incoming_.rend(); search++) {
        if (!std::get<I>(*search)) {
          return --search.base();
        }
      }
    }

    return std::end(this->incoming_);
  }

  void empty_buffers(void) {
    if (this->incoming_.empty()) {
      return;
    } else {
      // try to return all unused values and
      for (auto& set : this->incoming_) {
        empty_buffer_<(sizeof...(Ts) - 1)>(set);
      }
      // clear values so they're not used again
      this->incoming_.clear();
    }
  }

  inline void erase(const iterator& it) { this->incoming_.erase(it); }

  inline iterator begin(void) { return std::begin(this->incoming_); }

  inline iterator end(void) { return std::end(this->incoming_); }

  template <typename... Args>
  inline void emplace_front(Args&&... args) {
    this->incoming_.emplace_front(std::forward<Args>(args)...);
  }

 protected:
  template <std::size_t I>
  inline static typename std::enable_if<I == 0, bool>::type is_ready_(
      const in_set& set) {
    return (bool)std::get<I>(set);
  }

  template <std::size_t I>
  inline static typename std::enable_if<I >= 1, bool>::type is_ready_(
      const in_set& set) {
    return (bool)std::get<I>(set) && is_ready_<(I - 1)>(set);
  }

  template <std::size_t I>
  inline static void return_(in_set& set) {
    auto& val = std::get<I>(set);
    if (val) try_return(std::move(val));
  }

  template <std::size_t I>
  inline typename std::enable_if<(I == 0)>::type empty_buffer_(in_set& set) {
    return_<I>(set);
  }

  template <std::size_t I>
  inline typename std::enable_if<(I >= 1)>::type empty_buffer_(in_set& set) {
    this->empty_buffer_<(I - 1)>(set);
    return_<I>(set);
  }
};
}  // namespace components
}  // namespace hypercomm

#endif
