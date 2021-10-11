#include <hypercomm/messaging/packing.hpp>
#include <hypercomm/messaging/messaging.decl.h>

#include <hypercomm/core/config.hpp>
#include <hypercomm/core/module.hpp>
#include <hypercomm/core/entry_port.hpp>

#include <hypercomm/utilities.hpp>

namespace hypercomm {

namespace messaging {

constexpr auto hdr_size = sizeof(__msg__);
constexpr auto kIgnored = ptr_record::IGNORED;

void *__msg__::pack(__msg__ *msg) {
  msg->payload = (char *)((char *)msg->payload - (char *)msg);
  const auto has_port = (bool)msg->dst;
  const auto avail_size = std::size_t(msg->payload) - hdr_size;
  const auto real_size =
      has_port ? hypercomm::size(msg->dst) : sizeof(kIgnored);
  if (real_size <= avail_size) {
    auto *buf = (char *)msg + hdr_size;
    if (has_port) {
      packer s(buf);
      hypercomm::pup(s, msg->dst);
    } else {
      *(reinterpret_cast<ptr_record::kind_t *>(buf)) = kIgnored;
    }
    return (void *)msg;
  } else {
    // TODO introduce HYPERCOMM_NO_COPYING and abort only when undefined
    CkAbort("increase HYPERCOMM_PORT_SIZE to at least %lu bytes", real_size);
  }
}

__msg__ *__msg__::unpack(void *raw) {
  __msg__ *msg = (__msg__ *)raw;
  const auto expected_size = std::size_t(msg->payload) - hdr_size;
  msg->payload = (char *)((size_t)msg->payload + (char *)msg);
#if CMK_VERBOSE
  auto str =
      hypercomm::utilities::buf2str((char *)msg + hdr_size, expected_size);
  CkPrintf("info@%d> unpacking a %lu byte port from: %s\n", CkMyPe(),
           expected_size, str.c_str());
#endif
  auto *buf = (char *)msg + hdr_size;
  auto has_port =
      *(reinterpret_cast<hypercomm::ptr_record::kind_t *>(buf)) != kIgnored;
  if (has_port) {
    unpacker s(std::shared_ptr<void>{}, buf);
    hypercomm::pup(s, msg->dst);
    CkAssertMsg(expected_size >= s.size(), "entry port size changed");
  } else {
    new (&msg->dst) entry_port_ptr();
  }
  return msg;
}

bool locality_registered_(void) {
  static bool registered_ = (CkGetChareIdx("hypercomm::locality_base_") != -1);
  return registered_;
}

__msg__ *__msg__::make_message(const std::size_t &user_size,
                               const entry_port_ptr &dst) {
  const auto real_size = hypercomm::size(dst);
  const auto port_size = (real_size < kMinPortSize) ? kMinPortSize : real_size;
  const auto total_size = hdr_size + port_size + user_size;
  // NOTE default allocation via new(...) doesn't seem to work here
  auto *raw = CkAllocMsg(__msg__::__idx, total_size, 0, GroupDepNum{});
  auto *msg = new (raw) __msg__;
  auto *env = UsrToEnv(msg);
  msg->dst = dst;
  msg->payload = (char *)msg + hdr_size + port_size;
  env->setRef(0x0);
  if (locality_registered_()) {
    // NOTE ( this is occasionally relied on, but probably shouldn't be )
    env->setEpIdx(CkIndex_locality_base_::idx_demux_CkMessage());
  }
  return msg;
}

__msg__ *__msg__::make_null_message(const entry_port_ptr &dst) {
  auto msg = make_message(0x0, dst);
  UsrToEnv(msg)->setRef(0b1 << __attribs__::kNull);
  return msg;
}
}  // namespace messaging
}  // namespace hypercomm
