#ifndef __HYPERCOMM_SPECIAL_HPP__
#define __HYPERCOMM_SPECIAL_HPP__

#include "compatibility.hpp"
#include "../core/proxy.hpp"

namespace hypercomm {
template <class T>
struct is_polymorphic<T, typename std::enable_if<std::is_base_of<
                             hypercomm::proxy, T>::value>::type> {
  enum { value = true };
};

template <typename T>
class puper<std::shared_ptr<T>, typename std::enable_if<std::is_base_of<
                                    hypercomm::proxy, T>::value>::type> {
  template <typename A>
  inline static void helper(serdes& s, std::shared_ptr<T>& t) {
    if (s.unpacking()) {
      ::new (&t) std::shared_ptr<proxy>(new A());
    }

    auto& proxy = dynamic_cast<A*>(t.get())->proxy_;
    s | proxy;
  }

 public:
  static void impl(serdes& s, std::shared_ptr<T>& t) {
    chare_t ty = s.unpacking() ? (chare_t::TypeInvalid) : t->type();
    s | ty;
    if (ty == chare_t::TypeChare || ty == chare_t::TypeMainChare) {
      helper<chare_proxy>(s, t);
    } else if (ty == chare_t::TypeArray || ty == chare_t::TypeGroup ||
               ty == chare_t::TypeNodeGroup) {
      uint8_t collective = s.unpacking() ? false : t->collective();
      s | collective;
      if (collective) {
        CkAbort("collectives currently unsupported");
      }
      switch (ty) {
        case chare_t::TypeArray: {
          helper<array_element_proxy>(s, t);
          break;
        }
        case chare_t::TypeGroup: {
          helper<group_element_proxy>(s, t);
          break;
        }
        case chare_t::TypeNodeGroup: {
          helper<nodegroup_element_proxy>(s, t);
          break;
        }
        default: {
          CkAbort("unreachable");
        }
      }
    } else {
      CkAbort("invalid chare type %d", static_cast<int>(ty));
    }
  }
};
}  // namespace hypercomm

#endif
