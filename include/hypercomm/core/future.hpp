#ifndef __HYPERCOMM_CORE_FUTURE_HPP__
#define __HYPERCOMM_CORE_FUTURE_HPP__

#include "../core/math.hpp"
#include "../core/entry_port.hpp"
#include "../serialization/pup.hpp"
#include "module.hpp"

namespace hypercomm {

struct future {
  CProxyElement_locality_base_ source;
  future_id_t id;

  inline std::string to_string(void) const {
    std::stringstream ss;
    ss << "future(id=" << id << ",src=";
    if (source) {
      ss << this->source.operator std::string() << ")";
    } else {
      ss << "(nil))";
    }
    return ss.str();
  }

  inline void set(deliverable&& dev) { send2future(*this, std::move(dev)); }

  inline bool ready(void) const;

  inline hash_code hash(void) const {
    return hash_combine(source.hash(), hash_code(id));
  }

  inline bool equals(const future& other) const {
    return (this->id == other.id) && (this->source == other.source);
  }
};

template <>
struct puper<future> {
  inline static void impl(serdes& s, future& f) {
    s | f.source;
    s | f.id;
  }
};

struct future_port : public entry_port {
  future f;

  future_port(PUP::reconstruct) {}
  future_port(const future& _) : f(std::forward<const future&>(_)) {}

  virtual bool keep_alive(void) const override { return false; }

  virtual bool equals(const std::shared_ptr<comparable>& _1) const override {
    auto other = std::dynamic_pointer_cast<future_port>(_1);
    return other && this->f.equals(other->f);
  }

  virtual hash_code hash(void) const override { return f.hash(); }

  virtual void __pup__(serdes& s) override { s | f; }

  virtual std::string to_string(void) const override {
    std::stringstream ss;
    ss << "port2" << f.to_string();
    return ss.str();
  }
};

}  // namespace hypercomm

#endif
