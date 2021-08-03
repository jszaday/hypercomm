#include <hypercomm/serialization/construction.hpp>
#include <hypercomm/serialization/enrollment.hpp>

#include <charm++.h>
#include <map>

#include <hypercomm/messaging/intercept_msg.hpp>

namespace hypercomm {
using type_registry_t = std::map<std::type_index, polymorph_id_t>;
using alloc_registry_t = std::map<polymorph_id_t, allocator_fn>;

CsvDeclare(type_registry_t, type_registry_);
CsvDeclare(alloc_registry_t, alloc_registry_);

void init_polymorph_registry(void) {
  if (CkMyRank() == 0) {
    _registermessaging();
  }

  auto& idx = intercept_msg::handler();
  CkAssertMsg(idx >= 0, "could not register handler");

  CsvInitialize(type_registry_t, type_registry_);
  CsvInitialize(alloc_registry_t, alloc_registry_);
}

polymorph_id_t enroll(const std::type_index& index, const allocator_fn& alloc) {
  auto& t = CsvAccess(type_registry_);
  auto& i = CsvAccess(alloc_registry_);
  const auto id = static_cast<polymorph_id_t>(t.size());
#if CMK_ERROR_CHECKING
  auto search = t.find(index);
  if (search != t.end()) {
    CkAbort("fatal> the polymorph %s was enrolled multiple times\n",
            index.name());
  }
#endif
  t[index] = id;
  i[id] = alloc;
  return id;
}

polymorph_id_t identify(const std::type_index& index) {
  return (CsvAccess(type_registry_))[index];
}

polymorph_id_t identify(const polymorph& morph) {
  return identify(std::type_index(typeid(morph)));
}

polymorph_id_t identify(const polymorph_ptr& morph) { return identify(*morph); }

polymorph* instantiate(const polymorph_id_t& id) {
#if CMK_ERROR_CHECKING
  auto* reg = &(CsvAccess(alloc_registry_));
  auto search = reg->find(id);
  if (search == reg->end()) {
    CkAbort("fatal> could not find allocator for %lx", id);
  } else {
    return (search->second)();
  }
#else
  return ((CsvAccess(alloc_registry_))[id])();
#endif
}
}
