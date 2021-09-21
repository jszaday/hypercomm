#ifndef __HYPERCOMM_CORE_ZC_VALUE_HPP__
#define __HYPERCOMM_CORE_ZC_VALUE_HPP__

#include "value.hpp"
#include "../messaging/common.hpp"

namespace hypercomm {
struct zero_copy_value {
  std::vector<CkNcpyBuffer> buffers;
  std::vector<std::shared_ptr<void>> values;
  std::size_t nReceived = 0;
  message* msg;
  char* offset;

  zero_copy_value(message* _) : msg(_) {}

  inline bool ready(void) const {
    return this->nReceived && (this->nReceived == this->buffers.size());
  }

  inline bool receive(CkNcpyBuffer* which, std::shared_ptr<void>&& value) {
    auto search = std::find_if(std::begin(this->buffers), std::end(this->buffers),
      [&] (const CkNcpyBuffer& other) { return which == &other; }
    );
    if (search == std::end(this->buffers)) {
      return false;
    } else {
      std::size_t pos = search - std::begin(this->buffers);
      if (this->values.empty()) {
        this->values.resize(this->buffers.size());
      }
      this->values[pos] = std::move(value);
      this->nReceived += 1;
      return true;
    }
  }
};
}  // namespace hypercomm

#endif
