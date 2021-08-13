#ifndef __HYPERCOMM_UTIL_HPP__
#define __HYPERCOMM_UTIL_HPP__

#include <charm++.h>

#include <cmath>
#include <memory>
#include <type_traits>

#include "utilities/hash.hpp"

namespace hypercomm {

namespace utilities {

// source ( https://stackoverflow.com/a/28703383/1426075 )
template <typename R>
static constexpr R bitmask(const unsigned int& onecount) {
  return static_cast<R>(-(onecount != 0)) &
         (static_cast<R>(-1) >> ((sizeof(R) * (sizeof(char) * 8)) - onecount));
}

std::string idx2str(const CkArrayIndex& idx);
std::string buf2str(const char* data, const std::size_t& size);
std::string env2str(const envelope* env);

void pack_message(CkMessage*);
void unpack_message(CkMessage*);
bool is_reduction_message(CkMessage*);

template <typename T>
inline T* unwrap_message(std::shared_ptr<T>&& msg) {
  auto msg_raw = msg.get();
  if (msg.use_count() == 1) {
    ::new (&msg) std::shared_ptr<T>{};
    return (T*)msg_raw;
  } else {
    CkError("warning> forced to copy message %p!", msg_raw);
    return (T*)CkCopyMsg((void**)&msg_raw);
  }
}

template <typename T>
inline std::shared_ptr<T> wrap_message(T* msg) {
  return std::shared_ptr<T>(msg, [](T* msg) { CkFreeMsg(msg); });
}

CkMessage* copy_message(const CkMessage*);
std::shared_ptr<CkMessage> copy_message(const std::shared_ptr<CkMessage>&);

char* get_message_buffer(const CkMessage* msg);

inline char* get_message_buffer(const std::shared_ptr<CkMessage>& msg) {
  return get_message_buffer(msg.get());
}
}  // namespace utilities

template <template <typename...> class Template, typename T>
struct is_specialization_of : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template, Template<Args...> > : std::true_type {};

using dimension_type = decltype(CkArrayIndexBase::dimension);

template <class T, typename Enable = void>
struct dimensionality_of {
  static constexpr dimension_type value = static_cast<dimension_type>(1);
};

template <class T>
struct dimensionality_of<
    T,
    typename std::enable_if<is_specialization_of<std::tuple, T>::value>::type> {
  static constexpr dimension_type value =
      static_cast<dimension_type>(std::tuple_size<T>::value);
};

// TODO (offer versions for non-array index)
template <typename T>
T& reinterpret_index(CkArrayIndex& idx) {
  return *(reinterpret_cast<T*>(idx.data()));
}

template <typename T>
const T& reinterpret_index(const CkArrayIndex& idx) {
  return *(reinterpret_cast<const T*>(idx.data()));
}

template <typename Index, typename T>
inline Index conv2idx(const T& ord) {
  Index idx;
  // TODO (only enable for array index)
  idx.nInts = (dimension_type)ceil(sizeof(T) / (float)sizeof(int));
#if CMK_ERROR_CHECKING
  if (idx.nInts > CK_ARRAYINDEX_MAXLEN) {
    CkAbort(
        "max array index size exceeded, please increase CK_ARRAYINDEX_MAXLEN "
        "to %d",
        (int)idx.nInts);
  }
#endif
  idx.dimension = dimensionality_of<T>::value;
  reinterpret_index<T>(idx) = ord;
  return idx;
}
}  // namespace hypercomm

#endif
