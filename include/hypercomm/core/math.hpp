#ifndef __HYPERCOMM_MATH_HPP__
#define __HYPERCOMM_MATH_HPP__

#include <cstdint>

namespace hypercomm {

using hash_code = std::size_t;

inline hash_code hash_combine(const hash_code &lhs, const hash_code &rhs) {
  return lhs + 0x9e3779b9 + (rhs << 6) + (rhs >> 2);
}

namespace binary_tree {
template <typename T>
inline T left_child(const T &i) {
  return (2 * i) + 1;
}

template <typename T>
inline T right_child(const T &i) {
  return (2 * i) + 2;
}

template <typename T>
inline T parent(const T &i) {
  return (i > 0) ? ((i - 1) / 2) : -1;
}

template <typename T>
inline int num_leaves(const T &n) {
  return int(n + 1) / 2;
}
}
}

#endif
