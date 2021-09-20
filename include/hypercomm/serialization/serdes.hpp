#ifndef __HYPERCOMM_SERDES_HPP__
#define __HYPERCOMM_SERDES_HPP__

#include <algorithm>
#include <map>
#include <memory>

namespace hypercomm {

using ptr_id_t = std::size_t;
using polymorph_id_t = std::size_t;
struct ptr_record;

class sizer;
class packer;
class unpacker;

struct generic_ptr_ {
  virtual void reset(const std::shared_ptr<void>&) = 0;
};

template <typename T>
struct generic_ptr_impl_ : public generic_ptr_ {
  std::shared_ptr<T>& ptr;
  generic_ptr_impl_(std::shared_ptr<T>& _) : ptr(_) {}
  virtual void reset(const std::shared_ptr<void>& _) override {
    auto typed = std::static_pointer_cast<T>(_);
    new (&this->ptr) std::shared_ptr<T>(std::move(typed));
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
    for (auto& ptr : this->ptrs) {
      ptr->reset(_);
    }
  }
};

// records whether a ptr-type is a back-reference or an instance
struct ptr_record {
  enum type_t : std::uint8_t { UNKNOWN, IGNORE, REFERENCE, INSTANCE, DEFERRED };

  type_t ty;
  ptr_id_t id;
  std::size_t size;

  ptr_record(void) : ty(UNKNOWN) {}

  ptr_record(std::nullptr_t) : ty(IGNORE) {}

  ptr_record(const type_t& _1, const ptr_id_t& _2, const std::size_t& _3 = 0x0)
      : ty(_1), id(_2), size(_3) {}

  inline static ptr_record ref(const ptr_record& rec) {
    return ptr_record((rec.ty == INSTANCE) ? REFERENCE : rec.ty, rec.id,
                      rec.size);
  }

  inline bool is_null() const { return ty == IGNORE; }
  inline bool is_instance() const { return ty == INSTANCE; }
  inline bool is_reference() const { return ty == REFERENCE; }
  inline bool is_deferred() const { return ty == DEFERRED; }
};

class serdes {
  template <typename K, typename V>
  using owner_less_map = std::map<K, V, std::owner_less<K>>;

  std::map<ptr_id_t, deferred_> deferred;
  std::map<ptr_id_t, std::weak_ptr<void>> instances;

 public:
  owner_less_map<std::weak_ptr<void>, ptr_record> records;

  enum state_t { SIZING, PACKING, UNPACKING };

  bool deferrable = false;
  const std::weak_ptr<void> source;
  const char* start;
  char* current;
  const state_t state;

  serdes(const std::shared_ptr<void>& _1, const char* _2)
      : source(_1),
        start(_2),
        current(const_cast<char*>(_2)),
        state(UNPACKING) {}

  serdes(const char* _1, const state_t& _2)
      : start(_1), current(const_cast<char*>(_1)), state(_2) {}

  serdes(const char* _1) : serdes(_1, PACKING) {}

  serdes(void) : serdes(nullptr, SIZING) {}

 public:
  friend class sizer;
  friend class packer;
  friend class unpacker;

  serdes(serdes&&) = delete;
  serdes(const serdes&) = delete;

  template <typename T>
  inline void put_deferred(const ptr_record& rec, const std::shared_ptr<T>& t) {
    auto& def = this->deferred[rec.id];
    def.size = rec.size;
    def.push_back(t);
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
  sizer(void) = default;
};

class packer : public serdes {
 public:
  packer(const char* _1) : serdes(_1) {}
};

class unpacker : public serdes {
 public:
  unpacker(const std::shared_ptr<void>& _1, const char* _2) : serdes(_1, _2) {}
};

}  // namespace hypercomm

#endif
