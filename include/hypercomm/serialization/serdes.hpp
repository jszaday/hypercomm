#ifndef __HYPERCOMM_SERDES_HPP__
#define __HYPERCOMM_SERDES_HPP__

#include <algorithm>
#include <map>
#include <memory>

namespace hypercomm {

namespace detail {
template <typename T>
bool is_uninitialized(std::weak_ptr<T> const& weak) {
  using wt = std::weak_ptr<T>;
  return !weak.owner_before(wt{}) && !wt{}.owner_before(weak);
}
}  // namespace detail

using ptr_id_t = std::size_t;
using polymorph_id_t = std::size_t;
struct ptr_record;

class sizer;
class packer;
class unpacker;

// records whether a ptr-type is a back-reference or an instance
struct ptr_record {
  enum kind_t : std::uint8_t {
    INVALID,
    IGNORED,
    DEFERRED,
    REFERENCE,
    INSTANCE
  };

  static constexpr auto instance_size =
      sizeof(kind_t) + sizeof(ptr_id_t) + sizeof(polymorph_id_t);

  kind_t kind;
  ptr_id_t id;
  polymorph_id_t ty;

  ptr_record(const ptr_record& other)
      : kind(other.kind), id(other.id), ty(other.ty) {}
  ptr_record(const kind_t& _ = INVALID) : kind(_) {}
  ptr_record(std::nullptr_t) : ptr_record(IGNORED) {}
  ptr_record(const ptr_id_t& _1) : kind(REFERENCE), id(_1) {}
  ptr_record(const ptr_id_t& _1, const polymorph_id_t& _2)
      : kind(INSTANCE), id(_1), ty(_2) {}

  inline bool is_null() const { return kind == IGNORED; }
  inline bool is_instance() const { return kind == INSTANCE; }
  inline bool is_reference() const { return kind == REFERENCE; }
  inline bool is_deferred() const { return kind == DEFERRED; }
};

struct generic_ptr_ {
  virtual void reset(const std::shared_ptr<void>&) = 0;
  virtual std::shared_ptr<void> lock(void) = 0;
};

template <typename T>
struct generic_ptr_impl_ : public generic_ptr_ {
  std::shared_ptr<T>& ptr;
  generic_ptr_impl_(std::shared_ptr<T>& _) : ptr(_) {}

  virtual void reset(const std::shared_ptr<void>& _) override {
    auto typed = std::static_pointer_cast<T>(_);
    new (&this->ptr) std::shared_ptr<T>(std::move(typed));
  }

  virtual std::shared_ptr<void> lock(void) override {
    CkAssertMsg(this->ptr, "pointer died before pup!");
    return std::static_pointer_cast<void>(this->ptr);
  }
};

struct deferred_ {
  std::size_t size;
  std::vector<std::unique_ptr<generic_ptr_>> ptrs;

  template <typename T>
  inline void push_back(std::shared_ptr<T>& _) {
    ptrs.emplace_back(new generic_ptr_impl_<T>(_));
  }

  inline void reset(std::shared_ptr<void>&& _) {
    CkAssertMsg(!this->ptrs.empty(), "expected to reset at least one value!");
    for (auto& ptr : this->ptrs) {
      ptr->reset(_);
    }
  }
};

class serdes {
  template <typename K, typename V>
  using owner_less_map = std::map<K, V, std::owner_less<K>>;

  std::map<ptr_id_t, deferred_> deferred;
  std::map<ptr_id_t, std::weak_ptr<void>> instances;

  std::weak_ptr<void> source;

 public:
  owner_less_map<std::weak_ptr<void>, ptr_record> records;

  enum state_t { SIZING, PACKING, UNPACKING };
  const char* start;
  char* current;
  const state_t state;
  const bool deferrable;

  serdes(const bool& _0, const std::shared_ptr<void>& _1, const char* _2)
      : deferrable(_0),
        source(_1),
        start(_2),
        current(const_cast<char*>(_2)),
        state(UNPACKING) {}

  serdes(const bool& _0, const char* _1, const state_t& _2)
      : deferrable(_0), start(_1), current(const_cast<char*>(_1)), state(_2) {}

  serdes(const bool& _0, const char* _1) : serdes(_0, _1, PACKING) {}

  serdes(const bool& _0) : serdes(_0, nullptr, SIZING) {}

