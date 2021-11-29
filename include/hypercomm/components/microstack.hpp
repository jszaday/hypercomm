#ifndef __HYPERCOMM_COMPONENTS_MICROSTACK_HPP__
#define __HYPERCOMM_COMPONENTS_MICROSTACK_HPP__

#include <utility>
#include <vector>

#include "../serialization/serdes.hpp"
#include "../serialization/storage.hpp"

namespace hypercomm {
struct microstack {
 protected:
  std::shared_ptr<microstack> prev_;
  std::size_t start_, size_, end_;
  char* storage_;

  microstack(char* storage, std::size_t size)
      : microstack(nullptr, storage, size) {}

  template <typename T>
  microstack(T&& prev, char* storage, std::size_t size)
      : storage_(storage),
        prev_(std::forward<T>(prev)),
        start_(prev ? prev_->end_ : 0),
        size_(size),
        end_(start_ + size) {}

 public:
  std::shared_ptr<microstack>& unwind(void) { return this->prev_; }

  inline char* operator[](std::size_t pos) {
    if ((this->start_ <= pos) && (pos < this->end_)) {
      return this->storage_ + (pos - this->start_);
    } else if (this->start_ > pos) {
      return (*this->prev_)[pos];
    } else {
      return nullptr;
    }
  }

  template <typename T>
  inline T& at(std::size_t pos) {
    return *(reinterpret_cast<T*>((*this)[pos]));
  }

  inline std::size_t size(void) const { return this->size_; }
};

template <typename... Ts>
struct typed_microstack : public microstack {
 private:
  using storage_type = tuple_storage<Ts...>;
  storage_type storage_;

 public:
  friend puper<typed_microstack<Ts...>>;

  template <typename... Args>
  typed_microstack(const std::shared_ptr<microstack>& prev, Args&&... args)
      : microstack(prev, reinterpret_cast<char*>(&storage_),
                   sizeof(storage_type)),
        storage_(std::forward<Args>(args)...) {}

  template <typename... Args>
  typed_microstack(microstack* prev, Args&&... args)
      : microstack(prev, reinterpret_cast<char*>(&storage_),
                   sizeof(storage_type)),
        storage_(std::forward<Args>(args)...) {}
};
}  // namespace hypercomm

#endif
