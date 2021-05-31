#ifndef __HYPERCOMM_CORE_FUTURE_HPP__
#define __HYPERCOMM_CORE_FUTURE_HPP__

#include "../core.hpp"

namespace hypercomm {
using future_id_t = std::uint32_t;

struct future {
  std::shared_ptr<hypercomm::proxy> source;
  future_id_t id;

  inline std::string to_string(void) const {
    std::stringstream ss;
    ss << "future(id=" << id << ",src=";
    if (source)
      ss << source->to_string() << ")";
    else
      ss << "(nil))";
    return ss.str();
  }

  inline hash_code hash(void) const {
    return hash_combine(source->hash(), hash_code(id));
  }

  inline bool equals(const future& other) const {
    return this->id == other.id && this->source->equals(*other.source);
  }
};

template <>
struct puper<future> {
  inline static void impl(serdes& s, future& f) {
    s | f.source;
    s | f.id;
  }
};

struct future_port: public virtual entry_port {
  future f;

  future_port(PUP::reconstruct) {}
  future_port(const future& _1): f(_1) {}

  virtual bool keep_alive(void) const override { return false; }

  virtual bool equals(const std::shared_ptr<comparable>& _1) const override {
    auto other = std::dynamic_pointer_cast<future_port>(_1);
    return this->f.equals(other->f);
  }

  virtual hash_code hash(void) const override {
    return f.hash();
  }

  virtual void __pup__(serdes& s) override  {
    s | f;
  }
};

}

#endif
