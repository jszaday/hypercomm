#ifndef __HYPERCOMM_SECTIONS_IMPRINTABLE_HPP__
#define __HYPERCOMM_SECTIONS_IMPRINTABLE_HPP__

#include "../core/common.hpp"

namespace hypercomm {

class imprintable_base_ : public virtual comparable {
 public:
  virtual bool is_member(const CkArrayIndex&) const = 0;
  virtual const CkArrayIndex* pick_root(const CkArrayID&) const = 0;
};

/* imprintables define a list of members for collective
 * operations. they can be "imprinted" onto a chare to
 * generate an "identity", and can also select roots for
 * out-of-tree collective operations (namely, b/c's).
 */
template <typename Index>
class imprintable : public polymorph, public imprintable_base_ {
 public:
  using identity_ptr = std::shared_ptr<identity<Index>>;
  using locality_ptr = indexed_locality_<Index>*;

  friend class indexed_locality_<Index>;

  // determine whether an index is a member of this imprintable
  // TODO ( expand to consider collective proxies as well )
  virtual bool is_member(const Index&) const = 0;

  virtual bool is_member(const CkArrayIndex& idx) const {
    return this->is_member(reinterpret_index<Index>(idx));
  }

 protected:
  // apply this imprintable to a locality (generating an identity)
  virtual identity_ptr imprint(const locality_ptr&,
                               const reduction_id_t&) const = 0;
};
}  // namespace hypercomm

#endif
