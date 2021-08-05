#ifndef __HYPERCOMM_POLYMORPH_HPP__
#define __HYPERCOMM_POLYMORPH_HPP__

#include "serdes.hpp"

namespace hypercomm {

struct virtual_enable_shared_from_this_base
    : std::enable_shared_from_this<virtual_enable_shared_from_this_base> {
  virtual ~virtual_enable_shared_from_this_base() {}
};

template <typename T>
struct virtual_enable_shared_from_this
    : virtual virtual_enable_shared_from_this_base {
  std::shared_ptr<T> shared_from_this() {
    return std::dynamic_pointer_cast<T>(
        virtual_enable_shared_from_this_base::shared_from_this());
  }
};

struct polymorph : virtual virtual_enable_shared_from_this_base {
  struct trait {};

  virtual ~polymorph() = default;
  virtual void __pup__(serdes& s) = 0;
};

using polymorph_ptr = std::shared_ptr<polymorph>;
}  // namespace hypercomm

#endif
