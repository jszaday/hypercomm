#ifndef __HYPERCOMM_COMPONENTS_COMPONENT_HPP__
#define __HYPERCOMM_COMPONENTS_COMPONENT_HPP__

#include <ck.h>
#include "base.hpp"
#include "outbox.hpp"
#include "../core/typed_value.hpp"

namespace hypercomm {

template <>
struct wrap_<std::tuple<deliverable>, typed_value_ptr> {
  using type = std::tuple<deliverable>;
};

template <typename T, typename Enable = void>
struct dev_conv_;

template <>
struct dev_conv_<deliverable> {
  static deliverable convert(deliverable&& dev) { return std::move(dev); }
};

template <typename T>
struct dev_conv_<typed_value_ptr<T>> {
  static typed_value_ptr<T> convert(deliverable&& dev) {
    return dev2typed<T>(std::move(dev));
  }
};

template <typename T>
struct invoker_ {
  inline static void invoke(const T& t) { t(); }
};

template <>
struct invoker_<std::nullptr_t> {
  inline static void invoke(const std::nullptr_t&) {}
};

namespace components {
template <typename Inputs, typename Outputs>
class component : public base_ {
 public:
  using in_type = typename tuplify_<Inputs>::type;
  using out_type = typename tuplify_<Outputs, true>::type;

  using in_set = typename wrap_<in_type, typed_value_ptr>::type;
  using out_set = typename wrap_<out_type, typed_value_ptr>::type;

  template <std::size_t I>
  using in_elt_t = typename std::tuple_element<I, in_set>::type;

  template <std::size_t O>
  using out_elt_t = typename std::tuple_element<O, out_type>::type;

 private:
  static constexpr std::size_t n_inputs_ = std::tuple_size<in_type>::value;
  static constexpr std::size_t n_outputs_ = std::tuple_size<out_type>::value;
  static_assert(n_inputs_ >= 1, "expected at least one input!");

 protected:
  using outgoing_type = outbox_<out_set>;
  using incoming_type = std::list<in_set>;
  using incoming_iterator = typename incoming_type::iterator;

  incoming_type incoming_;
  outgoing_type outgoing_;

 public:
  using accept_array_t = std::array<accept_fn_t, n_inputs_>;
  static accept_array_t acceptors;
#if HYPERCOMM_ERROR_CHECKING
  using in_type_array_t = std::array<std::type_index, n_inputs_>;
  static in_type_array_t in_types;
#endif

  component(std::size_t id_)
      : base_(id_, n_inputs_, n_outputs_, acceptors.data(), in_types.data()) {}

  virtual ~component() { this->empty_buffers_(); }

  template <std::size_t I>
  inline static void accept(component<Inputs, Outputs>* self,
                            in_elt_t<I>&& val) {
    if (!val) {
      self->on_invalidation_<I>();
    } else if (n_inputs_ == 1) {
      // bypass seeking a partial set for single-input coms
      direct_stage<I>(self, std::move(val));
    } else {
      // look for a set missing this value
      auto search = self->find_gap_<I>();
      const auto gapless = search == std::end(self->incoming_);
      // if we couldn't find one:
      if (gapless) {
        // create a new set
        self->incoming_.emplace_front();
        // then update the search iterator
        search = self->incoming_.begin();
      }
      // update the value in the set
      std::get<I>(*search) = std::move(val);
      // check whether it's ready, and stage it if so!
      if (!gapless && is_ready(*search)) {
        self->stage_action(search);
      }
    }
  }

  template <std::size_t I>
  static void accept(base_* base, deliverable&& dev) {
    auto* self = static_cast<component<Inputs, Outputs>*>(base);
    accept<I>(self, dev_conv_<in_elt_t<I>>::convert(std::move(dev)));
  }

  template <std::size_t O, std::size_t I, typename Input_, typename Output_>
  void output_to(const component<Input_, Output_>& peer) {
    static_assert(
        std::is_same<out_elt_t<O>,
                     typename component<Input_, Output_>::in_elt_t<I>>::value,
        "component types must be compatible");
    this->output_to<O>(peer.id, I);
  }

  template <std::size_t O, typename... Args>
  void output_to(Args&&... args) {
    bool prev =
        (this->outgoing_).template connect_to<O>(std::forward<Args>(args)...);
    CkAssertMsg(!(this->active && prev),
                "component must be offline to change outbound connections");
    if (this->active) {
      (this->outgoing_).template try_flush<O>();
    }
  }

  virtual out_set action(in_set&) = 0;

  virtual void activate(void) override {
    CkAssertMsg(!this->active, "component already online");
    this->activated = this->active = true;
    incoming_iterator search;
    while (this->active &&
           (search = this->find_ready_()) != std::end(this->incoming_)) {
      this->stage_action(search);
    }
  }

  virtual void deactivate(status_ signal) override {
    this->active = false;

    this->empty_buffers_();
    if (signal == status_::kInvalidation) {
      this->send_invalidations_();
    }

    this->notify_listeners(signal);
  }

  virtual bool collectible(void) const override {
    return this->activated && !this->active && this->outgoing_.empty();
  }

 protected:
  inline static bool is_ready(const in_set& set) {
    return is_ready_<n_inputs_ - 1>(set);
  }

 private:
  template <std::size_t I>
  inline static void return_(in_set& set) {
    auto& val = std::get<I>(set);
    try_return(std::move(val));
  }

  template <std::size_t I>
  inline typename std::enable_if<(I == 0)>::type empty_buffer_(in_set& set) {
    return_<I>(set);
  }

