#ifndef __HYPERCOMM_MESSAGING_COMMON_HPP__
#define __HYPERCOMM_MESSAGING_COMMON_HPP__

namespace hypercomm {
namespace messaging {
struct __msg__;

void initialize(void);
}  // namespace messaging

using message = messaging::__msg__;
}  // namespace hypercomm

#endif
