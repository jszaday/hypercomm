#ifndef __HYPERCOMM_UTILITIES_WEAK_REF_HPP__
#define __HYPERCOMM_UTILITIES_WEAK_REF_HPP__

namespace hypercomm {
namespace utilities {

template <typename T>
class weak_ref {
  T* value;

 public:
  weak_ref(T* value_) : value(value_) {}

  void reset(T* value_) { this->value = value_; }

  T* operator->(void) { return this->value; }
  T& operator*(void) { return *(this->value); }

  operator bool(void) const { return (bool)this->value; }
};

}  // namespace utilities
}  // namespace hypercomm

#endif
