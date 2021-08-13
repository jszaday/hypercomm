#ifndef __HYPERCOMM_MESSAGING_MESSAGING_HPP__
#define __HYPERCOMM_MESSAGING_MESSAGING_HPP__

#include <hypercomm/messaging/messaging.decl.h>
#include "../core/common.hpp"

namespace hypercomm {
namespace messaging {

static constexpr auto __null_msg__ =
    std::numeric_limits<CMK_REFNUM_TYPE>::max();

enum __attribs__ : std::uint8_t { kNull = 0, kRedn = 1 };

struct __msg__ : public CMessage___msg__ {
  entry_port_ptr dst;
  char *payload;

  static void *pack(__msg__ *msg);
  static __msg__ *unpack(void *buf);
  static __msg__ *make_message(const std::size_t &user_size,
                               const entry_port_ptr &dst);
  static __msg__ *make_null_message(const entry_port_ptr &dst);

  inline bool is_null(void) const {
    return UsrToEnv(this)->getRef() & (0b1 << __attribs__::kNull);
  }

  inline bool is_redn(void) const {
    return UsrToEnv(this)->getRef() & (0b1 << __attribs__::kRedn);
  }

  inline void set_redn(const bool &value) const {
    constexpr auto mask = 0b1 << __attribs__::kRedn;
    auto ref = UsrToEnv(this)->getRef();
    if (value) {
      ref |= mask;
    } else {
      ref &= ~mask;
    }
    UsrToEnv(this)->setRef(ref);
  }

  static inline const int &index(void) { return __idx; }
};

}  // namespace messaging
}  // namespace hypercomm

#endif
