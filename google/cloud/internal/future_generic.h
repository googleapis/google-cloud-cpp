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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_GENERIC_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_GENERIC_H_
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
 * Implement ISO/IEC TS 19571:2016 `future<T>`.
 */
template <typename T>
class future final : private internal::future_base<T> {
 public:
  future() noexcept = default;

  // TODO(#1345) - implement the unwrapping constructor.
  // future(future<future<T>>&& f) noexcept;
  // future& operator=(future<future<T>>&& f) noexcept;

  /**
   * Waits until the shared state becomes ready, then retrieves the value stored
   * in the shared state.
   *
   * @note This operation invalidates the future, subsequent calls will fail,
   *   the application should capture the returned value because it would.
   *
   * @throws any exceptions stored in the shared state.
   * @throws std::future_error with std::no_state if the future does not have
   *   a shared state.
   */
  T get() {
    this->check_valid();
    std::shared_ptr<shared_state_type> tmp;
    tmp.swap(this->shared_state_);
    return tmp->get();
  }

  using internal::future_base<T>::valid;
  using internal::future_base<T>::wait;
  using internal::future_base<T>::wait_for;
  using internal::future_base<T>::wait_until;

 private:
  /// Shorthand to refer to the shared state type.
  using shared_state_type = internal::future_shared_state<T>;

  friend class promise<T>;
  explicit future(std::shared_ptr<shared_state_type> state)
      : internal::future_base<T>(std::move(state)) {}
};

/**
 * Implement `promise<T>` as defined in ISO/IEC TS 19571:2016.
 */
template <typename T>
class promise final : private internal::promise_base<T> {
 public:
  /// Creates a promise with an unsatisfied shared state.
  promise() = default;

  /// Constructs a new promise and transfer any shared state from @p rhs.
  promise(promise&& rhs) noexcept = default;

  /// Abandons the shared state in `*this`, if any, and transfers the shared
  /// state from @p rhs.
  promise& operator=(promise&& rhs) noexcept {
    promise tmp(std::move(rhs));
    this->swap(tmp);
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
    std::swap(this->shared_state_, other.shared_state_);
  }

  /**
   * Creates the `future<T>` using the same shared state as `*this`.
   */
  future<T> get_future() {
    internal::future_shared_state<T>::mark_retrieved(this->shared_state_);
    return future<T>(this->shared_state_);
  }

  /**
   * Satisfies the shared state.
   *
   * @throws std::future_error with std::future_errc::promise_already_satisfied
   *   if the shared state is already satisfied.
   * @throws std::future_error with std::no_state if the promise does not have
   *   a shared state.
   */
  void set_value(T&& value) {
    if (not this->shared_state_) {
      throw std::future_error(std::future_errc::no_state);
    }
    this->shared_state_->set_value(std::forward<T>(value));
  }

  /**
   * Satisfies the shared state.
   *
   * @throws std::future_error with std::future_errc::promise_already_satisfied
   *   if the shared state is already satisfied.
   * @throws std::future_error with std::no_state if the promise does not have
   *   a shared state.
   */
  void set_value(T const& value) { set_value(T(value)); }

  using internal::promise_base<T>::set_exception;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_GENERIC_H_
