// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_IMPL_H

#include "google/cloud/internal/future_fwd.h"
#include "google/cloud/terminate_handler.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <future>
#include <mutex>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/// Throws `std::future_error` or terminates the program.
[[noreturn]] void ThrowFutureError(std::future_errc ec, char const* msg);

/// Rethrows the exception or terminates the program.
[[noreturn]] void ThrowDelegate(std::exception_ptr ex, char const* msg);

/// Creates a std::exception_ptr with a `std::future_error`.
std::exception_ptr MakeFutureError(std::future_errc ec);

// Forward declare the implementation type as we will need it for some helpers.
template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
class future_shared_state;

/**
 * A monostate for `future<void>`.
 *
 * The implementation already uses `absl::monostate` to represent "future<void>
 * is **not** set". We need a distinct type to represent "`future<void>` value
 * is set".
 */
struct FutureVoid {};

// This struct is a friend of `future<T>`, forward declare it here. As the name
// implies, it provides the implementation for `future<T>::then()`.
struct FutureThenImpl;

// Compute the type `U` used in a `future_shared_state<U>` for a `future<T>.
template <typename T>
struct SharedStateValueImpl {
  using type = T;
};

// Compute the type `U` used in a `future_shared_state<U>` for a `future<T>.
template <>
struct SharedStateValueImpl<void> {
  using type = FutureVoid;
};

// Compute the type `U` used in a `future_shared_state<U>` for a `future<T>.
template <typename T>
using SharedStateValue = typename SharedStateValueImpl<T>::type;

// Compute the `future_shared_state<U>` for a `future<T>.
template <typename T>
using SharedStateType = future_shared_state<SharedStateValue<T>>;

// Compute the type `U` such that `future<T>::then(F) -> future<U>`.
template <typename U>
struct UnwrappedTypeImpl {
  using type = U;
};

// Compute the type `U` such that `future<T>::then(F) -> future<U>`.
template <typename U>
struct UnwrappedTypeImpl<future<U>> {
  using type = typename UnwrappedTypeImpl<U>::type;
};

// Compute the type `U` such that `future<T>::then(F) -> future<U>`.
template <typename U>
using UnwrappedType = typename UnwrappedTypeImpl<U>::type;

// Used in `make_ready_future()` to determine if a type is
// `std::reference_wrapper<U>`.
template <typename U>
struct IsReferenceWrapper : public std::false_type {};
template <typename U>
struct IsReferenceWrapper<std::reference_wrapper<U>> : public std::true_type {};

/**
 * Define an interface to type-erased continuations.
 *
 * Continuations (the parameters to a `.then()` call) can be of arbitrary
 * types: any callable (lambdas, function pointers, `std::function<>`) should be
 * accepted. We want to hold these continuations as type-erased objects, so we
 * can call them without having to know their types.
 *
 * A continuation object will hold both the callable and the state to call it
 * with, the implementation of `.then()` takes care of those details.
 */
template <typename T>
class Continuation {  // NOLINT(readability-identifier-naming)
 public:
  virtual ~Continuation() = default;

  /// Invoke the continuation.
  virtual void Execute(future_shared_state<T>& state) = 0;
};

/**
 * The value for a future once `.get()` is called.
 *
 * Once `.get()` is called the future no longer contains a value, but is no
 * longer in the initial state either. A second attempt to call `.get()` should
 * result in an exception.
 *
 */
struct FutureValueRetrieved {};

/**
 * The shared state for `future<T>` and `promise<T>`.
 *
 * The shared state contains either (1) the value stored by the promise, or (2)
 * the exception stored by the promise, (3) nothing, if the future has not been
 * satisfied, (4) a different kind of nothing, if the future was satisfied and
 * the value or exception was extracted.  It may also contain a continuation to
 * be called when the shared state is satisfied.
 *
 * The shared state value can be retrieved only once. This is enforced by
 * `future<T>::get()`: calling `.get()` invalidates the future.
 *
 * Calling `future<T>::then()` also invalidates the future, and all operations,
 * such as `.get()` and `.then()` cannot be called again.
 *
 * There are no accessors in `future<T>` or `promise<T>` to get the
 * continuation.
 *
 * Case (4) above is needed in the implementation of `.then()`. No public APIs
 * of `future<T>` or `promise<T>` can get a shared state to contain it.
 */
template <typename T>
class future_shared_state final {  // NOLINT(readability-identifier-naming)
 public:
  /**
   * The different states of the shared state.
   *
   * The shared state is initialized with `absl::monostate`, this represents
   * an unsatisfied future.
   *
   * The future may be satisfied with an exception, stored as a
   * `std::exception_ptr`.
   *
   * The future may be satisfied with a value, stored as a `T`. For
   * `future<void>` the type is `FutureVoid`.
   *
   * Finally, the value and/or exception may be retrieved as part of invoking
   * any continuation. We store this as a `FutureValueRetrieved`.
   */
  using ValueType = absl::variant<absl::monostate, std::exception_ptr, T,
                                  FutureValueRetrieved>;

#if __clang__
#elif __GNUC__
  // With some versions of Abseil and GCC the compiler emits spurious warnings.
  // This diagnostic is useful in other places, so let's just silence it here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
  future_shared_state() : future_shared_state([] {}, ValueType{}) {}

