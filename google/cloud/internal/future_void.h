// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_VOID_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_VOID_H_
/**
 * @file
 *
 * Fully specialize `future<void>` and `promise<R>` for void.
 */

#include "google/cloud/internal/port_platform.h"

// C++ futures only make sense when exceptions are enabled.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
#include "google/cloud/internal/future_base.h"
#include "google/cloud/internal/future_fwd.h"
#include "google/cloud/internal/future_impl.h"
#include "google/cloud/internal/future_then_meta.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * Implement ISO/IEC TS 19571:2016 future for void.
 */
template <>
class future<void> final : private internal::future_base<void> {
 public:
  future() noexcept = default;

  // TODO(#1345) - implement the unwrapping constructor.
  // future(future<future<void>>&& f) noexcept;
  // future& operator=(future<future<void>>&& f) noexcept;

  /**
   * Waits until the shared state becomes ready, then retrieves the value stored
   * in the shared state.
   *
   * @throws any exceptions stored in the shared state.
   * @throws std::future_error with std::no_state if the future does not have
   *   a shared state.
   */
  void get() {
    check_valid();
    std::shared_ptr<shared_state_type> tmp;
    tmp.swap(shared_state_);
    return tmp->get();
  }

  using future_base::valid;
  using future_base::wait;
  using future_base::wait_for;
  using future_base::wait_until;

  /**
   * Attach a continuation to the future.
   *
   * Attach a callable @a func to be invoked when the future is
   * ready.  The return type is a future wrapping the return type of
   * @a func.
   *
   * @return future<T> where T is std::result_of_t<F, R> (basically).
   * If T matches future<U> then it returns future<U>.  The returned
   * future will contain the result of @a func.
   * @param func a Callable to be invoked when the future is ready.
   * The function might be called immediately, e.g., if the future is
   * ready.
   *
   * Side effects: valid() == false if the operation is successful.
   */
  template <typename F>
  typename internal::then_helper<F, void>::future_t then(F&& func) {
    check_valid();
    using requires_unwrap_t =
        typename internal::then_helper<F, void>::requires_unwrap_t;
    return then_impl(std::forward<F>(func), requires_unwrap_t{});
  }

  /**
   * Return true if the future is satisfied.
   *
   * @throws std::future_error if the future is invalid.
   */
  bool is_ready() const {
    check_valid();
    return shared_state_->is_ready();
  }

 private:
  friend class promise<void>;
  explicit future(std::shared_ptr<shared_state_type> state)
      : future_base<void>(std::move(state)) {}

  /// Implement `then()` if the result does not require unwrapping.
  template <typename F>
  typename internal::then_helper<F, void>::future_t then_impl(F&& functor,
                                                              std::false_type) {
    // g++-4.9 gets confused about the use of a protected type alias here, so
    // create a non-protected one:
    using local_state_type = shared_state_type;
    // Some type aliases to make the rest of the code more readable.
    using functor_result_t =
        typename internal::then_helper<F, void>::functor_result_t;
    using future_t = typename internal::then_helper<F, void>::future_t;

    // The `shared_state_type` (aka `future_shared_state<void>`) is be written
    // without any reference to the `future<void>` class, otherwise there would
    // be cycling dependencies between the two classes. We must adapt the
    // provided functor, which takes a `future<void>` parameter to take a
    // `shared_ptr<shared_state_type` parameter so it can be consumed by the
    // underlying class. Because we need to support C++11, we use a local class
    // instead of a lambda, as support for move+capture in lambdas is a C++14
    // feature.
    struct adapter {
      explicit adapter(F&& func) : functor(func) {}

      auto operator()(std::shared_ptr<local_state_type> state)
          -> functor_result_t {
        return functor(future<void>(std::move(state)));
      }

      F functor;
    };

    auto output_shared_state = shared_state_type::make_continuation(
        shared_state_, adapter(std::forward<F>(functor)));

    // Nothing throws after this point, and we have not changed the state if
    // anything did throw.
    shared_state_.reset();
    return future_t(std::move(output_shared_state));
  }

  /// Implement `then()` if the result requires unwrapping.
  // TODO(#1345) - implement this in a future PR.
  template <typename F>
  typename std::shared_ptr<typename internal::then_helper<F, void>::state_t>
  then_impl(F&& func, std::true_type);
};

/**
 * Specialize promise as defined in ISO/IEC TS 19571:2016 for void.
 */
template <>
class promise<void> final : private internal::promise_base<void> {
 public:
  /// Creates a promise with an unsatisfied shared state.
  promise() = default;

  /// Constructs a new promise and transfer any shared state from @p rhs.
  promise(promise&& rhs) noexcept = default;

  /// Abandons the shared state in `*this`, if any, and transfers the shared
  /// state from @p rhs.
  promise& operator=(promise&& rhs) noexcept {
    promise tmp(std::move(rhs));
    tmp.swap(*this);
    return *this;
  }

  /**
   * Abandons any shared state.
   *
   * If the shared state was not already satisfied it becomes satisfied with
   * a `std::future_error` exception. The error code in this exception is
   * `std::future_errc::broken_promise`.
   */
  ~promise() = default;

  promise(promise const&) = delete;
  promise& operator=(promise const&) = delete;

  /// Swaps the shared state in `*this` with @p rhs.
  void swap(promise& other) noexcept {
    std::swap(shared_state_, other.shared_state_);
  }

  /**
   * Creates the `future<void>` using the same shared state as `*this`.
   */
  future<void> get_future() {
    shared_state_type::mark_retrieved(shared_state_);
    return future<void>(shared_state_);
  }

  /**
   * Satisfies the shared state.
   *
   * @throws std::future_error with std::future_errc::promise_already_satisfied
   *   if the shared state is already satisfied.
   * @throws std::future_error with std::no_state if the promise does not have
   *   a shared state.
   */
  void set_value() {
    if (not shared_state_) {
      throw std::future_error(std::future_errc::no_state);
    }
    shared_state_->set_value();
  }

  using promise_base<void>::set_exception;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_VOID_H_
