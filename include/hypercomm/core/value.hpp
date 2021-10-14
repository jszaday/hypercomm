#ifndef __HYPERCOMM_CORE_VALUE_HPP__
#define __HYPERCOMM_CORE_VALUE_HPP__

#include <typeindex>
#include "../utilities.hpp"

namespace hypercomm {

class hyper_value;

using value_ptr = std::unique_ptr<hyper_value>;

class value_source {
 public:
  virtual void take_back(value_ptr&&) = 0;
};

class hyper_value {
 public:
  using message_type = CkMessage*;
  using source_type = std::shared_ptr<value_source>;

  source_type source;
  const bool pupable;

  hyper_value(const bool& _ = false) : pupable(_) {}
  virtual ~hyper_value() = default;
  virtual void pup_buffer(serdes& s, const bool& encapsulate) {
    NOT_IMPLEMENTED;
  }
  virtual message_type release(void) = 0;
  virtual bool recastable(void) const = 0;
  virtual const std::type_index* get_type(void) const { return nullptr; }
};

inline void try_return(value_ptr&& value) {
  if (value) {
    auto& src = value->source;
    if (src) {
      src->take_back(std::move(value));
      return;
    }
  }
#if CMK_VERBOSE
  CkError("warning> unable to return value %p.\n", value.get());
#endif
}

class plain_value : public hyper_value {
 public:
  message_type msg;

  plain_value(void) : msg(nullptr) {}

  plain_value(message_type _1) : msg(_1) {}

  ~plain_value() {
    if (msg != nullptr) {
      CkFreeMsg(msg);
    }
  }

  virtual bool recastable(void) const override { return msg != nullptr; }

  virtual message_type release(void) override {
    auto value = this->msg;
    this->msg = nullptr;
    return value;
  }
};

class buffer_value : public hyper_value {
 public:
  std::shared_ptr<void> buffer;
  std::size_t size;

  buffer_value(const std::shared_ptr<void>& _1, const std::size_t& _2)
      : buffer(_1), size(_2) {}

  template <typename T>
  inline T* payload(void) const {
    return static_cast<T*>(this->buffer.get());
  }

  virtual bool recastable(void) const override { return (bool)this->buffer; }

  virtual message_type release(void) override { return nullptr; }
};

template <typename T, typename... Args>
inline std::unique_ptr<T> make_value(Args... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}  // namespace hypercomm

#endif
