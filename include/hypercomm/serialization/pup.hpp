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
inline size_t pup_size(serdes& s, const T& t) {
  pup(s, const_cast<T&>(t));
  return s.size();
}

template <typename T>
inline size_t size(const T& t, serdes* next = nullptr) {
  sizer s;
  auto sz = pup_size(s, t);
  if (next) {
    next->acquire(s);
  }
  return sz;
}

template <typename T, typename Enable = void>
struct quick_sizer;

template <typename T>
struct quick_sizer<T, typename std::enable_if<is_bytes<T>::value>::type> {
  inline static std::size_t impl(const T& t) { return sizeof(T); }
};

template <typename T>
inline std::size_t quick_size(const T& t) {
  return quick_sizer<T>::impl(t);
}

template <typename T, typename Enable = void>
struct puper;

template <typename T>
struct puper<T, typename std::enable_if<is_list_like<T>::value>::type> {
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
struct puper<T, typename std::enable_if<is_bytes<T>::value>::type> {
  inline static void impl(serdes& s, T& t) { s.copy(&t); }
};

template <typename T>
struct puper<temporary<T, kBuffer>> {
  inline static void impl(serdes& s, temporary<T, kBuffer>& t) { s | t.data; }
};

template <typename T>
struct puper<temporary<T, kInline>> {
  inline static void impl(serdes& s, temporary<T, kInline>& t) {
    s | t.value();
  }
};

template <typename T, std::size_t N>
struct puper<std::array<T, N>,
             typename std::enable_if<is_bytes<T>::value>::type> {
  inline static void impl(serdes& s, std::array<T, N>& t) {
    s.copy(t.data(), N);
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

// useful for finding pointer-within-pointer oddities
// template <typename T>
// struct pack_helper<std::shared_ptr<T>> {};

template <typename T, typename IdentifyFn>
inline static void pack_ptr(serdes& s, ptr_record** out, std::shared_ptr<T>& t,
                            const IdentifyFn& fn) {
  if ((bool)t) {
    auto* rec = s.get_record(t, fn);
    if (rec->is_instance()) {
      *out = rec;
    } else {
      pup(s, *rec);
    }
  } else {
    ptr_record rec(nullptr);
    pup(s, rec);
  }
}

template <typename T, typename IdentifyFn>
inline static void pack_ptr(serdes& s, std::shared_ptr<T>& t,
                            const IdentifyFn& fn) {
  ptr_record* rec = nullptr;
  pack_ptr(s, &rec, t, fn);
  if (rec != nullptr) {
    CkAssertMsg(rec->is_instance(), "expecting an instance!");
    pup(s, *rec);
    pack_helper<T>::pack(s, *t);
  }
}

template <typename T>
inline static void instance_unpack(serdes& s, std::shared_ptr<T>& t) {
  if (is_bytes<T>::value) {
    ::new (&t) std::shared_ptr<T>(std::move(s.observe_source()),
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
}

template <typename T>
inline static void default_unpack(serdes& s, ptr_record& rec,
                                  std::shared_ptr<T>& t) {
  if (rec.is_null()) {
    ::new (&t) std::shared_ptr<T>();
  } else if (rec.is_reference()) {
    ::new (&t) std::shared_ptr<T>(s.get_instance<T>(rec.id));
  } else {
    CkAbort("unknown record type %d", static_cast<int>(rec.kind));
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
      case ptr_record::DEFERRED:
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
        std::shared_ptr<polymorph> p(([&](void) {
          if (rec.is_instance()) {
            std::shared_ptr<polymorph> p(hypercomm::instantiate(rec.ty));
            auto ins = s.put_instance(rec.id, p);
            CkAssertMsg(ins, "instance insertion did not occur!");
            p->__pup__(s);
            return std::move(p);
          } else if (rec.is_reference()) {
            return s.get_instance<polymorph>(rec.id);
          } else {
            CkAbort("unknown record type %d", static_cast<int>(rec.kind));
          }
        })());
        ::new (&t)
            std::shared_ptr<T>(std::dynamic_pointer_cast<T>(std::move(p)));
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

template <typename T, typename Enable = void>
struct zero_copy_fallback {
  inline static void unpack(serdes& s, std::shared_ptr<T>& t) {
    instance_unpack(s, t);
  }

  inline static void pack(serdes& s, std::shared_ptr<T>& t) {
    pack_helper<T>::pack(s, *t);
  }
};

template <typename T>
struct puper<
    std::shared_ptr<T>,
    typename std::enable_if<hypercomm::is_zero_copyable<T>::value>::type> {
  inline static void impl(serdes& s, std::shared_ptr<T>& t) {
    if (s.unpacking()) {
      // unpack the record
      ptr_record rec;
      pup(s, rec);
      // if it was deferred
      if (rec.is_deferred()) {
        // record a reference to this pointer so it
        // can be updated later on
        s.put_deferred(rec, t);
      } else if (rec.is_instance()) {
        // if it's an instance, go through the fallback routine
        zero_copy_fallback<T>::unpack(s, t);
        CkEnforceMsg(s.put_instance(rec.id, t),
                     "instance insertion did not occur!");
      } else {
        // if not, simply unpack the pointer
        default_unpack(s, rec, t);
      }
    } else {
      ptr_record* rec = nullptr;
      pack_ptr(s, &rec, t, []() { return 0; });
      if (rec != nullptr) {
        CkAssertMsg(rec->is_instance(), "expecting an instance!");
        // use (quick size) to determine the size of the object
        // without running through another serdes cycle
        std::size_t size = t ? quick_size(*t) : 0;
        // if we can defer it (and it's worthwhile to do so)
        if (s.deferrable && size >= kZeroCopySize) {
          // update the kind/size of the record
          rec->ty = size;
          rec->kind = ptr_record::DEFERRED;
          pup(s, *rec);
          // and defer it
          s.put_deferred(*rec, t);
        } else {
          pup(s, *rec);
          zero_copy_fallback<T>::pack(s, t);
        }
      }
    }
  }
};

template <typename T>
struct puper<
    std::shared_ptr<T>,
    typename std::enable_if<!hypercomm::is_idiosyncratic_ptr<T>::value>::type> {
  inline static void impl(serdes& s, std::shared_ptr<T>& t) {
    if (s.unpacking()) {
      ptr_record rec;
      pup(s, rec);
      if (rec.is_instance()) {
        instance_unpack(s, t);
        CkEnforceMsg(s.put_instance(rec.id, t),
                     "instance insertion did not occur!");
      } else {
        default_unpack(s, rec, t);
      }
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

template <typename Key, typename T, typename Hash, typename KeyEqual>
struct puper<hash_map<Key, T, Hash, KeyEqual>> {
  using map_type = hash_map<Key, T, Hash, KeyEqual>;

  inline static void impl(serdes& s, map_type& t) {
    if (s.unpacking()) {
      std::size_t size;
      s.copy(&size);
      ::new (&t) map_type(size);
      for (auto i = 0; i < size; i++) {
        std::pair<Key, T> pair;
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
