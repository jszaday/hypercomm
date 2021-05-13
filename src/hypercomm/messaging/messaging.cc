#include <hypercomm/messaging/messaging.hpp>
#include <hypercomm/serialization/pup.hpp>

namespace hypercomm {
namespace messaging {

constexpr auto hdr_size = sizeof(__msg__);

void *__msg__::pack(__msg__ *msg){
    msg->payload = (char *)((char *)msg->payload - (char *)msg);
    const auto expected_size = std::size_t(msg->payload) - hdr_size;
    auto packer = hypercomm::serdes::make_packer((char*)msg + hdr_size);
    hypercomm::pup(packer, msg->dst);
    CkAssert(expected_size == packer.size() && "entry port size changed");
    return (void *)msg;
}

__msg__ *__msg__::unpack(void *buf) {
    __msg__ *msg = (__msg__ *)buf;
    const auto expected_size = std::size_t(msg->payload) - hdr_size;
    msg->payload = (char *)((size_t)msg->payload + (char *)msg);
    auto unpacker = hypercomm::serdes::make_unpacker(std::shared_ptr<void>{}, (char*)msg + hdr_size);
    hypercomm::pup(unpacker, msg->dst);
    CkAssert(expected_size == unpacker.size() && "entry port size changed");
    return msg;
}

__msg__ *__msg__::make_message(const std::size_t& user_size, const entry_port_ptr& dst) {
    const auto port_size = hypercomm::size(dst);
    const auto total_size = hdr_size + port_size + user_size;
    auto* msg = new(total_size) __msg__;
    msg->dst = dst;
    msg->payload = (char*)msg + hdr_size + port_size;
    return msg;
}

}
}

#include <hypercomm/messaging/messaging.def.h>
