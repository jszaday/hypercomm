#include <charm++.h>

#include "../reductions/reducer.hpp"

namespace hypercomm {
class locality_base_ : public ArrayElement {
 public:
  using base_index_type = CkArrayIndex;
  using stamp_type = typename reducer::stamp_type;

  virtual void execute(CkMessage* msg) = 0;
  virtual void replace_downstream(CkMessage* msg) { NOT_IMPLEMENTED; }
  virtual stamp_type __stamp__(const CkArrayIndex* = nullptr) const = 0;

  inline const CkArrayIndex& __base_index__(void) const {
    return this->ckGetArrayIndex();
  }

  inline collective_ptr<CkArrayIndex> __proxy__(void) const {
    return std::make_shared<array_proxy>(
        CProxy_ArrayBase(this->ckGetArrayID()));
  }

  inline element_ptr<CkArrayIndex> __element__(void) const {
    return std::make_shared<array_element_proxy>(CProxyElement_ArrayElement(
        this->ckGetArrayID(), this->ckGetArrayIndex()));
  }
};

namespace detail {
template <typename Target, typename Enable = void>
struct extract_proxy;

template <typename Proxy>
struct extract_proxy<CBaseT1<locality_base_, Proxy>> {
  using type = Proxy;
};

template <typename Target>
using extract_proxy_t = typename extract_proxy<Target>::type;

template <typename Base, typename Index>
struct base_ : public indexed_locality_<Index>, virtual CBase {
  using CProxy_Derived = detail::extract_proxy_t<Base>;
  using Parent = indexed_locality_<Index>;

  template <typename... Args>
  base_(Args&&... args) : Parent(std::forward<Args>(args)...) {
    thisProxy = this;
  }

  void parent_pup(PUP::er& p) {
    recursive_pup<Parent>(this, p);
    p | thisProxy;
  }

  CBASE_MEMBERS { recursive_pup<local_t>(dynamic_cast<local_t*>(this), p); }
};
}  // namespace detail
}  // namespace hypercomm
