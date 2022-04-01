#ifndef HYPERCOMM_TAGS_HPP
#define HYPERCOMM_TAGS_HPP

#include <pup.h>
#include <memory>

namespace hypercomm {
namespace tags {
using reconstruct = PUP::reconstruct;

struct no_init {};

struct allocate {};

template <typename T>
struct use_buffer {
  std::shared_ptr<T> buffer;

  template <typename... Args>
  use_buffer(Args&&... args) : buffer(std::forward<Args>(args)...) {}
};
}  // namespace tags
}  // namespace hypercomm

#endif
