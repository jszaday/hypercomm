#ifndef __HYPERCOMM_PROXY_HPP__
#define __HYPERCOMM_PROXY_HPP__

#include <charm++.h>
#include <ck.h>

#include <memory>
#include <utility>

#include "../messaging/messaging.hpp"
#include "../utilities.hpp"

namespace hypercomm {
using chare_t = ChareType;

struct proxy /* : virtual public hashable */ {
 public:
  inline bool node_level(void) const;

  virtual void* local(void) const = 0;
  virtual chare_t type(void) const = 0;
  virtual bool collective(void) const = 0;
  virtual std::string to_string(void) const = 0;

  virtual hash_code hash(void) const = 0;
  virtual const CProxy& c_proxy(void) const = 0;

  virtual bool equals(const hypercomm::proxy& other) const = 0;
};

template <typename Base>
class typed_proxy : virtual public proxy {
 public:
  Base proxy_;

  typed_proxy(void) {}
  typed_proxy(const Base& _1) : proxy_(_1) {}

  // TODO elevate id up to this level

  virtual hash_code hash(void) const override {
    return utilities::hash<Base>()(this->proxy_);
  }

  virtual const CProxy& c_proxy(void) const override { return this->proxy_; }
};

struct located_chare : virtual public proxy {
  virtual int home(void) const = 0;
  virtual int last_known(void) const = 0;

  inline std::pair<int, bool> path(void) const {
    auto home = this->home();
    auto last = this->last_known();
    auto node = this->node_level();
    auto mine = node ? CkMyNode() : CkMyPe();
    auto dst = (home == mine) ? last : home;
    return std::make_pair(dst, node);
  }
};

template <typename T>
constexpr bool is_array_proxy(void) {
  return std::is_base_of<CProxy_ArrayBase, T>::value;
}

template <typename T>
constexpr bool is_array_element(void) {
  return std::is_base_of<ArrayElement, T>::value;
}

template <typename T, typename Enable = void>
struct index_for;

template <typename T>
struct index_for<T, typename std::enable_if<is_array_proxy<T>() ||
                                            is_array_element<T>()>::type> {
  using type = CkArrayIndex;
};

template <typename T, typename Enable = void>
struct identifier_for;

template <typename T>
struct identifier_for<T, typename std::enable_if<is_array_proxy<T>()>::type> {
  using type = CkArrayID;
};

template <typename T, typename Enable = void>
struct chare_type_for;

template <typename T>
struct chare_type_for<T, typename std::enable_if<is_array_proxy<T>()>::type> {
  static constexpr auto value = chare_t::TypeArray;
};

template <typename Index>
struct collective_proxy;

struct generic_element_proxy : virtual public located_chare {
  virtual bool collective(void) const override { return false; }

  virtual void receive(message* msg) = 0;
};

template <typename Index>
struct element_proxy : virtual public generic_element_proxy {
  virtual Index index() const = 0;

  virtual std::shared_ptr<collective_proxy<Index>> collection(void) const = 0;

  virtual void receive(message*) override;
};

template <typename Index>
struct collective_proxy : virtual public proxy {
  using index_type = Index;
  using element_type = std::shared_ptr<element_proxy<index_type>>;

