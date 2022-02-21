#include <hypercomm/tasking/workgroup.hpp>

namespace hypercomm {
namespace tasking {
void task_base_::pup_base_(PUP::er &p) {
  CkAssert(active_);
  p | this->continuation_;
}

task_records_t &get_task_records_(void) {
  static task_records_t records;
  return records;
}

task_creator_t get_task_creator_(task_kind_t kind) {
  auto &records = get_task_records_();
  return records[kind].creator;
}

task_puper_t get_task_puper_(task_kind_t kind) {
  auto &records = get_task_records_();
  return records[kind].puper;
}

continuation_t get_continuation_(continuation_id_t id) {
  auto &continuations = get_continuations_();
  return continuations[id];
}

std::vector<continuation_t> &get_continuations_(void) {
  static std::vector<continuation_t> continuations;
  return continuations;
}

void workgroup::create(task_message *msg) {
  auto creator = get_task_creator_(msg->kind);
  auto tid = msg->tid;
  // record the active task
  this->active_ = tid;
  // then construct the task
  auto *task = creator(task_payload(msg));
  // only active tasks get maintained!
  if (task->active_) {
    auto ins = this->tasks_.emplace(tid, task);
    CkAssertMsg(ins.second, "task insertion unsucessful!");
    this->try_continue_(ins.first);
  }
}

void workgroup::try_continue_(const workgroup::task_iterator &it) {
  auto &task = it->second;
  auto &tid = this->active_;
  auto &map = this->buffers_[tid];
  // check whether there are messages for this continuation
  auto search = map.find(task->continuation_);
  if ((search == std::end(map)) || search->second.empty()) {
    return;
  } else {
    bool active;
    auto &buffer = search->second;
    // if so -- try delivering it!
    if (this->resume(tid, buffer.front(), &active)) {
      // pop it when we succeed
      buffer.pop_front();
      // keep going when the task is still alive
      if (active) {
        this->try_continue_(it);
      }
    }
  }
}

void workgroup::resume(task_message *msg) {
  task_payload payload(msg);
  if (!this->resume(msg->tid, payload)) {
    this->buffer_(msg->tid, std::move(payload));
  }
}

void workgroup::resume(CkReductionMsg *msg) {
  int nReductions;
  CkReduction::tupleElement *results;
  msg->toTuple(&results, &nReductions);
  CkAssert(nReductions == 2);
  task_payload payload(msg, &results[1]);
  auto &elt = results[0];
  CkAssert(elt.dataSize == sizeof(task_id));
  auto &tid = *((task_id *)elt.data);
  if (!this->resume(tid, payload)) {
    this->buffer_(tid, std::move(payload));
  }
}

bool workgroup::resume(task_id tid, task_payload &payload, bool *active) {
  auto search = this->tasks_.find(tid);
  if (search == std::end(this->tasks_)) {
    return false;
  } else {
    auto &task = search->second;
    auto &id = task->continuation_;
    // reject out of order messages
    if (id != payload.id()) {
      return false;
    }
    // set the active task
    this->active_ = tid;
    // invoke the continuation
    auto continuation = get_continuation_(id);
    continuation(task.get(), std::move(payload));
    auto keep = task->active_;
    // discard task if its inactive
    if (!keep) {
      this->tasks_.erase(search);
    }
    // check whether anyone's observing us
    if (active == nullptr) {
      if (keep) {
        // if not, and we kept the task, move on
        this->try_continue_(search);
      }
    } else {
      // otherwise, inform them of the outcome
      *active = keep;
    }
    return true;
  }
}

void workgroup::buffer_(task_id tid, task_payload &&payload) {
  this->buffers_[tid][payload.id()].emplace_back(std::move(payload));
}

void workgroup::pup(PUP::er &p) {
  p | this->last_task_;
  p | this->tasks_;
  p | this->buffers_;
}
}  // namespace tasking
}  // namespace hypercomm
