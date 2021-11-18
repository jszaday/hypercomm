#ifndef __HYPERCOMM_VARSTACK_HH__
#define __HYPERCOMM_VARSTACK_HH__

#include <memory>
#include <utility>
#include <vector>

#define VARSTACK_OFFSET (sizeof(varstack) + (sizeof(varstack) % ALIGN_BYTES))

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
      auto* raw = (char*)this + VARSTACK_OFFSET;
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
    auto* raw = ::operator new (size + VARSTACK_OFFSET);
    return new ((varstack*)raw) varstack(size);
  }

  static varstack* make_stack(const std::shared_ptr<varstack>& prev,
                              std::size_t size) {
    auto* raw = ::operator new (size + VARSTACK_OFFSET);
    return new ((varstack*)raw) varstack(prev, size);
  }
};

#endif
