#ifndef __HYPERCOMM_MATH_HPP__
#define __HYPERCOMM_MATH_HPP__

#include <cstdint>

namespace hypercomm {

using hash_code = std::size_t;

inline hash_code hash_combine(const hash_code& lhs, const hash_code& rhs) {
  return lhs + 0x9e3779b9 + (rhs << 6) + (rhs >> 2);
}

}

#endif
