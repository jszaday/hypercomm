
#ifndef __HYPERCOMM_REDUCTIONS_REDN_PORT_HPP__
#define __HYPERCOMM_REDUCTIONS_REDN_PORT_HPP__

#include "reducer.hpp"

namespace hypercomm {

template <typename Index>
class reduction_port : public entry_port {
  using stamp_type = reducer::stamp_type;
  using first_type = typename std::tuple_element<0, stamp_type>::type;
  using second_type = typename std::tuple_element<1, stamp_type>::type;

 public:
  hash_code identity;  // TODO use something better here
  second_type count;
  Index index;

  reduction_port(PUP::reconstruct) {}

  reduction_port(const stamp_type& _1, const Index& _2)
      : identity(utilities::hash<first_type>()(std::get<0>(_1))),
        count(std::get<1>(_1)),
        index(_2) {}

  bool affected_by(const stamp_type& stamp) const {
    return (this->count <= std::get<1>(stamp)) &&
           (this->identity ==
            utilities::hash<first_type>()(std::get<0>(stamp)));
  }

  virtual bool equals(const std::shared_ptr<comparable>& other) const override {
    auto theirs = std::dynamic_pointer_cast<reduction_port<Index>>(other);
    return theirs && this->identity == theirs->identity &&
           this->count == theirs->count && this->index == theirs->index;
  }

  virtual std::string to_string(void) const override {
    std::stringstream ss;
    ss << "reduction_port(count=" << this->count << ",idx=" << this->index
       << ")";
    return ss.str();
  }

  virtual hash_code hash(void) const override {
    auto hashId = utilities::hash<second_type>()(this->count);
    auto hashIdx = utilities::hash<Index>()(this->index);
    return hash_combine(this->identity, hash_combine(hashId, hashIdx));
  }

  virtual void __pup__(serdes& s) override {
    s | identity;
    s | count;
    s | index;
  }
};
}

#endif
