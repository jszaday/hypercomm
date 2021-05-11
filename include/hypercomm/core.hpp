#ifndef __HYPERCOMM_CORE_HPP__
#define __HYPERCOMM_CORE_HPP__

#include <charm++.h>

#include <memory>

namespace hypercomm {
struct callback {
  using value_type = std::shared_ptr<CkMessage>;

  virtual void send(value_type&&) = 0;
};

struct ignore_callback : public callback {
  virtual void send(callback::value_type&&) override {}
};

using callback_ptr = std::shared_ptr<callback>;
}

#endif
