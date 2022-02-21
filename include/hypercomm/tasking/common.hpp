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

struct task_base_ {
  bool active_;
  task_kind_t kind_;
  continuation_id_t continuation_;

  task_base_(task_kind_t kind) : kind_(kind), active_(true) {}

  void pup_base_(PUP::er &p);
};

struct task_payload;

using task_creator_t = task_base_ *(*)(task_payload &&);
using task_puper_t = void (*)(PUP::er &, task_base_ *&);

struct task_record_ {
  task_creator_t creator;
  task_puper_t puper;

  task_record_(task_creator_t creator_, task_puper_t puper_)
      : creator(creator_), puper(puper_) {}
};

using task_records_t = std::vector<task_record_>;
task_records_t &get_task_records_(void);
task_creator_t get_task_creator_(task_kind_t kind);
task_puper_t get_task_puper_(task_kind_t kind);

using continuation_t = void (*)(task_base_ *, task_payload &&);
continuation_t get_continuation_(continuation_id_t id);
std::vector<continuation_t> &get_continuations_(void);
}  // namespace tasking
}  // namespace hypercomm

#endif
