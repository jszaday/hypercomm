#ifndef __HYPERCOMM_PUP_HPP__
#define __HYPERCOMM_PUP_HPP__

#include "construction.hpp"
#include "enrollment.hpp"
#include "serdes.hpp"
#include "traits.hpp"

namespace hypercomm {

template <typename T>
inline void pup(serdes& s, const T& t);

template <typename T>
inline void pup(serdes& s, T& t);

template <typename... Ts>
inline void pup(serdes& s, const std::tuple<Ts...>& t);

template <typename T>
inline serdes& operator|(serdes& s, T& t) {
  pup(s, t);
  return s;
}

template <typename T>
inline size_t size(const T& t) {
  sizer s;
  pup(s, const_cast<T&>(t));
  return s.size();
}

template <typename T>
void interpup(PUP::er& p, T& t) {
  if (typeid(p) == typeid(PUP::fromMem)) {
    auto& mem = *static_cast<PUP::fromMem*>(&p);
    unpacker s(nullptr, mem.get_current_pointer());
    pup(s, t);
    mem.advance(s.size());
  } else if (typeid(p) == typeid(PUP::toMem)) {
    auto& mem = *static_cast<PUP::toMem*>(&p);
    packer s(mem.get_current_pointer());
    pup(s, t);
    mem.advance(s.size());
  } else if (typeid(p) == typeid(PUP::sizer)) {
    p(static_cast<char*>(nullptr), size(t));
  } else {
    CkAbort("unsure how to convert an %s into a serdes", typeid(p).name());
  }
}

template <typename T, typename Enable = void>
struct puper;

template <>
struct puper<chare_t> {
  using impl_type = std::underlying_type<chare_t>::type;

  inline static void impl(serdes& s, chare_t& t) {
    auto& impl = reinterpret_cast<impl_type&>(t);
    auto smol = (std::uint8_t)impl;  // shrink the enum

    s | smol;

    if (s.unpacking()) {
      impl = (impl_type)smol;
    }
  }
};

template <typename T>
struct puper<T, typename std::enable_if<is_list_or_deque<T>::value>::type> {
  using value_type = typename T::value_type;

  inline static void impl(serdes& s, T& t) {
    if (s.unpacking()) {
      std::size_t size;
      s.copy(&size);
      ::new (&t) T();
      for (auto i = 0; i < size; i++) {
        temporary<value_type> tmp(tags::no_init{});
        pup(s, tmp);
        t.emplace_back(std::move(tmp.value()));
      }
    } else {
      auto size = t.size();
      s.copy(&size);
      for (auto& i : t) {
        pup(s, i);
      }
    }
  }
};

template <typename T>
struct puper<T, typename std::enable_if<PUP::as_bytes<T>::value>::type> {
  inline static void impl(serdes& s, T& t) { s.copy(&t); }
};

// template <typename T>
// struct puper<temporary<T, kBuffer>> {
//   inline static void impl(serdes& s, temporary<T, kBuffer>& t) { s | t.data;
//   }
// };

template <typename T>
struct puper<temporary<T, kInline>> {
  inline static void impl(serdes& s, temporary<T, kInline>& t) {
    s | t.value();
  }
};

template <typename T, std::size_t N>
struct puper<std::array<T, N>,
             typename std::enable_if<PUP::as_bytes<T>::value>::type> {
  inline static void impl(serdes& s, std::array<T, N>& t) {
    s.copy(t.data(), N);
  }
};

inline std::shared_ptr<PUP::er> make_puper(const serdes& s) {
  switch (s.state) {
    case serdes_state::UNPACKING: {
      using pup_type = typename puper_for<serdes_state::UNPACKING>::type;
      return std::make_shared<pup_type>(s.current);
    }
    case serdes_state::PACKING: {
      using pup_type = typename puper_for<serdes_state::PACKING>::type;
      return std::make_shared<pup_type>(s.current);
    }
    case serdes_state::SIZING: {
      using pup_type = typename puper_for<serdes_state::SIZING>::type;
      return std::make_shared<pup_type>();
    }
    default: {
      return {};
    }
  }
}

template <typename T>
struct puper<T, typename std::enable_if<hypercomm::built_in<T>::value>::type> {
  static constexpr auto is_proxy = std::is_base_of<CProxy, T>::value;

