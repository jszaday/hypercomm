#ifndef __HYPERCOMM_MESSAGING_DELIVERY_HPP__
#define __HYPERCOMM_MESSAGING_DELIVERY_HPP__

#include "../core/entry_port.hpp"
#include "../core/value.hpp"

namespace hypercomm {

namespace detail {

struct payload;

using payload_ptr = std::unique_ptr<payload>;

void delete_value_(hyper_value* value, CkDataMsg* msg);

struct payload {
 private:
  enum types_ : uint8_t { kValue, kMessage };

  const types_ type_;

  union u_options_ {
    struct s_value_ {
      entry_port_ptr port_;
      value_ptr value_;
      // a value may be null, so we only check the port for validity
      inline bool valid(void) const { return (bool)this->port_; }
    } value_;

    CkMessage* msg_;

    u_options_(const entry_port_ptr& _1, value_ptr&& _2) {
      new (&this->value_) s_value_{.port_ = _1, .value_ = std::move(_2)};
    }

    u_options_(CkMessage* _1) : msg_(_1) {}

    ~u_options_() {}
  } options_;

 public:
  payload(void) = delete;
  payload(const payload&) = delete;
  payload(payload&& other) = delete;

  payload(CkMessage* _1) : options_(_1), type_(kMessage) {}
  payload(CkMarshalledMessage&& _1) : payload(_1.getMessage()) {}
  payload(const entry_port_ptr& _1, value_ptr&& _2)
      : options_(_1, std::move(_2)), type_(kValue) {}

  ~payload() {
    if (this->type_ == kMessage && this->options_.msg_) {
      CkFreeMsg(this->options_.msg_);
    } else if (this->type_ == kValue) {
      auto& pair = this->options_.value_;
      pair.port_.~entry_port_ptr();
      pair.value_.~value_ptr();
    }
  }

  inline bool valid(void) const {
    return (this->type_ == kMessage && this->options_.msg_) ||
           (this->type_ == kValue && this->options_.value_.valid());
  }

  static void process(ArrayElement* elt, payload_ptr&&, const bool& immediate);

  inline CkMessage* release(void) {
    CkAssert(this->valid());
    if (this->type_ == kMessage) {
      auto msg = this->options_.msg_;
      this->options_.msg_ = nullptr;
      return msg;
    } else {
      auto& value = this->options_.value_;
      auto pair = value.value_->as_nocopy();
      if (pair.first != nullptr) {
        auto* released = value.value_.release();
        // deletes the value when the rts is done with it
        CkCallback cb((CkCallbackFn)&delete_value_, released);
        CkNcpyBuffer src(pair.first, pair.second, cb, CK_BUFFER_REG, CK_BUFFER_NODEREG);
        auto size = PUP::size(src);
        auto* msg = message::make_message(size, std::move(value.port_));
        msg->set_zero_copy(true);
        PUP::toMemBuf(src, msg->payload, size);
        return msg;
      } else {
        // pack the value with its destination port to form a message
        return repack_to_port(std::move(value.port_), std::move(value.value_));
      }
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
