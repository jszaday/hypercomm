#ifndef __HYPERCOMM_REDUCTIONS_COMMON_HPP__
#define __HYPERCOMM_REDUCTIONS_COMMON_HPP__

#include "../sections/imprintable.hpp"

namespace hypercomm {
using imprintable_ptr = std::shared_ptr<imprintable_base_>;
using stamp_type = comparable_map<imprintable_ptr, reduction_id_t>;
}  // namespace hypercomm

#endif
