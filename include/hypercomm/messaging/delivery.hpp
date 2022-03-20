#ifndef __HYPERCOMM_MESSAGING_DELIVERY_HPP__
#define __HYPERCOMM_MESSAGING_DELIVERY_HPP__

#include "destination.hpp"
#include "deliverable.hpp"
#include "messages.hpp"

namespace hypercomm {
struct delivery : public detail::array_message {
  deliverable payload;

  delivery(const CkArrayID& _1, const CkArrayIndex& _2, deliverable&& _3)
      : array_message(_1, _2, handler()), payload(std::move(_3)) {
    CkAssert((bool)this->payload);
  }

  static void process(ArrayElement*, deliverable&&, bool);

  static const int& handler(void);

 private:
  static void handler_(void*);
};
}  // namespace hypercomm

#endif