  template <std::size_t I>
  inline typename std::enable_if<(I >= 1)>::type empty_buffer_(in_set& set) {
    this->empty_buffer_<(I - 1)>(set);
    return_<I>(set);
  }

  void empty_buffers_(void) {
    if (this->incoming_.empty()) {
      return;
    } else {
      // try to return all unused values and
      for (auto& set : this->incoming_) {
        empty_buffer_<(n_inputs_ - 1)>(set);
      }
      // clear values so they're not used again
      this->incoming_.clear();
    }
  }

  template <std::size_t O>
  inline typename std::enable_if<(O == 0)>::type send_invalidation_(void) {}

  template <std::size_t O>
  inline typename std::enable_if<(O == 1)>::type send_invalidation_(void) {
    (this->outgoing_).template invalidate<(O - 1)>();
  }

  template <std::size_t O>
  inline typename std::enable_if<(O > 1)>::type send_invalidation_(void) {
    this->send_invalidation_<(O - 1)>();
    (this->outgoing_).template invalidate<(O - 1)>();
  }

  void send_invalidations_(void) { this->send_invalidation_<n_outputs_>(); }

  template <std::size_t I>
  void on_invalidation_(void) {
    this->deactivate(kInvalidation);
  }

  void buffer_ready_set_(in_set&& set) {
    // this should be consistent with "find_ready"
    // and the opposite of "find_gap"
    this->incoming_.emplace_front(std::move(set));
  }

  template <std::size_t I>
  inline static typename std::enable_if<(I == 0) && (n_inputs_ == 1)>::type
  direct_stage(component<Inputs, Outputs>* self, in_elt_t<I>&& val) {
    CkAssertMsg((bool)val, "not equipped for invalidations!");
    in_set set(std::move(val));
    if (!self->stage_action(set, nullptr)) {
      self->buffer_ready_set_(std::move(set));
    }
  }

  template <std::size_t I>
  inline static typename std::enable_if<(I >= 0) && (n_inputs_ > 1)>::type
  direct_stage(component<Inputs, Outputs>* self, in_elt_t<I>&& val) {
    CkAbort("-- unreachable --");
  }

  inline void stage_action(const incoming_iterator& search) {
    this->stage_action(*search, [&](void) { this->incoming_.erase(search); });
  }

  // returns true if the set was consumed
  template <typename Fn>
  inline bool stage_action(in_set& set, const Fn& cleanup) {
    if (this->active) {
      try {
        auto res = this->action(set);
        invoker_<Fn>::invoke(cleanup);
        this->outgoing_.unspool(res);
        if (!this->persistent) {
          this->deactivate(kCompletion);
        }
      } catch (suspension_& s) {
        if (this->id != s.com) {
          throw s;
        } else {
          invoker_<Fn>::invoke(cleanup);
        }
      }
      return true;
    } else {
      return false;
    }
  }

  inline incoming_iterator find_ready_(void) {
    if (!this->incoming_.empty()) {
      for (auto it = std::begin(this->incoming_);
           it != std::end(this->incoming_); it++) {
        if (is_ready(*it)) {
          return it;
        }
      }
    }
    return std::end(this->incoming_);
  }

  template <std::size_t I>
  inline incoming_iterator find_gap_(void) {
    if (!this->incoming_.empty()) {
      auto search = this->incoming_.rbegin();
      for (; search != this->incoming_.rend(); search++) {
        if (!std::get<I>(*search)) {
          return --search.base();
        }
      }
    }

    return std::end(this->incoming_);
  }

  template <std::size_t I>
  inline static typename std::enable_if<I == 0, bool>::type is_ready_(
      const in_set& set) {
    return (bool)std::get<I>(set);
  }

  template <std::size_t I>
  inline static typename std::enable_if<I >= 1, bool>::type is_ready_(
      const in_set& set) {
    return (bool)std::get<I>(set) && is_ready_<(I - 1)>(set);
  }

  template <std::size_t I, typename Array>
  inline static typename std::enable_if<(I == 0)>::type make_acceptors_(
      Array& arr) {
    using elt_t = typename Array::value_type;
    new (&arr[I]) elt_t(accept<I>);
  }

  template <std::size_t I, typename Array>
  inline static typename std::enable_if<(I >= 1)>::type make_acceptors_(
      Array& arr) {
    using elt_t = typename Array::value_type;
    new (&arr[I]) elt_t(accept<I>);
    make_acceptors_<(I - 1), Array>(arr);
  }

  template <typename Array>
  inline static Array make_acceptors_(void) {
    using type = Array;
    std::aligned_storage<sizeof(type), alignof(type)> storage;
    auto* arr = reinterpret_cast<type*>(&storage);
    make_acceptors_<n_inputs_ - 1, type>(*arr);
    return *arr;
  }
};

#if HYPERCOMM_ERROR_CHECKING
template <typename Inputs, typename Outputs>
typename component<Inputs, Outputs>::in_type_array_t
    component<Inputs, Outputs>::in_types =
        make_type_list_<component<Inputs, Outputs>::in_type,
                        component<Inputs, Outputs>::n_inputs_>();
#endif

template <typename Inputs, typename Outputs>
typename component<Inputs, Outputs>::accept_array_t
    component<Inputs, Outputs>::acceptors =
        component<Inputs, Outputs>::make_acceptors_<accept_array_t>();
}  // namespace components
template <typename Input, typename Output>
using component = components::component<Input, Output>;
}  // namespace hypercomm

#endif
