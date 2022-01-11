#ifndef __HYPERCOMM_UTILS_OTP_HPP__
#define __HYPERCOMM_UTILS_OTP_HPP__

#include <map>
#include <memory>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "../core/config.hpp"

namespace hypercomm {
namespace utilities {

template <class T, std::size_t N>
class fwd_pool {
 private:
  static constexpr auto align = alignof(T);
  static constexpr auto item_size = sizeof(T) + align;
  static constexpr auto storage_size = item_size * N;

 public:
  using storage_type = typename std::aligned_storage<storage_size, align>::type;

 public:
  using header_type = std::uint8_t;

  enum block_header : header_type { kPool, kHeap };

  storage_type storage;
  std::size_t offset;

 public:
  using pointer = typename std::allocator<T>::pointer;
  using size_type = typename std::allocator<T>::size_type;
  using value_type = typename std::allocator<T>::value_type;

  template <class Type>
  struct rebind {
    typedef fwd_pool<Type, N> other;
  };

  pointer allocate(size_type n, const void* = 0) {
    header_type* raw;
    if (n != 1) {
      throw std::bad_alloc();
    } else if ((offset + item_size) <= storage_size) {
      raw = ((header_type*)&storage + offset);
      *raw = kPool;
      offset += item_size;
    } else {
      raw = (header_type*)aligned_alloc(align, item_size);
      *raw = kHeap;
    }
    return (T*)(raw + align);
  }

  void deallocate(pointer ptr, size_type n) {
    assert(n == 1);
    header_type* raw = (header_type*)ptr - align;
    if (*raw == kHeap) {
      free(raw);
    } else {
      assert(*raw == kPool);
      if (raw == ((header_type*)&storage + offset - item_size)) {
        offset -= item_size;
      }
    }
  }

  bool operator==(const fwd_pool<T, N>& other) const {
    return (this == &other);
  }
};
}  // namespace utilities

template <typename Key, typename T, typename Compare = std::less<Key>, std::size_t N = kStackSize>
using small_map = std::map<Key, T, Compare, utilities::fwd_pool<std::pair<const Key, T>, N>>;
}  // namespace hypercomm

#endif
