#ifndef HYPERCOMMM_TASKING_COMMON_HPP

#include <charm++.h>

namespace hypercomm {
namespace tasking {
struct task_id {
  int host_;
  std::uint32_t task_;

  bool operator==(const task_id &other) const {
    return (this->host_ == other.host_) && (this->task_ == other.task_);
  }
};

struct task_id_hasher_ {
  std::size_t operator()(const task_id &tid) const {
    static_assert(sizeof(std::size_t) == sizeof(task_id),
                  "sizes do not match!");
    auto &cast = reinterpret_cast<const std::size_t &>(tid);
    return std::hash<std::size_t>()(cast);
  }
};

using task_kind_t = std::uint32_t;
using continuation_id_t = CMK_REFNUM_TYPE;

struct task_base_;
}  // namespace tasking
}  // namespace hypercomm

#endif
