#include <hypercomm/utilities.hpp>
#include <hypercomm/messaging/messaging.hpp>

#include <iomanip>

namespace hypercomm {
namespace utilities {

char* get_message_buffer(const CkMessage* _1) {
  auto msg = const_cast<CkMessage*>(_1);
  auto env = UsrToEnv(msg);
  auto idx = env->getMsgIdx();

  if (idx == message::index()) {
    return static_cast<message*>(msg)->payload;
  } else if (idx == CMessage_CkMarshallMsg::__idx) {
    return static_cast<CkMarshallMsg*>(msg)->msgBuf;
  } else if (idx == CMessage_CkReductionMsg::__idx) {
    return static_cast<char*>(static_cast<CkReductionMsg*>(msg)->getData());
  } else if (idx == CMessage_CkDataMsg::__idx) {
    return static_cast<char*>(static_cast<CkDataMsg*>(msg)->getData());
  } else {
    CkAbort("unsure how to handle msg of type %s.", _msgTable[idx]->name);
  }
}

std::string idx2str(const CkArrayIndex& idx) {
  auto& nDims = idx.dimension;
  if (nDims == 1) {
    return std::to_string(idx.data()[0]);
  } else {
    std::stringstream ss;
    ss << "(";
    for (auto i = 0; i < nDims; i += 1) {
      if (nDims >= 4) {
        ss << idx.shortData()[i] << ",";
      } else {
        ss << idx.data()[i] << ",";
      }
    }
    ss << ")";
    return ss.str();
  }
}

std::string buf2str(const char* data, const std::size_t& size) {
  std::stringstream ss;
  ss << "[ ";
  for (auto i = 0; i < size; i++) {
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
       << (unsigned short)(0xFF & data[i]) << " ";
  }
  ss << "]";
  return ss.str();
}

std::string env2str(const envelope* env) {
  auto* bytes = reinterpret_cast<const char*>(env);
  std::stringstream ss;
  ss << buf2str(bytes, sizeof(envelope)) << "|";
  ss << buf2str(bytes + sizeof(envelope),
                env->getTotalsize() - sizeof(envelope));
  return ss.str();
}

std::shared_ptr<CkMessage> wrap_message(CkMessage* msg) {
  return std::shared_ptr<CkMessage>(msg,
                                    [](CkMessage* msg) { CkFreeMsg(msg); });
}

CkMessage* copy_message(const CkMessage* msg) {
  auto msg_raw = const_cast<CkMessage*>(msg);
  return (CkMessage*)CkCopyMsg((void**)&msg_raw);
}

std::shared_ptr<CkMessage> copy_message(const std::shared_ptr<CkMessage>& msg) {
  return wrap_message(copy_message(msg.get()));
}

void pack_message(CkMessage* msg) {
  auto idx = UsrToEnv(msg)->getMsgIdx();
  if (_msgTable[idx]->pack) {
    auto newMsg = _msgTable[idx]->pack(msg);
    CkAssertMsg(msg == newMsg, "message changed due to packing!");
  }
}

void unpack_message(CkMessage* msg) {
  auto idx = UsrToEnv(msg)->getMsgIdx();
  if (_msgTable[idx]->unpack) {
    auto newMsg = _msgTable[idx]->unpack(msg);
    CkAssertMsg(msg == newMsg, "message changed due to packing!");
  }
}

}  // namespace utilities
}  // namespace hypercomm
