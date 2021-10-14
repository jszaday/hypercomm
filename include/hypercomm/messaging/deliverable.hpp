#ifndef __HYPERCOMM_MESSAGING_DELIVERABLE_HPP__
#define __HYPERCOMM_MESSAGING_DELIVERABLE_HPP__

#include "../core/value.hpp"
#include "../serialization/construction.hpp"

namespace hypercomm {

struct deliverable {
  enum kind_ { kMessage, kValue };

  kind_ kind;

  template <typename T>
  struct kind_for_;

  union u_storage_ {
    CkMessage* msg;
    hyper_value* val;

    u_storage_(tags::no_init) {}

    u_storage_(CkMessage* msg_) : msg(msg_) {}

    u_storage_(hyper_value* val_) : val(val_) {}
  } storage;

  deliverable(CkMessage* msg) : kind(kMessage), storage(msg) {}

  deliverable(hyper_value* val) : kind(kValue), storage(val) {}

  template <typename T>
  deliverable(std::unique_ptr<T>&& ptr) : deliverable(ptr.release()) {}

  ~deliverable() {
    switch (kind) {
      case kMessage:
        if (this->storage.msg) delete this->storage.msg;
        break;
      case kValue:
        if (this->storage.val) delete this->storage.val;
        break;
    }
  }

  deliverable(const deliverable&) = delete;

  deliverable(deliverable&& other)
      : kind(other.kind), storage(tags::no_init()) {
    switch (kind) {
      case kMessage:
        new (&this->storage) u_storage_(other.release<CkMessage>());
        break;
      case kValue:
        new (&this->storage) u_storage_(other.release<hyper_value>());
        break;
    }
  }

  inline bool is_message(void) const {
    return (this->kind == kMessage);
  }

  template <typename T>
  inline T* release(void) {
    auto expected = kind_for_<T>::value;
    auto& ref = *(
        (expected == kMessage) ? (T**)&this->storage.msg
                               : (T**)&this->storage.val
    );
    CkAssert((this->kind == expected) && ref);
    auto* old = ref;
    ref = nullptr;
    return old;
  }
};

template <>
struct deliverable::kind_for_<CkMessage> {
  static constexpr auto value = kMessage;
};

template <>
struct deliverable::kind_for_<hyper_value> {
  static constexpr auto value = kValue;
};
}  // namespace hypercomm

#endif
