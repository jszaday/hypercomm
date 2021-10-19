#ifndef __HYPERCOMM_MESSAGING_ENDPOINT_HPP__
#define __HYPERCOMM_MESSAGING_ENDPOINT_HPP__

#include "../core/common.hpp"
#include "../core/module.hpp"
#include "../core/entry_port.hpp"

namespace hypercomm {
class endpoint {
  inline static const int& demux(void) {
    return CkIndex_locality_base_::idx_demux_CkMessage();
  }

 public:
  entry_port_ptr port_;
  int idx_;

  endpoint(PUP::reconstruct) : endpoint(0x0) {}
  endpoint(void) : endpoint(PUP::reconstruct()) {}
  endpoint(const int& _) : idx_(_), port_(nullptr) {}
  endpoint(entry_port_ptr&& _) : idx_(demux()), port_(_) {}
  endpoint(const entry_port_ptr& _) : idx_(demux()), port_(_) {}
  endpoint(int idx, const entry_port_ptr& port) : idx_(idx), port_(port) {}
  endpoint(std::tuple<int, const entry_port_ptr&>&& pair)
      : endpoint(std::get<0>(pair), std::get<1>(pair)) {}

  endpoint(endpoint&& other)
      : idx_(other.idx_), port_(std::move(other.port_)) {}

  endpoint(const endpoint& other) : idx_(other.idx_), port_(other.port_) {}

  endpoint(CkMessage* msg) {
    CkAssertMsg(msg, "cannot 'learn' from null msg");
    auto* env = UsrToEnv(msg);
    this->idx_ = env->getEpIdx();
    if (env->getMsgIdx() == message::index()) {
      this->port_ = std::move(((message*)msg)->dst);
    }
  }

  inline void export_to(CkMessage* msg) {
    auto* env = UsrToEnv(msg);
    env->setEpIdx(this->idx_);
    if (env->getMsgIdx() == message::index()) {
      ((message*)msg)->dst = std::move(this->port_);
    }
  }

  inline endpoint& operator=(endpoint&& other) {
    if (this != &other) {
      this->port_ = std::move(other.port_);
      this->idx_ = std::move(other.idx_);
    }
    return *this;
  }

  inline endpoint& operator=(const endpoint& other) {
    if (this != &other) {
      this->port_ = other.port_;
      this->idx_ = other.idx_;
    }
    return *this;
  }

  inline operator bool(void) const {
    return (this->idx_ != 0x0) && (!this->is_demux() || this->port_);
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

  template <typename T>
  static constexpr bool constructible_from(void) {
    return std::is_constructible<endpoint, const T&>::value;
  }

  inline bool is_demux(void) const { return (this->idx_ == demux()); }

  std::string to_string(void) const {
    std::stringstream ss;
    ss << "endpoint(";
    if (!this->is_demux()) {
      ss << "epIdx=" << this->idx_ << ",";
    }
    if (this->port_) {
      ss << this->port_->to_string();
    } else {
      ss << "NULL";
    }
    ss << ")";
    return ss.str();
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
}  // namespace hypercomm

#endif
