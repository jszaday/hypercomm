#ifndef __HYPERCOMM_SERDES_HPP__
#define __HYPERCOMM_SERDES_HPP__

#include <algorithm>
#include <map>
#include <memory>

namespace hypercomm {

using ptr_id_t = std::size_t;
using polymorph_id_t = std::size_t;
struct ptr_record;

class serdes {
  template <typename K, typename V>
  using owner_less_map = std::map<K, V, std::owner_less<K>>;

 public:
  enum state_t { SIZING, PACKING, UNPACKING };

  const std::weak_ptr<void> source;
  const char* start;
  char* current;
  const state_t state;

  owner_less_map<std::weak_ptr<void>, ptr_id_t> records;
  std::map<ptr_id_t, std::weak_ptr<void>> instances;

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

  template<typename T>
  inline std::shared_ptr<T> get_instance(const ptr_id_t& id) const {
    auto search = this->instances.find(id);
    if (search != std::end(this->instances)) {
      return std::move(std::static_pointer_cast<T>(search->second.lock()));
    } else {
      return {};
    }
  }

  template<typename T>
  inline bool put_instance(const ptr_id_t& id, const std::shared_ptr<T>& ptr) {
    auto weak = std::weak_ptr<void>(std::static_pointer_cast<void>(ptr));
    return this->put_instance(id, std::move(weak));
  }

  inline bool put_instance(const ptr_id_t& id, std::weak_ptr<void>&& ptr) {
    CkAssertMsg(this->unpacking(), "cannot put instance into a non-unpacking serdes");
    auto ins = this->instances.emplace(id, std::move(ptr));
    return ins.second;
  }

  template <typename T>
  inline void advance(std::size_t n = 1) {
    current += sizeof(T) * n;
  }

  inline static serdes make_unpacker(const std::shared_ptr<void>& source,
                                     const char* start) {
    serdes s{
        .source = source,
        .start = start,
        .current = const_cast<char*>(start),
        .state = UNPACKING
    };
    ::new (&s.instances) std::map<ptr_id_t, std::weak_ptr<void>>();
    return std::move(s);
  }

  inline static serdes make_packer(const char* start) {
    return serdes{
        .source = {},
        .start = start,
        .current = const_cast<char*>(start),
        .state = PACKING,
    };
  }

  inline static serdes make_sizer() {
    return serdes{
        .source = {},
        .start = nullptr,
        .current = nullptr,
        .state = SIZING,
    };
  }
};

// records whether a ptr-type is a back-reference or an instance
struct ptr_record {
  union data_t {
    struct s_reference {
      ptr_id_t id;
    } reference;
    struct s_instance {
      ptr_id_t id;
      polymorph_id_t ty;
    } instance;
  };

  enum type_t : std::uint8_t { UNKNOWN, IGNORE, REFERENCE, INSTANCE };

  data_t d;
  type_t t;

  ptr_record() : t(UNKNOWN) {}
  ptr_record(std::nullptr_t) : t(IGNORE) {}
  ptr_record(const ptr_id_t& id) : t(REFERENCE) { d.reference.id = id; }
  ptr_record(const ptr_id_t& id, const polymorph_id_t& ty) : t(INSTANCE) {
    d.instance.id = id;
    d.instance.ty = ty;
  }

  inline bool is_null() const { return t == IGNORE; }
  inline bool is_instance() const { return t == INSTANCE; }
  inline bool is_reference() const { return t == REFERENCE; }
};

}  // namespace hypercomm

#endif