  // Used in the implementation of `.then()` to transfer the value from one
  // instance to a new instance.
  explicit future_shared_state(ValueType value)
      : future_shared_state([] {}, std::move(value)) {}

  explicit future_shared_state(std::function<void()> cancellation_callback,
                               ValueType value = {})
      : value_(std::move(value)),
        cancellation_callback_(std::move(cancellation_callback)) {}
#if __clang__
#elif __GNUC__
#pragma GCC diagnostic pop
#endif

  /// The implementation details for `future<T>::get()`
  T get() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this] { return is_ready_unlocked(); });
    struct Visitor {
      T operator()(T v) { return v; }
      T operator()(std::exception_ptr ex) {
        ThrowDelegate(
            std::move(ex),
            "future<T>::get() had an exception but exceptions are disabled");
      }
      T operator()(FutureValueRetrieved) {
        ThrowFutureError(std::future_errc::no_state,
                         "future<T>::get() - already retrieved");
      }
      T operator()(absl::monostate) {
        ThrowFutureError(std::future_errc::no_state,
                         "future<T>::get() - not set");
      }
    };
    // Note that the value is moved out. It is impossible to retrieve the value
    // a second time. The `.get()` operation on a `future<T>` invalidates the
    // future, so new calls will fail.
    ValueType tmp(FutureValueRetrieved{});
    tmp.swap(value_);
    return absl::visit(Visitor{}, std::move(tmp));
  }

  /**
   * The implementation details for `promise<T>::set_value()`.
   *
   * If the shared state is not already satisfied this function atomically
   * stores the value and the state becomes satisfied.
   *
   * @param value the value to store in the shared state.
   * @throws std::future_error if the shared state was already satisfied. The
   *     error code is `std::future_errc::promise_already_satisfied`.
   */
  void set_value(T value) {
    std::unique_lock<std::mutex> lk(mu_);
    if (is_ready_unlocked()) {
      ThrowFutureError(std::future_errc::promise_already_satisfied, __func__);
    }
    // We can only reach this point once, all other states are terminal.
    // Therefore we know that `value_` has not been initialized.

    // TODO(#1405) - this is calling application code while holding a lock.
    // That could result in a deadlock (or at least unbounded priority
    // inversions) if the move constructor for `T` takes a long time to execute.
    value_ = std::move(value);
    notify_now(std::move(lk));
  }

  /// Return true if the shared state has a value or an exception.
  bool is_ready() const {
    std::unique_lock<std::mutex> lk(mu_);
    return is_ready_unlocked();
  }

  /// Return true if the shared state can be cancelled.
  bool cancellable() const { return !is_ready() && !cancelled_; }

  /// Block until is_ready() returns true.
  void wait() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this] { return is_ready_unlocked(); });
  }

  /**
   * Block until `is_ready()` returns true or until @p duration time has
   * elapsed.
   *
   * @param duration the maximum time to wait for `is_ready()`.
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
   */
  template <typename Rep, typename Period>
  std::future_status wait_for(std::chrono::duration<Rep, Period> duration) {
    std::unique_lock<std::mutex> lk(mu_);
    bool result =
        cv_.wait_for(lk, duration, [this] { return is_ready_unlocked(); });
    if (result) return std::future_status::ready;
    if (continuation_) return std::future_status::deferred;
    return std::future_status::timeout;
  }

  /**
   * Block until is_ready() returns true or until the @p deadline.
   *
   * @param deadline the maximum time to wait.
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
   */
  template <typename Clock>
  std::future_status wait_until(std::chrono::time_point<Clock> deadline) {
    std::unique_lock<std::mutex> lk(mu_);
    if (!lk.owns_lock()) return std::future_status::timeout;
    bool result =
        cv_.wait_until(lk, deadline, [this] { return is_ready_unlocked(); });
    if (result) return std::future_status::ready;
    if (continuation_) return std::future_status::deferred;
    return std::future_status::timeout;
  }

  /// Set the shared state to hold an exception and notify immediately.
  void set_exception(std::exception_ptr ex) {
    std::unique_lock<std::mutex> lk(mu_);
    set_exception(std::move(ex), lk);
    notify_now(std::move(lk));
  }

  /**
   * Abandon the shared state.
   *
   * The destructor of `promise<T>` abandons the state. If the shared state is
   * satisfied this has no effect. Otherwise the state is satisfied with an
   * `std::future_error` exception. The error code is
   * `std::future_errc::broken_promise`.
   */
  void abandon() {
    std::unique_lock<std::mutex> lk(mu_);
    if (is_ready_unlocked()) return;
    set_exception(MakeFutureError(std::future_errc::broken_promise), lk);
    notify_now(std::move(lk));
  }

  void set_continuation(std::unique_ptr<Continuation<T>> c) {
    std::unique_lock<std::mutex> lk(mu_);
    if (continuation_) {
      ThrowFutureError(std::future_errc::future_already_retrieved, __func__);
    }
    // If the future is already satisfied, invoke the continuation immediately.
    if (is_ready_unlocked()) {
      // Release the lock before calling the user's code, holding locks during
      // callbacks is a bad practice.
      lk.unlock();
      c->Execute(*this);
      return;
    }
    continuation_ = std::move(c);
  }

  std::function<void()> release_cancellation_callback() {
    return std::move(cancellation_callback_);
  }

  // Try to cancel the task by invoking the cancellation_callback.
  bool cancel() {
    if (!cancellable()) return false;
    cancellation_callback_();
    // If the callback fails with an exception we assume it had no effect.
    // Incidentally this means we provide the strong exception guarantee for
    // this function.
    cancelled_ = true;
    return true;
  }

  /**
   * The implementation details for `promise<T>::get_future()`.
   *
   * `promise<T>::get_future()` can be called exactly once, this function
   * must raise `std::future_error` if (quoting the C++ spec):
   *
   * `get_future` has already been called on a `promise` with the same shared
   * state as `*this`
   *
   * While it is not clear how one could create multiple promises pointing to
   * the same shared state, it is easier to keep all the locking and atomic
   * checks in one class.
   *
   * @throws std::future_error if the operation fails.
   */
  static void mark_retrieved(std::shared_ptr<future_shared_state> const& sh) {
    if (!sh) {
      ThrowFutureError(std::future_errc::no_state, __func__);
    }
    if (sh->retrieved_.test_and_set()) {
      ThrowFutureError(std::future_errc::future_already_retrieved, __func__);
    }
  }

  /**
   * Extract the value.
   *
   * This is used in the implementation of `.then()`, to move `value_` to a
   * new future.
   *
   * It is not necessary to notify any threads blocked on this shared state
   * change.
   */
  ValueType value() {
    ValueType result(FutureValueRetrieved{});
    {
      std::lock_guard<std::mutex> lk(mu_);
      result.swap(value_);
    }
    return result;
  }

 protected:
  bool is_ready_unlocked() const {
    return !absl::holds_alternative<absl::monostate>(value_);
  }

  /// Satisfy the shared state using an exception.
  void set_exception(std::exception_ptr ex, std::unique_lock<std::mutex>&) {
    if (is_ready_unlocked()) {
      ThrowFutureError(std::future_errc::promise_already_satisfied, __func__);
    }
    value_.template emplace<std::exception_ptr>(std::move(ex));
  }

  /// If needed, notify any waiting threads that the shared state is satisfied.
  void notify_now(std::unique_lock<std::mutex> lk) {
    if (continuation_) {
      // Release the lock before calling the continuation because the
      // continuation will likely call get() to fetch the state of the future.
      lk.unlock();
      continuation_->Execute(*this);
      // If there is a continuation there can be no threads blocked on get() or
      // wait() because then() invalidates the future. Therefore we can return
      // without notifying any other threads.
      return;
    }
    cv_.notify_all();
  }

  /**
   * Keep track of whether `get_future()` has been called.
   *
   * My (@coryan) reading of the spec is: calling `get_future()` on a promise
   * should succeed exactly once, even when used from multiple threads. This
   * requires some kind of flag and synchronization primitive.
   *
   * The obvious question is whether this flag should be in `promise<T>` or part
   * of the shared state.
   *
   * If this flag was a member of `promise<T>` we would need to be able to
   * synchronize multiple threads move-copying the flag and setting it.
   *
   * If it is a member of the shared state then it can be a
   * `std::atomic_flag`, which is guaranteed to be lock free and, well, atomic.
   *
   */
  std::atomic_flag retrieved_ = ATOMIC_FLAG_INIT;

  /// Synchronize changes to `value_` and `cv_`.
  mutable std::mutex mu_;
  /// Used to wait until `value_` does not contain `absl::monostate`.
  std::condition_variable cv_;
  /// The value of the shared state.
  ValueType value_;

  /**
   * The continuation, if any, associated with this shared state.
   *
   * Note that continuations may be set independently of having a value or
   * exception. Setting a continuation does not change `value_` and does not
   * satisfy the future represented by this shared state.
   */
  std::unique_ptr<Continuation<T>> continuation_;

  // Allow users "cancel" the future with the given callback.
  std::atomic<bool> cancelled_ = ATOMIC_VAR_INIT(false);
  std::function<void()> cancellation_callback_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_IMPL_H
