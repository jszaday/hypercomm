#ifndef __HYPERCOMM_ENROLLMENT_HPP__
#define __HYPERCOMM_ENROLLMENT_HPP__

#include <functional>
#include <typeindex>

#include "../utilities.hpp"
#include "polymorph.hpp"

namespace hypercomm {

using allocator_fn = std::function<polymorph*(void)>;

//  this should be called on all PEs that need to de/serialize data
// (although, in reality, it does more registration than that!)
void init_polymorph_registry(void);

// enrollment is node-level, and should only occur on rank 0
polymorph_id_t enroll(const std::type_index& index, const allocator_fn& alloc);

template <typename T>
inline polymorph_id_t enroll(void) {
  //  assumes the type is reconstructible using tags::reconstruct
  // (looking forward, this could be alloc -> hypercomm::reconstruct)
  return enroll(std::type_index(typeid(T)),
                []() -> polymorph* { return new T(tags::reconstruct{}); });
}

// determine the id for various permutations of polymorph ids
polymorph_id_t identify(const std::type_index&);
polymorph_id_t identify(const polymorph&);
polymorph_id_t identify(const polymorph_ptr&);

// instantiate a polymorph of a given id
polymorph* instantiate(const polymorph_id_t&);

//  hash the polymorph's id to get a unique hash for the type
// (that will be the consistent between nodes)
template <typename T>
hash_code hash_type(void) {
  utilities::hash<polymorph_id_t> hasher;
  return hasher(identify(typeid(T)));
}

}  // namespace hypercomm

#endif