  inline static void undelegate(serdes& s, T& t, std::true_type) {
    if (!s.unpacking()) {
      t.ckUndelegate();
    }
  }

  inline static void undelegate(serdes& s, T& t, std::false_type) {}

  inline static void impl(serdes& s, T& t) {
    undelegate(s, t, typename std::integral_constant<bool, is_proxy>::type());
    auto p = make_puper(s);
    if (s.unpacking()) {
      reconstruct(&t);
    }
    (*p) | t;
    s.advanceBytes(p->size());
  }
};

template <typename T>
struct puper<
    std::shared_ptr<T>,
    typename std::enable_if<std::is_base_of<PUP::able, T>::value>::type> {
  inline static void impl(serdes& s, std::shared_ptr<T>& t) {
    if (s.unpacking()) {
      PUP::able* p = nullptr;
      pup<PUP::able*>(s, p);
      ::new (&t) std::shared_ptr<T>(
          std::dynamic_pointer_cast<T>(std::shared_ptr<PUP::able>(p)));
    } else {
      auto p = dynamic_cast<PUP::able*>(t.get());
      pup<PUP::able*>(s, p);
    }
  }
};

template <>
struct puper<CkCallback> {
  inline static void impl(serdes& s, CkCallback& t) {
    auto p = make_puper(s);
    *p | t;
    s.advanceBytes(p->size());
  }
};

template <typename T>
struct puper<T*, typename std::enable_if<is_message<T>::value>::type> {
  inline static void impl(serdes& s, T*& t) {
    auto p = make_puper(s);
    CkPupMessage(*p, reinterpret_cast<void**>(&t), 1);
    s.advanceBytes(p->size());
  }
};

template <typename T>
struct puper<std::shared_ptr<T>,
             typename std::enable_if<is_message<T>::value>::type> {
  inline static void impl(serdes& s, std::shared_ptr<T>& t) {
    if (s.unpacking()) {
      T* msg = nullptr;
      pup(s, msg);
      ::new (&t) std::shared_ptr<T>(static_cast<T*>(msg),
                                    [](T* msg) { CkFreeMsg(msg); });
    } else {
      T* msg = t.get();
      pup(s, msg);
    }
  }
};

namespace {

template <typename T, typename Enable = void>
struct pack_helper {
  static void pack(serdes& s, T& p) { pup(s, p); }
};

template <typename T>
struct pack_helper<T, typename std::enable_if<is_polymorph<T>::value>::type> {
  static void pack(serdes& s, T& p) { p.__pup__(s); }
};

template <typename T, typename IdentifyFn>
inline static void pack_ptr(serdes& s, std::shared_ptr<T>& t,
                            const IdentifyFn& fn) {
  if ((bool)t) {
    auto search = s.records.find(t);
    if (search == s.records.end()) {
      auto id = s.records.size() + 1;
      auto pair =
          s.records.emplace(std::piecewise_construct, std::make_tuple(t),
                            std::make_tuple(id, fn()));
      auto& rec = pair.first->second;
      pup(s, rec);
      pack_helper<T>::pack(s, *t);
    } else {
      auto& other = search->second;
      ptr_record rec(other);
      if (other.is_instance()) {
        rec.kind = ptr_record::REFERENCE;
      }
      pup(s, rec);
    }
  } else {
    ptr_record rec(nullptr);
    pup(s, rec);
  }
}
}  // namespace

template <typename T>
struct puper<T, typename std::enable_if<is_polymorph<T>::value &&
                                        !std::is_abstract<T>::value>::type> {
  inline static void impl(serdes& s, T& t) {
    if (s.unpacking()) reconstruct(&t);

    t.__pup__(s);
  }
};

template <>
struct puper<ptr_record> {
  using impl_type = typename std::underlying_type<ptr_record::kind_t>::type;

