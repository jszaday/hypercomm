
#include <hypercomm/core/generic_locality.hpp>

namespace hypercomm {
void entry_port::take_back(std::shared_ptr<hyper_value>&& value) {
  access_context_()->receive_value(this->shared_from_this(), std::move(value));
}

void entry_port::on_completion(const component&) {
  access_context_()->invalidate_port(this->shared_from_this());
}

void entry_port::on_invalidation(const component&) {
  access_context_()->invalidate_port(this->shared_from_this());
}
}