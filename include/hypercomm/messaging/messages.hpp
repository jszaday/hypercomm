#ifndef __HYPERCOMM_MESSAGING_ENVELOPE_HPP__
#define __HYPERCOMM_MESSAGING_ENVELOPE_HPP__

#include <charm++.h>

namespace hypercomm {
namespace detail {
struct converse_message {
  char core[CmiReservedHeaderSize];

  converse_message(CmiUInt2 hdl = 0) {
    std::fill(this->core, this->core + CmiReservedHeaderSize, '\0');

    this->set_handler(hdl);
  }

  void set_handler(CmiUInt2 hdl) {
    CmiSetHandler(static_cast<void*>(this), hdl);
  }

  void operator delete(void* blk) { CmiFree(blk); }

  void* operator new(std::size_t count) { return CmiAlloc(count); }
};

struct array_message : public detail::converse_message {
  CkArrayID aid;
  CkArrayIndex idx;

  array_message(CmiUInt2 hdl = 0) : converse_message(hdl) {}

  array_message(const CkArrayID& aid_, const CkArrayIndex& idx_,
                CmiUInt2 hdl = 0)
      : converse_message(hdl), aid(aid_), idx(idx_) {}

  void set_destination(const CkArrayID& aid, const CkArrayIndex& idx) {
    this->aid = aid;
    this->idx = idx;
  }
};
}  // namespace detail
}  // namespace hypercomm

#endif
