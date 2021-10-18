#ifndef __HYPERCOMM_COMPONENTS_CONNECTOR_HPP__
#define __HYPERCOMM_COMPONENTS_CONNECTOR_HPP__

#include <cstdint>
#include "../serialization/construction.hpp"
#include "../messaging/destination.hpp"

namespace hypercomm {

template <typename... Args>
void passthru_context_(Args&&... args);

namespace components {
template <typename T>
class connector_ {
  destination* dst_;
  std::aligned_storage<sizeof(destination), alignof(destination)> storage_;

 public:
  connector_(void) : dst_(nullptr) {}

  template <typename... Args>
  connector_(Args&&... args) : dst_((destination*)&storage_) {
    new (dst_) destination(std::forward<Args>(args)...);
  }

  ~connector_() {
    if (dst_) dst_->~destination();
  }

  inline void relay(T&& value) const {
    passthru_context_(*(this->dst_), std::move(value));
  }

  inline bool ready(void) const { return this->dst_ != nullptr; }
};
}  // namespace components
}  // namespace hypercomm

#endif
