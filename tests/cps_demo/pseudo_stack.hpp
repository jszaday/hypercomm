#ifndef __PSEUDO_STACK_HH__
#define __PSEUDO_STACK_HH__

#include <memory>
#include <utility>
#include <vector>

struct pseudo_stack {
  using entry_t = std::pair<std::shared_ptr<char>, std::size_t>;
  std::vector<entry_t> entries;

  char* operator[](std::size_t pos) {
    std::size_t curr = 0;

    for (auto& entry : this->entries) {
      if (pos <= (entry.second + curr)) {
        return entry.first.get() + (pos - curr);
      } else {
        curr += entry.second;
      }
    }

    return nullptr;
  }

  template <typename T>
  T& at(std::size_t pos) {
    return *(reinterpret_cast<T*>((*this)[pos]));
  }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    this->entries.emplace_back(std::forward<Args>(args)...);
  }

  void allocate(std::size_t sz) {
    this->emplace_back((char*)(::operator new (sz)), sz);
  }
};

#endif
