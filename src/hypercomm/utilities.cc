#include <hypercomm/utilities.hpp>

namespace hypercomm {
namespace utilities {

std::shared_ptr<CkMessage> wrap_message(CkMessage* msg) {
  return std::shared_ptr<CkMessage>(msg, [](CkMessage* msg) { CkFreeMsg(msg); });
}

std::shared_ptr<CkMessage> copy_message(const std::shared_ptr<CkMessage>& msg) {
  auto msg_raw = msg.get();
  return wrap_message((CkMessage*)CkCopyMsg((void**)&msg_raw));
}

CkMessage* unwrap_message(std::shared_ptr<CkMessage>&& msg) {
  auto msg_raw = msg.get();
  if (msg.use_count() == 1) {
    ::new (&msg) std::shared_ptr<CkMessage>{};
    return msg_raw;
  } else {
    return (CkMessage*)CkCopyMsg((void**)&msg_raw);
  }
}


void pack_message(CkMessage* msg) {
  auto idx = UsrToEnv(msg)->getMsgIdx();
  if (_msgTable[idx]->pack) {
    auto newMsg = _msgTable[idx]->pack(msg);
    CkAssert(msg == newMsg && "message changed due to packing!");
  }
}

void unpack_message(CkMessage* msg) {
  auto idx = UsrToEnv(msg)->getMsgIdx();
  if (_msgTable[idx]->unpack) {
    auto newMsg = _msgTable[idx]->unpack(msg);
    CkAssert(msg == newMsg && "message changed due to packing!");
  }
}

}
}
