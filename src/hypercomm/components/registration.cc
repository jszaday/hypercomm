#include <hypercomm/utilities.hpp>
#include <hypercomm/components.hpp>

namespace hypercomm {
namespace components {

using manager_t = manager::manager_t;
CkpvDeclare(uint32_t, counter_);
CkpvExtern(manager_t, manager_);
CkpvDeclare(int, value_handler_idx_);
CkpvDeclare(int, connection_handler_idx_);
CkpvDeclare(int, invalidation_handler_idx_);

void connection_handler_(char* msg) {
  std::tuple<placeholder, component::id_t> tup;
  PUP::fromMem p(msg + CmiMsgHeaderSizeBytes);
  p | tup;
  (CkpvAccess(manager_))->recv_connection(std::get<0>(tup), std::get<1>(tup));
  CmiFree(msg);
}

extern std::tuple<component::id_t&, component::id_t&> get_from_to(envelope* env);

void invalidation_handler_(envelope* env) {
  auto tup = get_from_to(env);
  CkpvAccess(manager_)->recv_invalidation(std::get<0>(tup), std::get<1>(tup));
  CkFreeMsg(EnvToUsr(env));
}

void value_handler_(envelope* env) {
  auto tup = get_from_to(env);
  auto msg = utilities::wrap_message((CkMessage*)EnvToUsr(env));
  utilities::unpack_message(msg.get());
  CkpvAccess(manager_)
      ->recv_value(std::get<0>(tup), std::get<1>(tup), std::move(msg));
}

void initialize_module(void) {
  CkpvInitialize(uint32_t, counter_);
  CkpvAccess(counter_) = 0;
  CkpvInitialize(manager_t, manager_);
  (CkpvAccess(manager_)).reset(new manager());
  CkpvInitialize(int, value_handler_idx_);
  CkpvAccess(value_handler_idx_) =
      CmiRegisterHandler((CmiHandler)value_handler_);
  CkpvInitialize(int, invalidation_handler_idx_);
  CkpvAccess(invalidation_handler_idx_) =
      CmiRegisterHandler((CmiHandler)invalidation_handler_);
  CkpvInitialize(int, connection_handler_idx_);
  CkpvAccess(connection_handler_idx_) =
      CmiRegisterHandler((CmiHandler)connection_handler_);
}

}
}
