#ifndef __HYPERCOMM_COMPONENTS_VARSTACK_HPP__
#define __HYPERCOMM_COMPONENTS_VARSTACK_HPP__

#include <memory>
#include <utility>
#include <vector>

#define __HYPERCOMM_VARSTACK_OFFSET__ \
  (sizeof(varstack) + (sizeof(varstack) % ALIGN_BYTES))

namespace hypercomm {
struct varstack {
 private:
  std::shared_ptr<varstack> prev;
  std::size_t start, size, end;

  varstack(std::size_t size_) : start(0), size(size_), end(size_) {}

  varstack(const std::shared_ptr<varstack>& prev_, std::size_t size_)
      : prev(prev_),
        start(prev_ ? prev_->end : 0),
        size(size_),
        end(start + size) {}

 public:
  char* operator[](std::size_t pos) {
    if ((this->start <= pos) && (pos < this->end)) {
      auto* raw = (char*)this + __HYPERCOMM_VARSTACK_OFFSET__;
      return raw + (pos - this->start);
    } else if (this->start > pos) {
      return (*this->prev)[pos];
    } else {
      return nullptr;
    }
  }

  template <typename T>
  T& at(std::size_t pos) {
    return *(reinterpret_cast<T*>((*this)[pos]));
  }

  static varstack* make_stack(std::size_t size) {
    auto* raw = ::operator new (size + __HYPERCOMM_VARSTACK_OFFSET__);
    return new ((varstack*)raw) varstack(size);
  }

  static varstack* make_stack(const std::shared_ptr<varstack>& prev,
                              std::size_t size) {
    auto* raw = ::operator new (size + __HYPERCOMM_VARSTACK_OFFSET__);
    return new ((varstack*)raw) varstack(prev, size);
  }
};
}  // namespace hypercomm

#endif
