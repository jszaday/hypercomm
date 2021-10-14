#ifndef __HYPERCOMM_MESSAGING_DELIVERY_HPP__
#define __HYPERCOMM_MESSAGING_DELIVERY_HPP__

#include "../core/module.hpp"
#include "../core/entry_port.hpp"
#include "../core/zero_copy_value.hpp"
#include "destination.hpp"

namespace hypercomm {

namespace detail {
// TODO ( it would be good to rename this at some point )
message* repack_to_port(const entry_port_ptr& port,
                        value_ptr&& value);

struct payload;

using payload_ptr = std::unique_ptr<payload>;

struct payload {
 private:
  enum types_ : uint8_t { kValue, kMessage };

  const types_ type_;

  union u_options_ {
    struct s_value_ {
      endpoint ep_;
      value_ptr value_;

      template <typename T>
      s_value_(const T& _1, value_ptr&& _2)
          : ep_(_1), value_(std::forward<value_ptr>(_2)) {}
    } value_;

    CkMessage* msg_;

    template <typename T>
    u_options_(const T& _1, value_ptr&& _2) : value_(_1, std::move(_2)) {}

    u_options_(CkMessage* _1) : msg_(_1) {}

    ~u_options_() {}
  } options_;

 public:
  payload(void) = delete;
  payload(const payload&) = delete;
  payload(payload&& other) = delete;
  payload(CkMessage* _1) : options_(_1), type_(kMessage) {}

  template <typename T>
  payload(const T& _1, value_ptr&& _2)
      : options_(_1, std::move(_2)), type_(kValue) {}

  ~payload() {
    if (this->type_ == kValue) {
      this->options_.value_.~s_value_();
    } else if (this->type_ == kMessage && this->options_.msg_) {
      CkFreeMsg(this->options_.msg_);
    }
  }

  inline bool valid(void) const {
    return (this->type_ == kMessage && this->options_.msg_) ||
           (this->type_ == kValue && this->options_.value_.ep_.valid());
  }

  static void process(ArrayElement* elt, payload_ptr&&, const bool& immediate);

  inline CkMessage* release(void) {
    CkAssert(this->valid());
    if (this->type_ == kMessage) {
      auto msg = this->options_.msg_;
      this->options_.msg_ = nullptr;
      return msg;
    } else {
      // pack the value with its destination port to form a message
      auto& value = this->options_.value_;
      auto* msg =
          repack_to_port(std::move(value.ep_.port_), std::move(value.value_));
      UsrToEnv(msg)->setEpIdx(value.ep_.idx_);
      return msg;
    }
  }
};

// convenience method for creating a unique payload ptr
template <typename... Args>
inline payload_ptr make_payload(Args... args) {
  return std::move(payload_ptr(new payload(std::forward<Args>(args)...)));
}
}  // namespace detail

struct delivery {
  char core[CmiReservedHeaderSize];

  CkArrayID aid;
  CkArrayIndex idx;
  detail::payload_ptr payload;

  delivery(const CkArrayID& _1, const CkArrayIndex& _2,
           detail::payload_ptr&& _3)
      : aid(_1), idx(_2), payload(std::move(_3)) {
    CkAssert(this->payload->valid());
    std::fill(this->core, this->core + CmiReservedHeaderSize, '\0');
    CmiSetHandler(this, handler());
  }

  static const int& handler(void);

  void* operator new(std::size_t count) { return CmiAlloc(count); }

  void operator delete(void* blk) { CmiFree(blk); }

 private:
  static void handler_(delivery* msg);
};
}  // namespace hypercomm

#endif