  virtual element_type operator[](const index_type& idx) const = 0;
  virtual bool collective(void) const override { return false; }
};

template <typename Proxy>
struct generic_collective_proxy
    : public typed_proxy<Proxy>,
      public collective_proxy<typename index_for<Proxy>::type> {
  using base_type = collective_proxy<typename index_for<Proxy>::type>;
  using element_type = typename base_type::element_type;
  using index_type = typename base_type::index_type;
  using proxy_type = Proxy;
  using identifier_type = typename identifier_for<Proxy>::type;

  generic_collective_proxy(const Proxy& _1) : typed_proxy<Proxy>(_1) {}

  template <PUP::Requires<is_array_proxy<Proxy>()> = nullptr>
  identifier_type id(void) const {
    return this->proxy_.ckGetArrayID();
  }

  virtual element_type operator[](const index_type& idx) const;

  virtual chare_t type(void) const override {
    return chare_type_for<Proxy>::value;
  }

  virtual bool equals(const hypercomm::proxy& _1) const override {
    const auto* other =
        dynamic_cast<const generic_collective_proxy<Proxy>*>(&_1);
    return other && (const_cast<proxy_type&>(this->proxy_) == other->proxy_);
  }

  virtual void* local(void) const override {
    return this->proxy_.ckLocalBranch();
  }

  virtual std::string to_string(void) const override {
    std::stringstream ss;
    ss << "collective(" << typeid(Proxy).name() << ")";
    return ss.str();
  }
};

using array_proxy = generic_collective_proxy<CProxy_ArrayBase>;

struct non_migratable_proxy : virtual public located_chare {
  virtual int last_known(void) const override { return this->home(); }
};

struct chare_proxy : public typed_proxy<CProxy_Chare>,
                     public non_migratable_proxy {
  using proxy_type = CProxy_Chare;

  chare_proxy(void) = default;
  chare_proxy(const proxy_type& _1) : typed_proxy<CProxy_Chare>(_1) {}

  virtual bool equals(const hypercomm::proxy& _1) const override {
    const auto* _2 = dynamic_cast<const chare_proxy*>(&_1);

    if (_2) {
      const auto& ours = this->id();
      const auto& theirs = _2->id();
      return (ours.onPE == theirs.onPE) && (ours.objPtr == theirs.objPtr);
    } else {
      return false;
    }
  }

  inline const CkChareID& id(void) const { return proxy_.ckGetChareID(); }

  virtual chare_t type(void) const override { return chare_t::TypeChare; }

  virtual int home(void) const override { return this->id().onPE; }

  virtual bool collective(void) const override { return false; }

  virtual void* local(void) const override {
    auto& id = this->id();
    if (id.onPE == CkMyPe()) {
      auto* objs = &(CkpvAccess(chare_objs));
      if (reinterpret_cast<std::size_t>(id.objPtr) >= objs->size()) {
        return id.objPtr;
      } else {
        return CkLocalChare(&id);
      }
    } else {
      return nullptr;
    }
  }

  virtual std::string to_string(void) const override {
    std::stringstream ss;
    const auto& ourId = this->id();
    ss << "chare(pe=" << ourId.onPE << ",obj=" << ourId.objPtr << ")";
    return ss.str();
  }
};

struct array_element_proxy : public element_proxy<CkArrayIndex>,
                             public typed_proxy<CProxyElement_ArrayElement> {
  using proxy_type = CProxyElement_ArrayElement;

  array_element_proxy(void) = default;
  array_element_proxy(const proxy_type& _1)
      : typed_proxy<CProxyElement_ArrayElement>(_1) {}

  virtual bool equals(const hypercomm::proxy& _1) const override {
    const auto* other = dynamic_cast<const array_element_proxy*>(&_1);
    auto result = other && other->id() == this->id();
    if (!result) return false;
    const auto& ourIdx = this->proxy_.ckGetIndex();
    const auto& theirIdx = other->proxy_.ckGetIndex();
    const auto* ourData = ourIdx.data();
    const auto* theirData = theirIdx.data();
    // TODO determine when/if this is ever necessary
    // result = result && ourIdx.dimension == theirIdx.dimension;
    for (auto i = 0; result && i < CK_ARRAYINDEX_MAXLEN; i += 1) {
      result = result && (ourData[i] == theirData[i]);
    }
    return result;
  }

  virtual std::shared_ptr<collective_proxy<CkArrayIndex>> collection(
      void) const override {
    return std::make_shared<array_proxy>(CProxy_ArrayBase(this->id()));
  }

  inline CkArrayID id(void) const { return proxy_.ckGetArrayID(); }
  virtual CkArrayIndex index() const override { return proxy_.ckGetIndex(); }

  virtual chare_t type(void) const override { return chare_t::TypeArray; }

  virtual int home(void) const override {
    return this->id().ckLocalBranch()->homePe(this->index());
  }

  virtual int last_known(void) const override {
    return this->id().ckLocalBranch()->lastKnown(this->index());
  }

  virtual void* local(void) const override {
    return this->id().ckLocalBranch()->lookup(this->index());
  }

  virtual std::string to_string(void) const override {
    std::stringstream ss;
    const auto& id = ((CkGroupID)this->id()).idx;
    const auto& idx = this->index();
    ss << "array(id=" << id << ", idx=" << utilities::idx2str(idx) << ")";
    return ss.str();
  }
};

template <typename T>
struct grouplike_element_proxy : public element_proxy<int>,
                                 public typed_proxy<T>,
                                 public non_migratable_proxy {
  using proxy_type = T;

  static constexpr auto is_node =
      std::is_same<CProxyElement_NodeGroup, proxy_type>::value;

  grouplike_element_proxy(void) = default;
  grouplike_element_proxy(const proxy_type& _1) : typed_proxy<T>(_1) {}

  virtual std::shared_ptr<collective_proxy<int>> collection(
      void) const override {
    throw std::runtime_error("not yet implemented!");
  }

  virtual bool equals(const hypercomm::proxy& _1) const override {
    const auto* other = dynamic_cast<const grouplike_element_proxy<T>*>(&_1);
    return other && (const_cast<proxy_type&>(this->proxy_) == other->proxy_);
  }

  inline CkGroupID id(void) const { return this->proxy_.ckGetGroupID(); }

  virtual int index(void) const override { return this->proxy_.ckGetGroupPe(); }

  virtual chare_t type(void) const override {
    return (is_node) ? (chare_t::TypeNodeGroup) : (chare_t::TypeGroup);
  }

  virtual int home(void) const override { return this->index(); }

  virtual void* local(void) const override {
    if (is_node) {
      return (this->home() == CkMyNode()) ? CkLocalNodeBranch(this->id())
                                          : nullptr;
    } else {
      return (this->home() == CkMyPe()) ? CkLocalBranch(this->id()) : nullptr;
    }
  }

  virtual std::string to_string(void) const override {
    std::stringstream ss;
    ss << (is_node ? "nodegroup" : "group");
    ss << "(pe=" << this->index() << ",id=" << this->id().idx << ")";
    return ss.str();
  }
};

using group_element_proxy = grouplike_element_proxy<CProxyElement_Group>;

using nodegroup_element_proxy =
    grouplike_element_proxy<CProxyElement_NodeGroup>;

inline bool proxy::node_level(void) const {
  return dynamic_cast<const nodegroup_element_proxy*>(this) != nullptr;
}

inline std::shared_ptr<chare_proxy> make_proxy(
    const chare_proxy::proxy_type& base) {
  return std::make_shared<chare_proxy>(base);
}

template <typename T, PUP::Requires<std::is_base_of<
                          array_element_proxy::proxy_type, T>::value> = nullptr>
inline std::shared_ptr<array_element_proxy> make_proxy(const T& base) {
  return std::make_shared<array_element_proxy>(
      static_cast<const array_element_proxy::proxy_type&>(base));
}

template <typename T,
          PUP::Requires<std::is_base_of<array_proxy::proxy_type, T>::value &&
                        !std::is_base_of<array_element_proxy::proxy_type,
                                         T>::value> = nullptr>
inline std::shared_ptr<array_proxy> make_proxy(const T& base) {
  return std::make_shared<array_proxy>(
      static_cast<const array_proxy::proxy_type&>(base));
}

inline std::shared_ptr<group_element_proxy> make_proxy(
    const group_element_proxy::proxy_type& base) {
  return std::make_shared<group_element_proxy>(base);
}

inline std::shared_ptr<nodegroup_element_proxy> make_proxy(
    const nodegroup_element_proxy::proxy_type& base) {
  return std::make_shared<nodegroup_element_proxy>(base);
}

template <
    typename Proxy, typename Index,
    PUP::Requires<std::is_base_of<array_proxy::proxy_type,
                                  typename Proxy::proxy_type>::value> = nullptr>
typename Proxy::element_type element_at(const Proxy* proxy, const Index& idx) {
  using element_proxy_type = array_element_proxy::proxy_type;
  const auto& element =
      reinterpret_cast<const element_proxy_type&>(proxy->c_proxy());
  element_proxy_type element_proxy(element.ckGetArrayID(), idx);
  return make_proxy(element_proxy);
}

template <typename T>
typename generic_collective_proxy<T>::element_type
generic_collective_proxy<T>::operator[](
    const generic_collective_proxy<T>::index_type& idx) const {
  return element_at(this, idx);
}
}  // namespace hypercomm

#include "../messaging/delivery.hpp"

namespace hypercomm {

template <typename Index>
void element_proxy<Index>::receive(message* msg) {
  deliver(*this, msg);
}

}  // namespace hypercomm

#endif
