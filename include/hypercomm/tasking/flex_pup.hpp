#ifndef HYPERCOMM_FLEX_PUP_HPP
#define HYPERCOMM_FLEX_PUP_HPP

#include "common.hpp"

PUPbytes(hypercomm::tasking::task_id);

#define HYPERCOMM_USE_PUP

#ifdef HYPERCOMM_USE_PUP
namespace hypercomm {
namespace flex {
using puper_t = PUP::er;

inline void pup_message(puper_t& p, void*& msg) { CkPupMessage(p, &msg); }

inline bool pup_is_unpacking(puper_t& p) { return p.isUnpacking(); }

template <typename T>
inline int pup_size(const T& t) {
  return (int)PUP::size(const_cast<T&>(t));
}

template <typename T>
inline void pup_pack(const T& t, char* buf, int len) {
  PUP::toMemBuf(const_cast<T&>(t), buf, (std::size_t)len);
}
}  // namespace flex
}  // namespace hypercomm

namespace PUP {
template <>
struct ptr_helper<hypercomm::tasking::task_base_, false>;
}
#else
#endif

#endif