 public:
  friend class sizer;
  friend class packer;
  friend class unpacker;

  serdes(serdes&&) = delete;
  serdes(const serdes&) = delete;

  inline std::shared_ptr<void> observe_source(void) const {
    if (detail::is_uninitialized(this->source)) {
      return nullptr;
    } else {
      return this->source.lock();
    }
  }

  template <typename T>
  inline void reset_source(const std::shared_ptr<T>& t) {
    this->source = t;
  }

  template <typename T>
  inline void put_deferred(const ptr_record& rec, std::shared_ptr<T>& t) {
    CkAssertMsg(this->deferrable, "unexpected deferred values!");
    auto& def = this->deferred[rec.id];
    def.size = (std::size_t)rec.ty;
    def.push_back(t);
  }

  using deferred_type =
      std::tuple<ptr_id_t, std::size_t, std::shared_ptr<void>>;
  inline void get_deferred(std::vector<deferred_type>& vect) {
    using value_type = decltype(this->deferred)::value_type;

    std::transform(std::begin(this->deferred), std::end(this->deferred),
                   std::back_inserter(vect),
                   [](const value_type& pair) -> deferred_type {
                     auto& inst = pair.second;
                     return std::make_tuple(pair.first, inst.size,
                                            inst.ptrs.front()->lock());
                   });

#if CMK_ERROR_CHECKING
    auto sorted =
        std::is_sorted(std::begin(vect), std::end(vect),
                       [](const deferred_type& lhs, const deferred_type& rhs) {
                         return std::get<0>(lhs) - std::get<1>(rhs);
                       });

    CkAssertMsg(sorted, "resulting vector was unsorted!");
#endif
  }

  inline std::size_t n_deferred(void) const { return this->deferred.size(); }

  inline void reset_deferred(const std::size_t& idx,
                             std::shared_ptr<void>&& value) {
    auto it = std::begin(this->deferred);
    std::advance(it, idx);
    (it->second).reset(std::move(value));
  }

  inline bool packing() const { return state == state_t::PACKING; }
  inline bool unpacking() const { return state == state_t::UNPACKING; }
  inline bool sizing() const { return state == state_t::SIZING; }

  inline std::size_t size() { return current - start; }

  template <typename T>
  inline void copy(T* data, std::size_t n = 1) {
    const auto nBytes = n * sizeof(T);
    // TODO test whether we need: CK_ALIGN(nBytes, sizeof(T));
    const auto nAlign = 0;
    switch (state) {
      case PACKING:
        std::copy(reinterpret_cast<char*>(data),
                  reinterpret_cast<char*>(data) + nBytes, current + nAlign);
        break;
      case UNPACKING:
        std::copy(current + nAlign, current + nAlign + nBytes,
                  reinterpret_cast<char*>(data));
        break;
    }
    advanceBytes(nAlign + nBytes);
  }

  inline void advanceBytes(std::size_t size) { current += size; }

  template <typename T>
  inline std::shared_ptr<T> get_instance(const ptr_id_t& id) const {
    auto search = this->instances.find(id);
    if (search != std::end(this->instances)) {
      return std::move(std::static_pointer_cast<T>(search->second.lock()));
    } else {
      return {};
    }
  }

  template <typename T>
  inline bool put_instance(const ptr_id_t& id, const std::shared_ptr<T>& ptr) {
    auto weak = std::weak_ptr<void>(std::static_pointer_cast<void>(ptr));
    return this->put_instance(id, std::move(weak));
  }

  inline bool put_instance(const ptr_id_t& id, std::weak_ptr<void>&& ptr) {
    CkAssertMsg(this->unpacking(),
                "cannot put instance into a non-unpacking serdes");
    auto ins = this->instances.emplace(id, std::move(ptr));
    return ins.second;
  }

  template <typename T>
  inline void advance(std::size_t n = 1) {
    current += sizeof(T) * n;
  }
};

class sizer : public serdes {
 public:
  sizer(const bool& deferrable = false) : serdes(deferrable) {}
};

class packer : public serdes {
 public:
  packer(const char* _1, const bool& deferrable = false)
      : serdes(deferrable, _1) {}
};

class unpacker : public serdes {
 public:
  unpacker(const std::shared_ptr<void>& _1, const char* _2,
           const bool& deferrable = false)
      : serdes(deferrable, _1, _2) {}
};

}  // namespace hypercomm

#endif