  inline static void impl(serdes& s, ptr_record& rec) {
    auto& k = *(reinterpret_cast<impl_type*>(&rec.kind));
    s | k;
    switch (k) {
      case ptr_record::REFERENCE:
        pup(s, rec.id);
        break;
      case ptr_record::INSTANCE:
        pup(s, rec.id);
        pup(s, rec.ty);
        break;
      case ptr_record::IGNORED:
        break;
      default:
        CkAbort("unknown record type %d", static_cast<int>(k));
        break;
    }
  }
};

namespace {
template <typename T>
using skip_cast =
    typename std::integral_constant<bool, is_polymorph<T>::value>::type;
}

template <typename T>
class puper<std::shared_ptr<T>,
            typename std::enable_if<is_polymorph<T>::value ||
                                    is_trait<T>::value>::type> {
  inline static std::shared_ptr<polymorph> cast_to_packable(
      std::shared_ptr<T>& t, std::false_type) {
    return std::dynamic_pointer_cast<polymorph>(t);
  }

  inline static std::shared_ptr<T> cast_to_packable(std::shared_ptr<T>& t,
                                                    std::true_type) {
    return t;
  }

 public:
  inline static void impl(serdes& s, std::shared_ptr<T>& t) {
    if (s.unpacking()) {
      ptr_record rec;
      pup(s, rec);
      if (rec.is_null()) {
        ::new (&t) std::shared_ptr<T>();
      } else {
        std::shared_ptr<polymorph> p;
        if (rec.is_instance()) {
          p.reset(hypercomm::instantiate(rec.ty));
          CkAssertMsg(s.put_instance(rec.id, p),
                      "instance insertion did not occur!");
          p->__pup__(s);
        } else if (rec.is_reference()) {
          p = s.get_instance<polymorph>(rec.id);
        } else {
          CkAbort("unknown record type %d", static_cast<int>(rec.kind));
        }
        ::new (&t) std::shared_ptr<T>(std::dynamic_pointer_cast<T>(p));
      }
    } else {
      auto p = cast_to_packable(t, skip_cast<T>());
#if CMK_ERROR_CHECKING
      if (t && (p == nullptr))
        CkAbort("could not cast %s to pup'able", typeid(t.get()).name());
#endif
      pack_ptr(s, p, [p]() { return hypercomm::identify(*p); });
    }
  }
};

template <typename T>
class puper<std::unique_ptr<T>,
            typename std::enable_if<is_polymorph<T>::value ||
                                    is_trait<T>::value>::type> {
  inline static polymorph* cast_to_packable(std::unique_ptr<T>& t,
                                            std::false_type) {
    return dynamic_cast<polymorph*>(t.get());
  }

  inline static T* cast_to_packable(std::unique_ptr<T>& t, std::true_type) {
    return t.get();
  }

 public:
  inline static void impl(serdes& s, std::unique_ptr<T>& t) {
    if (s.unpacking()) {
      polymorph_id_t tid;
      s | tid;
      auto p = hypercomm::instantiate(tid);
      p->__pup__(s);
      t.reset(dynamic_cast<T*>(p));
    } else {
      auto* p = cast_to_packable(t, skip_cast<T>());
      CkAssertMsg(p != nullptr, "expected a non-null unique ptr");
      auto tid = hypercomm::identify(*p);
      s | tid;
      p->__pup__(s);
    }
  }
};

template <typename T, typename U>
struct puper<std::pair<T, U>> {
  inline static void impl(serdes& s, std::pair<T, U>& t) {
    s | t.first;
    s | t.second;
  }
};

template <typename T>
class puper<std::unique_ptr<T>,
            typename std::enable_if<!(is_polymorph<T>::value ||
                                      is_trait<T>::value)>::type> {
 public:
  inline static void impl(serdes& s, std::unique_ptr<T>& t) {
    if (s.unpacking()) {
      new (&t) std::unique_ptr<T>();
    }

    bool is_null = (t.get() == nullptr);
    s | is_null;

    if (!is_null) {
      if (s.unpacking()) {
        t.reset((T*)(::operator new(sizeof(T))));
      }

      s | *t;
    }
  }
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

template <typename T>
struct puper<std::shared_ptr<T>,
             typename std::enable_if<!hypercomm::is_pupable<T>::value>::type> {
  inline static void unpack(serdes& s, std::shared_ptr<T>& t) {
    ptr_record rec;
    pup(s, rec);
    if (rec.is_null()) {
      ::new (&t) std::shared_ptr<T>();
    } else if (rec.is_instance()) {
      if (is_bytes<T>()) {
        ::new (&t) std::shared_ptr<T>(std::move(s.source.lock()),
                                      reinterpret_cast<T*>(s.current));

        s.advance<T>();
      } else {
        // allocate memory for the object
        auto* p = (T*)aligned_alloc(alignof(T), sizeof(T));
        // then reconstruct it
        pup(s, *p);
        // the object must be reconstructed before any shared
        // ptrs in case it's shared_from_this enabled. it will
        // result in subtle failures otherwise.
        ::new (&t) std::shared_ptr<T>(p, [](T* p) {
          p->~T();
          free(p);
        });
      }
      CkAssertMsg(s.put_instance(rec.id, t),
                  "instance insertion did not occur!");
    } else if (rec.is_reference()) {
      ::new (&t) std::shared_ptr<T>(s.get_instance<T>(rec.id));
    } else {
      CkAbort("unknown record type %d", static_cast<int>(rec.kind));
    }
  }

  inline static void impl(serdes& s, std::shared_ptr<T>& t) {
    if (s.unpacking()) {
      unpack(s, t);
    } else {
      pack_ptr(s, t, []() { return 0; });
    }
  }
};

namespace {
template <bool B>
using Requires = PUP::Requires<B>;

template <size_t N, typename... Args,
          Requires<(sizeof...(Args) > 0 && N == 0)> = nullptr>
inline void pup_tuple_impl(serdes& s, std::tuple<Args...>& t) {
  pup(s, std::get<N>(t));
}

template <size_t N, typename... Args,
          Requires<(sizeof...(Args) > 0 && N > 0)> = nullptr>
inline void pup_tuple_impl(serdes& s, std::tuple<Args...>& t) {
  pup_tuple_impl<N - 1>(s, t);
  pup(s, std::get<N>(t));
}
}  // namespace

template <typename... Ts>
struct puper<std::tuple<Ts...>,
             typename std::enable_if<(sizeof...(Ts) > 0)>::type> {
  inline static void impl(serdes& s, std::tuple<Ts...>& t) {
    pup_tuple_impl<sizeof...(Ts) - 1>(s, t);
  }
};

template <typename... Ts>
struct puper<std::tuple<Ts...>,
             typename std::enable_if<(sizeof...(Ts) == 0)>::type> {
  inline static void impl(serdes& s, std::tuple<Ts...>& t) {}
};

namespace {
template <typename T>
bool is_uninitialized(std::weak_ptr<T> const& weak) {
  using wt = std::weak_ptr<T>;
  return !weak.owner_before(wt{}) && !wt{}.owner_before(weak);
}
}  // namespace

template <typename T>
inline void pup(serdes& s, const T& t) {
  puper<T>::impl(s, const_cast<T&>(t));
}

template <typename T>
inline void pup(serdes& s, T& t) {
  puper<T>::impl(s, t);
}

template <typename... Ts>
inline void pup(serdes& s, const std::tuple<Ts...>& t) {
  puper<std::tuple<Ts...>>::impl(s, const_cast<std::tuple<Ts...>&>(t));
}

template <typename K, typename V>
struct puper<comparable_map<K, V>> {
  inline static void impl(serdes& s, comparable_map<K, V>& t) {
    if (s.unpacking()) {
      std::size_t size;
      s.copy(&size);
      ::new (&t) comparable_map<K, V>(size);
      for (auto i = 0; i < size; i++) {
        std::pair<K, V> pair;
        s | pair;
        auto ins = t.insert(std::move(pair));
        CkAssertMsg(ins.second, "insertion did not occur!");
      }
    } else {
      auto size = t.size();
      s.copy(&size);
      for (auto& pair : t) {
        s | pair;
      }
    }
  }
};
}  // namespace hypercomm

#endif
