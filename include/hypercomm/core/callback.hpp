#ifndef __HYPERCOMM_CORE_CALLBACK_HPP__
#define __HYPERCOMM_CORE_CALLBACK_HPP__

#include "../serialization/polymorph.hpp"

namespace hypercomm {
namespace core {
struct callback: public polymorph {
  using value_type = std::shared_ptr<CkMessage>;

  virtual void send(value_type&&) = 0;
};
}
}

#endif
