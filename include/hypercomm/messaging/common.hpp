#ifndef __HYPERCOMM_MESSAGING_COMMON_HPP__
#define __HYPERCOMM_MESSAGING_COMMON_HPP__

namespace hypercomm {
namespace messaging {
struct __msg__;
}

using message = messaging::__msg__;

int message_index(void);
}  // namespace hypercomm

#endif
