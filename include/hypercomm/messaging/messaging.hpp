#ifndef __HYPERCOMM_MESSAGING_MESSAGING_HPP__
#define __HYPERCOMM_MESSAGING_MESSAGING_HPP__

#include "messaging.decl.h"
#include "../core/entry_port.hpp"

namespace hypercomm {
namespace messaging {
struct __msg__ : public CMessage___msg__ {
  entry_port_ptr dst;
  char *payload;

  static void *pack(__msg__ *msg);
  static __msg__ *unpack(void *buf);
  static __msg__ *make_message(const std::size_t& user_size, const entry_port_ptr& dst);
};
}

using message = messaging::__msg__;
using message_ptr = std::shared_ptr<message>;

}

using hypercomm_msg = hypercomm::message;
using CMessage_hypercomm_msg = hypercomm::messaging::CMessage___msg__;

#endif
