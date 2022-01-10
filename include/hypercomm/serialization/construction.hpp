#ifndef __HYPERCOMM_CONSTRUCTION_HPP__
#define __HYPERCOMM_CONSTRUCTION_HPP__

#include <pup.h>
#include <memory>

namespace hypercomm {

namespace tags {
using reconstruct = PUP::reconstruct;
}

// reconstruct(ers) call placement new on a deserialized obj

// reconstructer for types that use tags::reconstruct
template <typename T>
inline typename std::enable_if<
    std::is_constructible<T, tags::reconstruct>::value>::type
reconstruct(T* p) {
  ::new (p) T(tags::reconstruct());
}

// reconstructer for chare-types that use CkMigrateMessage*
template <typename T>
inline typename std::enable_if<
    std::is_constructible<T, CkMigrateMessage*>::value &&
    !std::is_constructible<T, tags::reconstruct>::value>::type
reconstruct(T* p) {
  ::new (p) T(nullptr);
}

// reconstructor for default constructible types
template <typename T>
inline typename std::enable_if<
    !std::is_constructible<T, tags::reconstruct>::value &&
    !std::is_constructible<T, CkMigrateMessage*>::value &&
    std::is_default_constructible<T>::value>::type
reconstruct(T* p) {
  ::new (p) T();
}

namespace tags {
struct no_init {};

struct allocate {};

template <typename T>
struct use_buffer {
  std::shared_ptr<T> buffer;

  template <typename... Args>
  use_buffer(Args&&... args) : buffer(std::forward<Args>(args)...) {}
};
}  // namespace tags

enum storage_scheme { kInline, kBuffer };

template <typename T, storage_scheme Scheme = kInline>
struct temporary;

// temporary storage for an object of a given type
template <typename T>
struct temporary<T, kInline> {
  typename std::aligned_storage<sizeof(T), alignof(T)>::type data;

  // used to when there should be no initialization whatsoever
  temporary(tags::no_init) {}

  temporary(tags::reconstruct) { reconstruct(&(this->value())); }

  template <typename... Args>
  temporary(Args&&... args) {
    ::new (&this->value()) T(std::forward<Args>(args)...);
  }

  ~temporary() { value().~T(); }

  inline T& value(void) { return *(reinterpret_cast<T*>(&data)); }

  inline const T& value(void) const {
    return *(reinterpret_cast<const T*>(&data));
  }
};

template <typename T>
struct temporary<T, kBuffer> {
  std::shared_ptr<T> data;

  // used to when there should be no initialization whatsoever
  temporary(tags::no_init) {}
  temporary(tags::reconstruct) {}
  temporary(tags::allocate) : data((T*)(::operator new(sizeof(T)))) {}
  temporary(tags::use_buffer<T>&& _) : data(std::move(_.buffer)) {}

  template <typename... Args>
  temporary(Args&&... args) : temporary(tags::allocate{}) {
    ::new (&this->value()) T(std::forward<Args>(args)...);
  }

  T& value(void) { return *data; }

  const T& value(void) const { return *data; }
};

// buffers should just be buffers, y'know?
template <typename T>
struct temporary<std::shared_ptr<T>, kBuffer> {
  std::shared_ptr<T> data;

  // used to when there should be no initialization whatsoever
  temporary(tags::no_init) {}
  temporary(tags::reconstruct) {}
  temporary(tags::allocate) : data((T*)(::operator new(sizeof(T)))) {}
  temporary(tags::use_buffer<T>&& _) : data(std::move(_.buffer)) {}

  template <typename... Args>
  temporary(Args&&... args) : temporary(tags::allocate{}) {
    ::new (this->get()) T(std::forward<Args>(args)...);
  }

  inline T* get(void) { return this->data.get(); }

  inline std::shared_ptr<T>& value(void) { return this->data; }

  inline const std::shared_ptr<T>& value(void) const { return this->data; }
};
}  // namespace hypercomm

#endif
