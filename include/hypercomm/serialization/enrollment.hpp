#ifndef __HYPERCOMM_ENROLLMENT_HPP__
#define __HYPERCOMM_ENROLLMENT_HPP__

#include <typeindex>
#include <functional>

#include "polymorph.hpp"

namespace hypercomm {

using allocator_fn = std::function<polymorph_ptr(void)>;

// this should be called on all PEs that need to de/serialize data
void init_polymorph_registry(void);

// enrollment should also occur on all PEs as well
polymorph_id_t enroll(const std::type_index& index, const allocator_fn& alloc);

template <typename T>
inline polymorph_id_t enroll(void) {
  return enroll(std::type_index(typeid(T)),
                []() { return std::make_shared<T>(PUP::reconstruct{}); });
}

polymorph_id_t identify(const std::type_index&);
polymorph_id_t identify(const polymorph&);
polymorph_id_t identify(const polymorph_ptr&);

polymorph_ptr instantiate(const polymorph_id_t&);

}

#endif
