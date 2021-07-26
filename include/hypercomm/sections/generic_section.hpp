#ifndef __HYPERCOMM_SECTIONS_GENERIC_HPP__
#define __HYPERCOMM_SECTIONS_GENERIC_HPP__

#include "section.hpp"

#include <cstdint>
#include <vector>
#include <type_traits>

namespace hypercomm {

// TODO fix the ordinal to be something more like (typename std::vector<Index>::size_type)
template <typename Index>
struct vector_section : public section<std::int64_t, Index>, public polymorph {
  using indices_type = std::vector<Index>;
  using store_type = typename std::aligned_storage<sizeof(indices_type), alignof(indices_type)>::type;

  const indices_type& index_vector_;
  store_type index_store_;

  vector_section(const indices_type& _1): index_vector_(_1) {}

  vector_section(PUP::reconstruct): index_vector_(*reinterpret_cast<indices_type*>(&index_store_)) {
    // TODO evaluate whether this is necessary
    new (const_cast<indices_type*>(&index_vector_)) indices_type();
  }

  vector_section(indices_type&& _1): vector_section<Index>(PUP::reconstruct{}) {
    new (const_cast<indices_type*>(&index_vector_)) indices_type(std::move(_1));
  }

  vector_section(const vector_section<Index>& _1): vector_section<Index>(PUP::reconstruct{}) {
    new (const_cast<indices_type*>(&index_vector_)) indices_type(_1.members());
  }

  virtual const indices_type& members(void) const override {
    return this->index_vector_;
  }

  virtual bool equals(const std::shared_ptr<comparable>& _1) const override {
    const auto& other = std::dynamic_pointer_cast<vector_section<Index>>(_1);
    return other && this->members() == other->members();
  }

  virtual void __pup__(serdes& s) override {
    s | const_cast<indices_type&>(index_vector_);
  }

  virtual typename section<std::int64_t, Index>::section_ptr clone(void) const override {
    return std::make_shared<vector_section<Index>>(*this);
  }
};

template <typename Index>
using generic_section = vector_section<Index>;

template <typename T>
std::shared_ptr<vector_section<T>> sectionify(std::vector<T>&& data) {
  return std::make_shared<vector_section<T>>(std::move(data));
}

}

#endif
