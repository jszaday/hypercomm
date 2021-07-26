#ifndef __HYCOMM_UTILS_HASH_HPP__
#define __HYCOMM_UTILS_HASH_HPP__

#include "charm++.h"

#include "../core/comparable.hpp"

namespace hypercomm {
namespace utilities {

template <bool B>
using Requires = PUP::Requires<B>;

template <class T>
struct hash<
    T, typename std::enable_if<std::is_base_of<hashable, T>::value>::type> {
  std::size_t operator()(const T& t) const { return const_cast<T&>(t).hash(); }
};

template <class T>
struct hash<
    T*, typename std::enable_if<std::is_base_of<hashable, T>::value>::type> {
  using type = T*;
  std::size_t operator()(const type& t) const { return t ? t->hash() : 0; }
};

template <class T>
struct hash<
    T*, typename std::enable_if<!std::is_base_of<hashable, T>::value>::type> {
  std::size_t operator()(const T* t) const { return t ? hash<T>()(*t) : 0; }
};

template <class T>
struct hash<
    T, typename std::enable_if<std::is_integral<T>::value ||
                               std::is_floating_point<T>::value ||
                               std::is_same<std::string, T>::value>::type> {
  std::size_t operator()(const T& t) const { return std::hash<T>()(t); }
};

template <class T>
struct hash<std::shared_ptr<T>> {
  std::size_t operator()(const std::shared_ptr<T>& t) const {
    return hash<T*>()(t.get());
  }
};

template <class T>
struct hash<std::vector<T>> {
  std::size_t operator()(const std::vector<T>& t) const {
    return hash_iterable(t);
  }
};

template <class T>
struct hash<std::deque<T>> {
  std::size_t operator()(const std::deque<T>& t) const {
    return hash_iterable(t);
  }
};

template <>
struct hash<CkChareID> {
  std::size_t operator()(const CkChareID& t) const {
    auto seed = std::hash<void*>()(t.objPtr);
    seed = hash_combine(seed, std::hash<int>()(t.onPE));
    return seed;
  }
};

template <>
struct hash<CkGroupID> {
  std::size_t operator()(const CkGroupID& t) const {
    return std::hash<int>()(t.idx);
  }
};

template <>
struct hash<CkArrayIndex> {
  std::size_t operator()(const CkArrayIndex& t) const {
    const auto* data = t.data();
    auto seed = std::hash<short unsigned int>()(t.dimension);
    for (auto i = 0; i < CK_ARRAYINDEX_MAXLEN; i += 1) {
      seed = hash_combine(seed, std::hash<int>()(data[i]));
    }
    return seed;
  }
};

template <typename T>
struct hash<T, typename std::enable_if<std::is_base_of<
                   CProxyElement_ArrayElement, T>::value>::type> {
  std::size_t operator()(const T& t) const {
    auto seed = hash<CkGroupID>()((CkGroupID)t.ckGetArrayID());
    seed = hash_combine(seed, hash<CkArrayIndex>()(t.ckGetIndex()));
    return seed;
  }
};

template <typename T>
struct hash<
    T, typename std::enable_if<
           std::is_base_of<CProxy_ArrayBase, T>::value &&
           !(std::is_base_of<CProxyElement_ArrayElement, T>::value)>::type> {
  std::size_t operator()(const T& t) const {
    return hash<CkGroupID>()((CkGroupID)t.ckGetArrayID());
  }
};

template <typename T>
struct hash<
    T, typename std::enable_if<std::is_base_of<CProxy_Chare, T>::value>::type> {
  std::size_t operator()(const T& t) const {
    return hash<CkChareID>()(t.ckGetChareID());
  }
};

template <typename T>
struct hash<T, typename std::enable_if<
                   std::is_base_of<CProxyElement_Group, T>::value ||
                   std::is_base_of<CProxyElement_NodeGroup, T>::value>::type> {
  std::size_t operator()(const T& t) const {
    auto seed = hash<CkGroupID>()(t.ckGetGroupID());
    seed = hash_combine(seed, std::hash<int>()(t.ckGetGroupPe()));
    return seed;
  }
};

template <typename T>
struct hash<T,
            typename std::enable_if<
                (std::is_base_of<CProxy_Group, T>::value ||
                 std::is_base_of<CProxy_NodeGroup, T>::value) &&
                !(std::is_base_of<CProxyElement_Group, T>::value ||
                  std::is_base_of<CProxyElement_NodeGroup, T>::value)>::type> {
  std::size_t operator()(const T& t) const {
    return hash<CkGroupID>()(t.ckGetGroupID());
  }
};

template <size_t N = 0, typename... Args,
          Requires<0 == sizeof...(Args)> = nullptr>
void hash_tuple_impl(std::size_t& /* p */, const std::tuple<Args...>& /* t */) {
}

template <size_t N = 0, typename... Args,
          Requires<(0 < sizeof...(Args) && 0 == N)> = nullptr>
void hash_tuple_impl(std::size_t& p, const std::tuple<Args...>& t) {
  const auto& value = std::get<N>(t);
  p = hash_combine(p,
                   hash<typename std::decay<decltype(value)>::type>()(value));
}

template <size_t N, typename... Args,
          Requires<(sizeof...(Args) > 0 && N > 0)> = nullptr>
void hash_tuple_impl(std::size_t& p, const std::tuple<Args...>& t) {
  const auto& value = std::get<N>(t);
  p = hash_combine(p,
                   hash<typename std::decay<decltype(value)>::type>()(value));
  hash_tuple_impl<N - 1>(p, t);
}

template <typename... Args>
struct hash<std::tuple<Args...>> {
  std::size_t operator()(const std::tuple<Args...>& t) const {
    size_t seed = 0;
    hash_tuple_impl<sizeof...(Args)-1>(seed, t);
    return seed;
  }
};
}
}

#endif
