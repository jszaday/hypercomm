#ifndef __HYPERCOMM_MESSAGING_MESSAGING_HPP__
#define __HYPERCOMM_MESSAGING_MESSAGING_HPP__

#include <hypercomm/messaging/messaging.decl.h>
#include "../core/common.hpp"

namespace hypercomm {
namespace messaging {

static constexpr auto __null_msg__ =
    std::numeric_limits<CMK_REFNUM_TYPE>::max();

enum __attribs__ : std::uint8_t { kNull = 0, kRedn = 1, kZeroCopy = 2 };

struct __msg__ : public CMessage___msg__ {
  entry_port_ptr dst;
  char *payload;

  static void *pack(__msg__ *msg);
  static __msg__ *unpack(void *buf);
  static __msg__ *make_message(const std::size_t &user_size,
                               const entry_port_ptr &dst);
  static __msg__ *make_null_message(const entry_port_ptr &dst);

  inline bool is_zero_copy(void) const {
    return UsrToEnv(this)->getRef() & (0b1 << __attribs__::kZeroCopy);
  }

  inline bool is_null(void) const {
    return UsrToEnv(this)->getRef() & (0b1 << __attribs__::kNull);
  }

  inline bool is_redn(void) const {
    return UsrToEnv(this)->getRef() & (0b1 << __attribs__::kRedn);
  }

  inline void set_redn(const bool &value) {
    this->set_flag_<kRedn>(value);
  }

  inline void set_zero_copy(const bool &value) {
    this->set_flag_<kZeroCopy>(value);
  }

  static inline const int &index(void) { return __idx; }

 private:
  template<__attribs__ which>
  inline void set_flag_(const bool& value) {
    constexpr auto mask = 0b1 << which;
    auto* env = UsrToEnv(this);
    auto ref = env->getRef();
    if (value) {
      ref |= mask;
    } else {
      ref &= ~mask;
    }
    env->setRef(ref);
  }
};

}  // namespace messaging

inline value_ptr msg2value(message *msg) {
  if (msg->is_null()) {
    CkFreeMsg(msg);
    return nullptr;
  } else {
    CkAssertMsg(!msg->is_zero_copy(), "value for msg unavailable!");
    return make_value<plain_value>(msg);
  }
}

inline value_ptr msg2value(typename hyper_value::message_type msg) {
  if (UsrToEnv(msg)->getMsgIdx() == message::index()) {
    return msg2value((message*)msg);
  } else {
    return make_value<plain_value>(msg);
  }
}

inline std::unique_ptr<plain_value> msg2value(
    std::shared_ptr<CkMessage>&& msg) {
  return make_value<plain_value>(utilities::unwrap_message(std::move(msg)));
}
}  // namespace hypercomm

#endif
