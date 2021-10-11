#ifndef __HYPERCOMM_MESSAGING_DESTINATION_HPP__
#define __HYPERCOMM_MESSAGING_DESTINATION_HPP__

#include "../core/common.hpp"
#include "../core/module.hpp"

namespace hypercomm {

class endpoint {
  inline static const int& demux(void) {
    return CkIndex_locality_base_::idx_demux_CkMessage();
  }

 public:
  const int idx_;
  const entry_port_ptr port_;

  endpoint(PUP::reconstruct) : endpoint(0x0) {}
  endpoint(const int& _) : idx_(_), port_(nullptr) {}
  endpoint(const entry_port_ptr& _) : idx_(demux()), port_(_) {}
  endpoint(std::tuple<int, const entry_port_ptr&>&& pair)
      : idx_(std::get<0>(pair)), port_(std::get<1>(pair)) {}

  inline bool valid(void) const {
    return this->port_ || (this->idx_ != demux());
  }

  inline value_handler_fn_ get_handler(void) const {
    return CkIndex_locality_base_::get_value_handler(this->idx_);
  }

  inline bool operator==(const endpoint& other) const {
    return (this->idx_ == other.idx_) &&
           comparable_comparator<entry_port_ptr>()(this->port_, other.port_);
  }

  inline hash_code hash(void) const {
    return hash_combine(utilities::hash<int>()(this->idx_),
                        utilities::hash<entry_port_ptr>()(this->port_));
  }

  inline void pup(serdes& s) {
    s | const_cast<int&>(this->idx_);
    s | const_cast<entry_port_ptr&>(this->port_);
  }

  template <typename T>
  static constexpr bool constructible_from(void) {
    return std::is_constructible<endpoint, const T&>::value;
  }
};

struct endpoint_source : public virtual value_source,
                         public std::enable_shared_from_this<endpoint_source> {
  endpoint ep_;

  template <typename... Args>
  endpoint_source(Args... args) : ep_(std::forward<Args>(args)...) {}

  virtual void take_back(value_ptr&&) override;
};

struct endpoint_hasher {
  inline hash_code operator()(const endpoint& ep) const { return ep.hash(); }
};

template <typename T>
using is_valid_endpoint_t =
    typename std::enable_if<endpoint::constructible_from<T>()>::type;

template <typename T>
using endpoint_map = std::unordered_map<endpoint, T, endpoint_hasher>;

class destination {
  union u_options {
    callback_ptr cb;
    component_port_t port;
    ~u_options() {}
    u_options(void) {}
    u_options(const callback_ptr& x) : cb(x) {}
    u_options(const component_port_t& x) : port(x) {}
    u_options(const component::id_t& com_, const component::port_type& port_)
        : port(com_, port_) {}
  } options;

 public:
  enum type_ : uint8_t { kCallback, kComponentPort };

  const type_ type;

  ~destination() {
    switch (type) {
      case kCallback: {
        this->options.cb.~callback_ptr();
        break;
      }
      case kComponentPort: {
        this->options.port.~component_port_t();
        break;
      }
    }
  }

  destination(const callback_ptr& cb) : type(kCallback), options(cb) {}

  destination(const component_id_t& com, const component::port_type& port)
      : type(kComponentPort), options(com, port) {}

  destination(const component_port_t& port)
      : type(kComponentPort), options(port) {}

  destination(const destination& dst) : type(dst.type) {
    switch (type) {
      case kCallback: {
        ::new (&this->options) u_options(dst.cb());
        break;
      }
      case kComponentPort: {
        ::new (&this->options) u_options(dst.port());
        break;
      }
    }
  }

  const callback_ptr& cb(void) const {
    CkAssert(this->type == kCallback);
    return this->options.cb;
  }

  const component_port_t& port(void) const {
    CkAssert(this->type == kComponentPort);
    return this->options.port;
  }
};
}  // namespace hypercomm

#endif
