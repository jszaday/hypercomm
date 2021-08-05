#ifndef __HYPERCOMM_MATH_HPP__
#define __HYPERCOMM_MATH_HPP__

#include <cstdint>

namespace hypercomm {

using hash_code = std::size_t;

inline hash_code hash_combine(const hash_code &lhs, const hash_code &rhs) {
  return lhs + 0x9e3779b9 + (rhs << 6) + (rhs >> 2);
}

namespace utilities {
template <class, typename Enable = void>
struct hash;
}

template <class T>
inline std::size_t hash_iterable(const T &t) {
  hash_code seed = 0x0;
  for (const auto &i : t) {
    using element_type = typename std::decay<decltype(i)>::type;
    seed = hash_combine(seed, utilities::hash<element_type>()(i));
  }
  return seed;
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
}  // namespace binary_tree
}  // namespace hypercomm

#endif
