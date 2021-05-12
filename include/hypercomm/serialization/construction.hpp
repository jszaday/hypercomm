#ifndef __HYPERCOMM_CONSTRUCTION_HPP__
#define __HYPERCOMM_CONSTRUCTION_HPP__

#include <pup.h>

namespace hypercomm {

template <typename T>
inline typename std::enable_if<
    std::is_constructible<T, PUP::reconstruct>::value>::type
reconstruct(T* p) {
  ::new (p) T(PUP::reconstruct());
}

template <typename T>
inline typename std::enable_if<
    std::is_constructible<T, CkMigrateMessage*>::value &&
    !std::is_constructible<T, PUP::reconstruct>::value>::type
reconstruct(T* p) {
  ::new (p) T(nullptr);
}

template <typename T>
inline typename std::enable_if<
    !std::is_constructible<T, PUP::reconstruct>::value &&
    !std::is_constructible<T, CkMigrateMessage*>::value &&
    std::is_default_constructible<T>::value>::type
reconstruct(T* p) {
  ::new (p) T();
}

template <typename T>
struct temporary {
  typename std::aligned_storage<sizeof(T), alignof(T)>::type data;

  temporary(void) { reconstruct(&(this->value())); }

  ~temporary() { value().~T(); }

  const T& value(void) const { return *(reinterpret_cast<const T*>(&data)); }

  T& value(void) { return *(reinterpret_cast<T*>(&data)); }
};
}

#endif
