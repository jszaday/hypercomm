#include <hypercomm/messaging/packing.hpp>
#include <hypercomm/messaging/interceptor.hpp>

#include <hypercomm/core/config.hpp>
// #include <hypercomm/core/module.hpp>

#include <hypercomm/utilities.hpp>

namespace hypercomm {

CProxy_interceptor interceptor_;

namespace messaging {

constexpr auto hdr_size = sizeof(__msg__);

void *__msg__::pack(__msg__ *msg) {
  msg->payload = (char *)((char *)msg->payload - (char *)msg);
  const auto avail_size = std::size_t(msg->payload) - hdr_size;
  const auto real_size = hypercomm::size(msg->dst);
  if (real_size <= avail_size) {
    auto packer = hypercomm::serdes::make_packer((char *)msg + hdr_size);
    hypercomm::pup(packer, msg->dst);
    return (void *)msg;
  } else {
    // TODO introduce HYPERCOMM_NO_COPYING and abort, then copy in the other
    // case
    CkAbort("increase HYPERCOMM_PORT_SIZE to at least %lu bytes", real_size);
  }
}

__msg__ *__msg__::unpack(void *buf) {
  __msg__ *msg = (__msg__ *)buf;
  const auto expected_size = std::size_t(msg->payload) - hdr_size;
  msg->payload = (char *)((size_t)msg->payload + (char *)msg);
#if CMK_VERBOSE
  auto str =
      hypercomm::utilities::buf2str((char *)msg + hdr_size, expected_size);
  CkPrintf("info@%d> unpacking a %lu byte port from: %s\n", CkMyPe(),
           expected_size, str.c_str());
#endif
  auto unpacker = hypercomm::serdes::make_unpacker(std::shared_ptr<void>{},
                                                   (char *)msg + hdr_size);
  hypercomm::pup(unpacker, msg->dst);
  CkAssert(expected_size >= unpacker.size() && "entry port size changed");
  return msg;
}

// bool locality_registered_(void) {
//   static bool registered_ = (CkGetChareIdx("hypercomm::locality_base_") != -1);
//   return registered_;
// }

__msg__ *__msg__::make_message(const std::size_t &user_size,
                               const entry_port_ptr &dst) {
  const auto real_size = hypercomm::size(dst);
  const auto port_size = (real_size < kMinPortSize) ? kMinPortSize : real_size;
  const auto total_size = hdr_size + port_size + user_size;
  // NOTE default allocation via new(...) doesn't seem to work here
  auto *raw = CkAllocMsg(__msg__::__idx, total_size, 0, GroupDepNum{});
  auto *msg = new (raw) __msg__;
  msg->dst = dst;
  msg->payload = (char *)msg + hdr_size + port_size;
  // if (locality_registered_()) {
  //   UsrToEnv(msg)->setEpIdx(CkIndex_locality_base_::idx_demux_CkMessage());
  // }
  return msg;
}

__msg__ *__msg__::make_null_message(const entry_port_ptr &dst) {
  auto msg = make_message(0x0, dst);
  UsrToEnv(msg)->setRef(__null_msg__);
  return msg;
}
}
}

#include <hypercomm/messaging/messaging.def.h>
