#ifndef HYPERCOMM_FLEX_PUP_HPP
#define HYPERCOMM_FLEX_PUP_HPP

#include "common.hpp"

PUPbytes(hypercomm::tasking::task_id);

#ifdef HYPERCOMM_USE_PUP
namespace hypercomm {
namespace flex {
template <typename T>
inline int pup_size(const T& t) {
  return (int)PUP::size(const_cast<T&>(t));
}

template <typename T>
inline void pup_pack(const T& t, char* buf, int len) {
  PUP::toMemBuf(const_cast<T&>(t), buf, (std::size_t)len);
}

template <typename T>
inline void pup_unpack(T& t, char* buf, std::unique_ptr<CkMessage>&& src) {
  PUP::fromMem p(buf);
  p | t;
}
}  // namespace flex
}  // namespace hypercomm
#else
#include "../core/proxy.hpp"
#include "../serialization/pup.hpp"
#include "../serialization/special.hpp"

namespace hypercomm {
namespace flex {
template <typename T>
inline int pup_size(const T& t) {
  return (int)hypercomm::size(t);
}

template <typename T>
inline void pup_pack(const T& t, char* buf, int len) {
  hypercomm::packer s(buf);
  s | t;
  CkAssert(len == s.size());
}

template <typename T>
inline void pup_unpack(T& t, char* buf, std::unique_ptr<CkMessage>&& msg) {
  std::shared_ptr<void> src((void*)msg.release(), CkFreeMsg);
  hypercomm::unpacker s(src, buf);
  s | t;
}
}  // namespace flex
}  // namespace hypercomm
#endif

namespace PUP {
template <>
struct ptr_helper<hypercomm::tasking::task_base_, false>;
}

#endif
