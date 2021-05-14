#include <hypercomm/messaging/messaging.hpp>
#include <hypercomm/serialization/pup.hpp>
#include <hypercomm/utilities.hpp>

namespace hypercomm {
namespace messaging {

constexpr auto hdr_size = sizeof(__msg__);

void *__msg__::pack(__msg__ *msg){
    msg->payload = (char *)((char *)msg->payload - (char *)msg);
    auto packer = hypercomm::serdes::make_packer((char*)msg + hdr_size);
    hypercomm::pup(packer, msg->dst);
    const auto expected_size = std::size_t(msg->payload) - hdr_size;
    CkAssert(expected_size == packer.size() && "entry port size changed");
#if CMK_DEBUG
    auto str = hypercomm::utilities::buf2str((char*)msg + hdr_size, expected_size);
    CkPrintf("info@%d> packed a %lu byte port with id %lu: %s\n", CkMyPe(), expected_size, identify(*(msg->dst)), str.c_str());
#endif
    return (void *)msg;
}

__msg__ *__msg__::unpack(void *buf) {
    __msg__ *msg = (__msg__ *)buf;
    const auto expected_size = std::size_t(msg->payload) - hdr_size;
    msg->payload = (char *)((size_t)msg->payload + (char *)msg);
#if CMK_DEBUG
    auto str = hypercomm::utilities::buf2str((char*)msg + hdr_size, expected_size);
    CkPrintf("info@%d> unpacking a %lu byte port from: %s\n", CkMyPe(), expected_size, str.c_str());
#endif
    auto unpacker = hypercomm::serdes::make_unpacker(std::shared_ptr<void>{}, (char*)msg + hdr_size);
    hypercomm::pup(unpacker, msg->dst);
    CkAssert(expected_size == unpacker.size() && "entry port size changed");
    return msg;
}

__msg__ *__msg__::make_message(const std::size_t& user_size, const entry_port_ptr& dst) {
    const auto port_size = hypercomm::size(dst);
    const auto total_size = hdr_size + port_size + user_size;
    // NOTE default allocation via new(...) doesn't seem to work here
    auto* raw = CkAllocMsg(__msg__::__idx, total_size, 0, GroupDepNum{});
    auto* msg = new(raw) __msg__;
    msg->dst = dst;
    msg->payload = (char*)msg + hdr_size + port_size;
    return msg;
}

}
}

#include <hypercomm/messaging/messaging.def.h>
