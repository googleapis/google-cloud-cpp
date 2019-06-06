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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_FUTURE_GENERIC_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_FUTURE_GENERIC_H_
/**
 * @file
 *
 * Fully specialize `future<void>` and `promise<R>` for void.
 */

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
  using shared_state_type =
      typename internal::future_base<T>::shared_state_type;

  // workaround Apple Clang-7xx series bug, if we use `= default` here, the
  // compiler believes there is no default constructor defined. :shrug:
  future() noexcept {}

  /**
   * Creates a new future that unwraps @p rhs.
   *
   * This constructor creates a new shared state that becomes satisfied when
   * both `rhs` and `rhs.get()` become satisfied. If `rhs` is satisfied, but
   * `rhs.get()` returns an invalid future then the newly created future becomes
   * satisfied with a `std::future_error` exception, and the exception error
   * code is `std::future_errc::broken_promise`.
   *
   * @note The technical specification requires this to be a `noexcept`
   *   constructor I (coryan) believe this is a defect in the technical
   *   specification, as this *creates* a new shared state: shared states are
   *   dynamically allocated, and the allocator (which might be the default
   *   `operator new`) may raise.
   */
  future(future<future<T>>&& rhs) noexcept(false);

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

  using internal::future_base<T>::is_ready;
  using internal::future_base<T>::valid;
  using internal::future_base<T>::wait;
  using internal::future_base<T>::wait_for;
  using internal::future_base<T>::wait_until;

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
  typename internal::then_helper<F, T>::future_t then(F&& func) {
    this->check_valid();
    using requires_unwrap_t =
        typename internal::then_helper<F, T>::requires_unwrap_t;
    return then_impl(std::forward<F>(func), requires_unwrap_t{});
  }

  explicit future(std::shared_ptr<shared_state_type> state)
      : internal::future_base<T>(std::move(state)) {}

 private:
  /// Implement `then()` if the result does not require unwrapping.
  template <typename F>
  typename internal::then_helper<F, T>::future_t then_impl(F&& functor,
                                                           std::false_type);

  /// Implement `then()` if the result requires unwrapping.
  template <typename F>
  typename internal::then_helper<F, T>::future_t then_impl(F&& functor,
                                                           std::true_type);

  template <typename U>
  friend class future;
  friend class future<void>;
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
  promise(promise&&) noexcept = default;

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
    if (!this->shared_state_) {
      internal::ThrowFutureError(std::future_errc::no_state, __func__);
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

/// Create a future<void> that is immediately ready.
template <typename T>
inline future<typename internal::make_ready_return<T>::type> make_ready_future(
    T&& t) {
  using V = typename internal::make_ready_return<T>::type;
  // TODO(#1410) - Implement specializations of future<R&> and promise<R&>.
  static_assert(!std::is_reference<V>::value, "future<R&> is not implemented");
  promise<V> p;
  p.set_value(std::forward<T>(t));
  return p.get_future();
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_FUTURE_GENERIC_H_
