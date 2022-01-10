#ifndef __HYPERCOMM_MESSAGING_PACKING_HPP__
#define __HYPERCOMM_MESSAGING_PACKING_HPP__

#include "messaging.hpp"
#include "../utilities.hpp"
#include "../serialization/special.hpp"

namespace hypercomm {
template <typename... Args>
void unpack(std::shared_ptr<CkMessage>&& msg, Args&... args) {
  unpacker s(msg, utilities::get_message_buffer(msg));
  hypercomm::pup(s, std::forward_as_tuple(args...));
}

template <typename... Args>
void unpack(CkMessage* msg, Args&... args) {
  auto src = utilities::wrap_message(msg);
  unpack(std::move(src), args...);
}

template <typename... Args>
CkMessage* pack(const Args&... _args) {
  auto args = std::forward_as_tuple(const_cast<Args&>(_args)...);
  packer s((char*)nullptr);
  auto size = hypercomm::size(args, &s);
  auto* msg = CkAllocateMarshallMsg(size);
  s.reset(msg->msgBuf);
  auto real_size = pup_size(s, args);
  CkAssert(size == real_size);
  return msg;
}

template <typename... Args>
message* pack_to_port(const entry_port_ptr& dst, const Args&... _args) {
  auto args = std::forward_as_tuple(const_cast<Args&>(_args)...);
  packer s((char*)nullptr);
  auto size = hypercomm::size(args, &s);
  auto* msg = message::make_message(size, dst);
  s.reset(msg->payload);
  auto real_size = pup_size(s, args);
  CkAssert(size == real_size);
  return msg;
}

}  // namespace hypercomm

#endif
