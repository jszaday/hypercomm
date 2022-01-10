#ifndef __HYPERCOMM_PERSISTENT_PORT_HPP__
#define __HYPERCOMM_PERSISTENT_PORT_HPP__

#include "entry_port.hpp"

namespace hypercomm {

template <typename... Ts>
struct temporary_port : public entry_port {
  using tuple_type = std::tuple<Ts...>;

  tuple_type tag;

  temporary_port(tags::reconstruct) {}
  temporary_port(const std::tuple<Ts...>& ts) : tag(ts) {}
  temporary_port(Ts... ts) : tag(std::forward<Ts>(ts)...) {}

  virtual std::string to_string(void) const override {
    // TODO ( fix this )
    return "temporary_port(...)";
  }

  virtual bool keep_alive(void) const override { return false; }

  virtual bool equals(const std::shared_ptr<comparable>& other) const override {
    auto theirs = std::dynamic_pointer_cast<temporary_port<Ts...>>(other);
    return theirs && this->tag == theirs->tag;
  }

  virtual hash_code hash(void) const override {
    return utilities::hash<tuple_type>()(tag);
  }

  virtual void __pup__(serdes& s) override { s | tag; }
};
}  // namespace hypercomm

#endif
