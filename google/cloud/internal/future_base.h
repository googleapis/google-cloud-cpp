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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_BASE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_BASE_H
/**
 * @file
 *
 * Define the implementation details for `google::cloud::future<T>`.
 */

#include "google/cloud/internal/future_impl.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
/**
 * Refactor common functions to `future<T>`, `future<R&>` and `future<void>`.
 * @tparam T
 */
template <typename T>
class future_base {  // NOLINT(readability-identifier-naming)
 public:
  future_base() noexcept = default;
  future_base(future_base&&) noexcept = default;
  future_base& operator=(future_base&&) noexcept = default;

  future_base(future_base const& rhs) = delete;
  future_base& operator=(future_base const& rhs) = delete;

  /// Returns true if the future has a shared state.
  bool valid() const noexcept { return static_cast<bool>(shared_state_); }

  /**
   * Blocks until the shared state is ready.
   *
   * @throws std::future_error with std::future_errc::no_state if the future has
   *     no shared state.
   */
  void wait() const {
    check_valid();
    shared_state_->wait();
  }

  /**
   * Blocks until the shared state is ready, or until @p duration time has
   * elapsed.
   *
   * @param duration the maximum time to wait for the shared state to become
   *     ready.
   *
   * @tparam Rep a placeholder to match the Rep tparam for @p duration's
   *     type, the semantics of this template parameter are documented in
   *     `std::chrono::duration<>` (in brief, the underlying arithmetic type
   *     used to store the number of ticks), for our purposes it is simply a
   *     formal parameter.
   * @tparam Period a placeholder to match the Period tparam for @p duration's
   *     type, the semantics of this template parameter are documented in
   *     `std::chrono::duration<>` (in brief, the length of the tick in seconds,
   *     expressed as a `std::ratio<>`), for our purposes it is simply a formal
   *     parameter.
   *
   * @return `std::future_status::ready` if the shared state is satisfied.
   *     `std::future_status::deferred` if the shared state is not satisfied and
   *     there is a continuation ready to execute when it is satisfied.
   *     `std::future_status::timeout` otherwise.
   *
   * @throws std::future_error with std::future_errc::no_state if the future has
   *     no shared state.
   */
  template <typename Rep, typename Period>
  std::future_status wait_for(
      std::chrono::duration<Rep, Period> const& rel_time) const {
    check_valid();
    return shared_state_->wait_for(rel_time);
  }

  /**
   * Blocks until the shared state is ready, or until @p deadline expires.
   *
   * @param deadline a time point after which this function no longer waits.
   *
   * @tparam Clock a placeholder to match the Clock tparam for @p tp's
   *     type, the semantics of this template parameter are documented in
   *     `std::chrono::time_point<>` (in brief, the underlying clock type
   *     associated with the time point), for our purposes it is simply a
   *     formal parameter.
   *
   * @return `std::future_status::ready` if the shared state is satisfied.
   *     `std::future_status::deferred` if the shared state is not satisfied and
   *     there is a continuation ready to execute when it is satisfied.
   *     `std::future_status::timeout` otherwise.
   *
   * @throws std::future_error with std::future_errc::no_state if the future has
   *     no shared state.
   */
  template <typename Rep, typename Period>
  std::future_status wait_until(
      std::chrono::time_point<Rep, Period> const& abs_time) const {
    check_valid();
    return shared_state_->wait_until(abs_time);
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

  /**
   * Cancel the future by invoking cancel() on the shared state.
   */
  bool cancel() { return shared_state_->cancel(); }

 protected:
  /// Shorthand to refer to the shared state type.
  using shared_state_type = internal::future_shared_state<T>;

  /// Creates a future from a shared state.
  explicit future_base(std::shared_ptr<shared_state_type> state)
      : shared_state_(std::move(state)) {}

  /// Throws an exception if the shared state is not valid.
  void check_valid() const {
    if (!shared_state_) {
      ThrowFutureError(std::future_errc::no_state, __func__);
    }
  }

  std::shared_ptr<shared_state_type> shared_state_;
};

template <typename T>
class promise_base {  // NOLINT(readability-identifier-naming)
 public:
  explicit promise_base(std::function<void()> cancellation_callback)
      : shared_state_(
            std::make_shared<shared_state_type>(cancellation_callback)) {}
  promise_base(promise_base&&) noexcept = default;

  ~promise_base() {
    if (shared_state_) {
      shared_state_->abandon();
    }
  }

  // Delete the operators we do not want. Note that the move operator is deleted
  // because the implementation must call the destructor (or at least abandon)
  // on *this.
  promise_base& operator=(promise_base&&) noexcept = delete;
  promise_base(promise_base const&) = delete;
  promise_base& operator=(promise_base const&) = delete;

  /**
   * Satisfies the shared state using the exception @p ex.
   *
   * @throws std::future_error with std::future_errc::promise_already_satisfied
   *   if the shared state is already satisfied.
   * @throws std::future_error with std::no_state if the promise does not have
   *   a shared state.
   */
  void set_exception(std::exception_ptr ex) {
    if (!shared_state_) {
      ThrowFutureError(std::future_errc::no_state, __func__);
    }
    shared_state_->set_exception(std::move(ex));
  }

 protected:
  /// Shorthand to refer to the shared state type.
  using shared_state_type = internal::future_shared_state<T>;
  std::shared_ptr<shared_state_type> shared_state_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_BASE_H
