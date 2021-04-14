#ifndef __HYPERCOMM_UTIL_HPP__
#define __HYPERCOMM_UTIL_HPP__

#include <ck.h>
#include <memory>

namespace hypercomm {
namespace utilities {

void pack_message(CkMessage*);
void unpack_message(CkMessage*);

CkMessage* unwrap_message(std::shared_ptr<CkMessage>&&);
std::shared_ptr<CkMessage> wrap_message(CkMessage*);
std::shared_ptr<CkMessage> copy_message(const std::shared_ptr<CkMessage>&);

}
}

#endif