#ifndef __HYPERCOMM_COMPONENTS_MICROSTACK_HPP__
#define __HYPERCOMM_COMPONENTS_MICROSTACK_HPP__

#include <utility>
#include <vector>

#include "../serialization/serdes.hpp"
#include "../serialization/storage.hpp"

namespace hypercomm {

struct microstack;

namespace detail {
template <typename T>
inline std::size_t get_depth(const T&);
}  // namespace detail

struct microstack {
  const std::size_t depth;

 protected:
  std::shared_ptr<microstack> prev_;
  std::size_t start_, size_, end_;
  void** storage_;

  microstack(void** storage, std::size_t size)
      : microstack(nullptr, storage, size) {}

  template <typename T>
  microstack(T&& prev, void** storage, std::size_t size)
      : depth(detail::get_depth(prev) + 1),
        storage_(storage),
        prev_(std::forward<T>(prev)),
        start_(prev ? prev_->end_ : 0),
        size_(size),
        end_(start_ + size) {}

 public:
  virtual ~microstack() {}

  std::shared_ptr<microstack>& unwind(void) { return this->prev_; }

  inline void* operator[](std::size_t pos) {
    if ((this->start_ <= pos) && (pos < this->end_)) {
      return this->storage_[pos - this->start_];
    } else if (this->start_ > pos) {
      return (*this->prev_)[pos];
    } else {
      return nullptr;
    }
  }

  template <typename T>
  inline T& at(std::size_t pos) {
    return *(static_cast<T*>((*this)[pos]));
  }

  inline std::size_t size(void) const { return this->size_; }

  virtual microstack* clone(void) const = 0;
};

template <typename... Ts>
struct typed_microstack : public microstack {
 private:
  static constexpr auto n_items_ = sizeof...(Ts);

  static_assert(n_items_ >= 1, "only defined for non-zero stacks");

  using storage_type = tuple_storage<Ts...>;

  storage_type storage_;
  std::array<void*, n_items_> items_;

 public:
  friend puper<typed_microstack<Ts...>>;

  template <typename... Args>
  typed_microstack(const std::shared_ptr<microstack>& prev, Args&&... args)
      : microstack(prev, items_.data(), n_items_),
        storage_(std::forward<Args>(args)...) {
    this->template initialize_address_<(n_items_ - 1)>();
  }

  template <typename... Args>
  typed_microstack(microstack* prev, Args&&... args)
      : microstack(prev, items_.data(), n_items_),
        storage_(std::forward<Args>(args)...) {
    this->template initialize_address_<(n_items_ - 1)>();
  }

  virtual microstack* clone(void) const override {
    return new typed_microstack<Ts...>(this->prev_, this->storage_);
  }

 private:
  template <std::size_t I>
  void* address_of_(void) {
    return &(element_at<I>(this->storage_));
  }

  template <std::size_t I>
  inline typename std::enable_if<(I == 0)>::type initialize_address_(void) {
    this->items_[I] = this->template address_of_<I>();
  }

  template <std::size_t I>
  inline typename std::enable_if<(I > 0)>::type initialize_address_(void) {
    this->template initialize_address_<(I - 1)>();
    this->items_[I] = this->template address_of_<I>();
  }
};

namespace detail {
template <typename T>
inline std::size_t get_depth(const T& stk) {
  return stk ? stk->depth : 0;
}

template <>
inline std::size_t get_depth<std::nullptr_t>(const std::nullptr_t&) {
  return 0;
}
}  // namespace detail
}  // namespace hypercomm

#endif
