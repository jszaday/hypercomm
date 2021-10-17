#include <hypercomm/core/module.hpp>
#include <hypercomm/core/locality_map.hpp>
#include <hypercomm/core/generic_locality.hpp>
#include <hypercomm/messaging/common.hpp>

namespace hypercomm {

using value_handler_map_ = std::map<int, value_handler_fn_>;
CksvDeclare(value_handler_map_, handlers_);

void CkIndex_locality_base_::put_value_handler(const int& epIdx,
                                               const value_handler_fn_& fn) {
  auto ins = CksvAccess(handlers_).emplace(epIdx, fn);
  CkAssertMsg(ins.second, "insertion did not occur!");
}

value_handler_fn_ CkIndex_locality_base_::get_value_handler(const int& epIdx) {
  auto& handlers = CksvAccess(handlers_);
  auto search = handlers.find(epIdx);
  if (search == std::end(handlers)) {
    return nullptr;
  } else {
    return search->second;
  }
}

void call_demux_(generic_locality_* self, deliverable&& dev) {
  self->receive(std::move(dev));
}

void call_execute_(CkMessage* msg, locality_base_* obj) { obj->execute(msg); }

void call_replace_downstream_(CkMessage* msg, locality_base_* obj) {
  obj->replace_downstream(msg);
}

const int& CkIndex_locality_base_::idx_demux_CkMessage(void) {
  static int epIdx = register_value_handler<call_demux_>(
      "hypercomm::locality_base_::demux(CkMessage*)");
  return epIdx;
}

const int& CkIndex_locality_base_::idx_execute_CkMessage(void) {
  static int epIdx =
      CkRegisterEp("hypercomm::locality_base_::execute(CkMessage*)",
                   reinterpret_cast<CkCallFnPtr>(call_execute_),
                   CMessage_CkMessage::__idx, CkIndex_locality_base_::__idx, 0);
  return epIdx;
}

const int& CkIndex_locality_base_::idx_replace_downstream_CkMessage(void) {
  static int epIdx =
      CkRegisterEp("hypercomm::locality_base_::replace_downstream(CkMessage*)",
                   reinterpret_cast<CkCallFnPtr>(call_replace_downstream_),
                   CMessage_CkMessage::__idx, CkIndex_locality_base_::__idx, 0);
  return epIdx;
}

int CkIndex_locality_base_::__idx;

namespace core {
void initialize(void) {
  if (CkMyRank() == 0) {
    auto& __idx = CkIndex_locality_base_::__idx;

    __idx = CkRegisterChare("hypercomm::locality_base_", 0, TypeArray);
    CkRegisterArrayDimensions(__idx, -1);
    CkRegisterBase(__idx, CkIndex_ArrayElement::__idx);

    CkIndex_locality_base_::idx_demux_CkMessage();

    CkIndex_locality_base_::idx_execute_CkMessage();

    CkIndex_locality_base_::idx_replace_downstream_CkMessage();

    // register the locality module
    _registerlocality();
  }

  // register the messaging module
  messaging::initialize();
}
}  // namespace core
}  // namespace hypercomm

#include <hypercomm/core/locality.def.h>
