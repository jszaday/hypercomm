#ifndef __HYPERCOMM_REDUCTIONS_CONTRIBUTION__
#define __HYPERCOMM_REDUCTIONS_CONTRIBUTION__

#include "../sections/imprintable.hpp"

namespace hypercomm {
template<typename T>
class reducer;

template<typename T>
class contribution {
 private:
  typed_value_ptr<T> value_;
  combiner_ptr<T> combiner_;
  callback_ptr<T> callback_;

 public:
  template<typename A>
  friend class reducer;

  template<typename A>
  friend class puper;

  contribution(const tags::reconstruct &) {}

  contribution(typed_value_ptr<T> &&_1, const combiner_ptr<T> &_2, const callback_ptr<T> &_3)
      : value_(std::move(_1)), combiner_(_2), callback_(_3) {}
};

// template <typename T>
// struct puper<contribution<T>> {
//   inline static void impl(serdes &s, contribution<T> &t) {
//     // s | t.msg_;
//     s | t.combiner_;
//     s | t.callback_;
//   }
// };
}  // namespace hypercomm

#endif
