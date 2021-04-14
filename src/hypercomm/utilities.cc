#include <hypercomm/utilities.hpp>

namespace hypercomm {
namespace utilities {

std::shared_ptr<CkMessage> copy_message(const std::shared_ptr<CkMessage>& msg) {
  auto msg_raw = msg.get();
  auto msg_copy = (CkMessage*)CkCopyMsg((void**)&msg_raw);
  return std::shared_ptr<CkMessage>(msg_copy,
                                    [](CkMessage* msg) { CkFreeMsg(msg); });
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
