#ifndef __HYPERCOMM_CONSTRUCTION_HPP__
#define __HYPERCOMM_CONSTRUCTION_HPP__

#include <pup.h>

namespace hypercomm {

// reconstruct(ers) call placement new on a deserialized obj

// reconstructer for types that use PUP::reconstruct
template <typename T>
inline typename std::enable_if<
    std::is_constructible<T, PUP::reconstruct>::value>::type
reconstruct(T* p) {
  ::new (p) T(PUP::reconstruct());
}

// reconstructer for chare-types that use CkMigrateMessage*
template <typename T>
inline typename std::enable_if<
    std::is_constructible<T, CkMigrateMessage*>::value &&
    !std::is_constructible<T, PUP::reconstruct>::value>::type
reconstruct(T* p) {
  ::new (p) T(nullptr);
}

// reconstructor for default constructible types
template <typename T>
inline typename std::enable_if<
    !std::is_constructible<T, PUP::reconstruct>::value &&
    !std::is_constructible<T, CkMigrateMessage*>::value &&
    std::is_default_constructible<T>::value>::type
reconstruct(T* p) {
  ::new (p) T();
}

namespace tags {
struct no_init {};

using reconstruct = PUP::reconstruct;
}  // namespace tags

// temporary storage for an object of a given type
template <typename T>
struct temporary {
  typename std::aligned_storage<sizeof(T), alignof(T)>::type data;

  // used to when there should be no initialization whatsoever
  temporary(const tags::no_init&) {}

  temporary(const tags::reconstruct&) { reconstruct(&(this->value())); }

  template <typename... Args>
  temporary(Args... args) {
    ::new (&this->value()) T(std::forward<Args>(args)...);
  }

  ~temporary() { value().~T(); }

  const T& value(void) const { return *(reinterpret_cast<const T*>(&data)); }

  T& value(void) { return *(reinterpret_cast<T*>(&data)); }
};
}  // namespace hypercomm

#endif
