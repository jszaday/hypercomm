#ifndef __HYPERCOMM_REDUCTIONS_CONTRIBUTION__
#define __HYPERCOMM_REDUCTIONS_CONTRIBUTION__

#include "../sections/imprintable.hpp"

namespace hypercomm {
class reducer;

class contribution {
 private:
  CkMessage *msg_;
  combiner_ptr combiner_;
  callback_ptr callback_;

 public:
  friend class reducer;
  friend class puper<contribution>;

  contribution(const tags::reconstruct &) {}

  contribution(value_ptr &&_1, const combiner_ptr &_2, const callback_ptr &_3)
      : msg_(_1->release()), combiner_(_2), callback_(_3) {}
};

template <>
struct puper<contribution> {
  inline static void impl(serdes &s, contribution &t) {
    s | t.msg_;
    s | t.combiner_;
    s | t.callback_;
  }
};
}  // namespace hypercomm

#endif
