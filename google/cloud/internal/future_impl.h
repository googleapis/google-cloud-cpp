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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_IMPL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_IMPL_H_
/**
 * @file
 *
 * Define the implementation details for `google::cloud::future<T>`.
 */

#include "google/cloud/internal/future_then_meta.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/terminate_handler.h"
#include <condition_variable>
#include <exception>
#include <future>
#include <mutex>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
[[noreturn]] void ThrowFutureError(std::future_errc ec, char const* msg);

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
class continuation_base {
 public:
  virtual ~continuation_base() = default;

  /// Invoke the continuation.
  virtual void execute() = 0;
};

/**
 * Common base class for all shared state classes.
 *
 * The implementation of the shared state for future<T>, future<R&> and
 * future<void> share a lot of code. This class refactors that code, it
 * represents a shared state of unknown type.
 *
 * @note While most of the invariants for promises and futures are implemented
 *   by this class, not all of them are. Notably, future values can only be
 *   retrieved once, but this is enforced because calling .get() or .then() on a
 *   future invalidates the future for further use. The shared state does not
 *   record that state change.
 */
class future_shared_state_base {
 public:
  future_shared_state_base() : mu_(), cv_(), current_state_(state::not_ready) {}

  /// Return true if the shared state has a value or an exception.
  bool is_ready() const {
    std::unique_lock<std::mutex> lk(mu_);
    return is_ready_unlocked();
  }

  /// Block until is_ready() returns true ...
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
    if (result) {
      return std::future_status::ready;
    }
    if (continuation_) {
      return std::future_status::deferred;
    }
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
    if (!lk.owns_lock()) {
      return std::future_status::timeout;
    }
    bool result =
        cv_.wait_until(lk, deadline, [this] { return is_ready_unlocked(); });
    if (result) {
      return std::future_status::ready;
    }
    if (continuation_) {
      return std::future_status::deferred;
    }
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
   * The destructor of `promise<T>` abandons the state. If it is satisfied this
   * has no effect, but otherwise the state is satisfied with an
   * `std::future_error` exception. The error code is
   * `std::future_errc::broken_promise`.
   */
  void abandon() {
    std::unique_lock<std::mutex> lk(mu_);
    if (is_ready_unlocked()) {
      return;
    }
    set_exception(std::make_exception_ptr(
                      std::future_error(std::future_errc::broken_promise)),
                  lk);
    cv_.notify_all();
  }

  void set_continuation(std::unique_ptr<continuation_base> c) {
    std::unique_lock<std::mutex> lk(mu_);
    if (continuation_) {
      ThrowFutureError(std::future_errc::future_already_retrieved, __func__);
    }
    // If the future is already satisfied, invoke the continuation immediately.
    if (is_ready_unlocked()) {
      // Release the lock before calling the user's code, holding locks during
      // callbacks is a bad practice.
      lk.unlock();
      c->execute();
      return;
    }
    continuation_ = std::move(c);
  }

 protected:
  bool is_ready_unlocked() const { return current_state_ != state::not_ready; }

  /// Satisfy the shared state using an exception.
  void set_exception(std::exception_ptr ex, std::unique_lock<std::mutex>& lk) {
    if (is_ready_unlocked()) {
      ThrowFutureError(std::future_errc::promise_already_satisfied, __func__);
    }
    exception_ = std::move(ex);
    current_state_ = state::has_exception;
  }

  /// If needed, notify any waiting threads that the shared state is satisfied.
  void notify_now(std::unique_lock<std::mutex>&& lk) {
    if (continuation_) {
      // Release the lock before calling the continuation because the
      // continuation will likely call get() to fetch the state of the future.
      lk.unlock();
      continuation_->execute();
      // If there is a continuation there can be no threads blocked on get() or
      // wait() because then() invalidates the future. Therefore we can return
      // without notifying any other threads.
      return;
    }
    // Unlock first to avoid waking up a thread which becomes immediately
    // blocked on the mutex.
    lk.unlock();
    cv_.notify_all();
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
  static void mark_retrieved(future_shared_state_base* sh) {
    if (!sh) {
      ThrowFutureError(std::future_errc::no_state, __func__);
    }
    if (sh->retrieved_.test_and_set()) {
      ThrowFutureError(std::future_errc::future_already_retrieved, __func__);
    }
  }

  // My (@coryan) reading of the spec is that calling get_future() on a promise
  // should succeed exactly once, even when used from multiple threads. This
  // requires some kind of flag and synchronization primitive. The obvious
  // question is whether this flag should be in `promise<T>` or part of the
  // shared state. If it is a member of the shared state then it can be a
  // `std::atomic_flag`, which is guaranteed to be lock free and, well, atomic.
  // But an object of type `std::atomic_flag` (or `std::atomic<bool>`) cannot
  // be a member of `promise<T>` because such objects are not MoveConstructible,
  // and `promise<T>` must be. Once could implement this with an `std::mutex` +
  // a bool, but that is more overhead than just a flag here.
  /// Keep track of whether `get_future()` has been called.
  std::atomic_flag retrieved_ = ATOMIC_FLAG_INIT;

  mutable std::mutex mu_;
  std::condition_variable cv_;
  enum class state {
    not_ready,
    has_exception,
    has_value,
  };
  state current_state_;
  std::exception_ptr exception_;

  /**
   * The continuation, if any, associated with this shared state.
   *
   * Note that continuations may be set independently of having a value or
   * exception. Setting a continuation does not change the `current_state_`
   * member variable and does not satisfy the shared state.
   */
  std::unique_ptr<continuation_base> continuation_;
};

/**
 * Forward declare the generic version of future_share_state.
 */
template <typename T>
class future_shared_state;

/**
 * The shared state for `future<T>` and `promise<T>`.
 *
 * The shared state contains either (1) the value stored by the promise, or (2)
 * the exception stored by the promise, or (3) a continuation to be called when
 * the shared state is satisfied.
 *
 * The shared state value can be retrieved only once, this is enforced by
 * `future<T>::get()`, by invalidating the future after `get()` is called. It is
 * impossible to retrieve the continuation stored in a future, calling
 * `future<T>::then()` also invalidates the future, and all operations, such as
 * `.get()` and `.then()` cannot be called again.
 *
 * We rely on these guarantees to simplify the implementation of this class.
 */
template <typename T>
class future_shared_state final : private future_shared_state_base {
 public:
  future_shared_state() : future_shared_state_base(), buffer_() {}
  ~future_shared_state() {
    if (current_state_ == state::has_value) {
      // Recall that state::has_value is a terminal state, once a value is
      // stored in this class nothing else (no exceptions nor continuations)
      // can be stored.  And if a value was stored then we need to call the
      // destructor. Even if the value was moved out, the destructor still
      // may need to do some cleanup:
      reinterpret_cast<T*>(&buffer_)->~T();
    }
  }

  using future_shared_state_base::abandon;
  using future_shared_state_base::is_ready;
  using future_shared_state_base::set_continuation;
  using future_shared_state_base::set_exception;
  using future_shared_state_base::wait;
  using future_shared_state_base::wait_for;
  using future_shared_state_base::wait_until;

  /// The implementation details for `future<T>::get()`
  T get() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this] { return is_ready_unlocked(); });
    if (current_state_ == state::has_exception) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
      std::rethrow_exception(exception_);
#else
      google::cloud::Terminate(
          "future<T>::get() had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    }
    // Note that the value is moved out. It is impossible to retrieve the value
    // a second time. The `.get()` operation on a `future<T>` invalidates the
    // future, so new calls will fail.
    return std::move(*reinterpret_cast<T*>(&buffer_));
  }

  /**
   * The implementation details for `promise<T>::set_value()`.
   *
   * If the shared state is not already satisfied this function atomically
   * stores the value and the state becomes satisfied.
   *
   * @param value the value to store in the shared state.
   * @throws `std::future_error` if the shared state was already satisfied. The
   *     error code is `std::future_errc::promise_already_satisfied`.
   */
  void set_value(T&& value) {
    std::unique_lock<std::mutex> lk(mu_);
    if (is_ready_unlocked()) {
      ThrowFutureError(std::future_errc::promise_already_satisfied, __func__);
    }
    // We can only reach this point once, all other states are terminal.
    // Therefore we know that `buffer_` has not been initialized and calling
    // placement new via the move constructor is the best way to initialize the
    // buffer.

    // TODO(#1405) - this is calling application code while holding a lock.
    // That could result in a deadlock (or at least unbounded priority
    // inversions) if the move constructor for `T` takes a long time to execute.
    new (reinterpret_cast<T*>(&buffer_)) T(std::move(value));
    current_state_ = state::has_value;
    notify_now(std::move(lk));
  }

  /**
   * Create a continuation object wrapping the given functor.
   *
   * @tparam F the functor type.
   * @param self the object that will hold the continuation.
   * @param functor the continuation type.
   * @return A shared pointer to the shared state that will store the results
   *     of the continuation.
   */
  template <typename F>
  static std::shared_ptr<typename internal::continuation_helper<F, T>::state_t>
  make_continuation(std::shared_ptr<future_shared_state> self, F&& functor);

  /**
   * Create a continuation object wrapping the given functor.
   *
   * @tparam F the functor type.
   * @param self the object that will hold the continuation.
   * @param functor the continuation type.
   * @param requires_unwrapping the functor returns a `future<U>`, and must be
   *   implicitly unwrapped to return the `U`.
   * @return A shared pointer to the shared state that will store the results
   *     of the continuation.
   */
  template <typename F>
  static std::shared_ptr<
      typename internal::unwrapping_continuation_helper<F, T>::state_t>
  make_continuation(std::shared_ptr<future_shared_state> self, F&& functor,
                    std::true_type requires_unwrapping);

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
    future_shared_state_base::mark_retrieved(sh.get());
  }

 private:
  // We use std::aligned_storage<T> because T may not have a default
  // constructor, if we used 'T' here we could not default initialize this class
  // either.
  using aligned_storage_t = std::aligned_storage<sizeof(T), alignof(T)>;
  typename aligned_storage_t::type buffer_;
};

/**
 * Specialize the shared state for `void`.
 *
 * The shared state for `void` does not have any value to hold, `get()` does
 * not return any value, and `set_value()` does not take any arguments. We must
 * use an specialization because the default implementation would define
 * `set_value()` as `set_value(void&&)` which is not legal, nor is the signature
 * we want for that matter.
 */
template <>
class future_shared_state<void> final : private future_shared_state_base {
 public:
  future_shared_state() : future_shared_state_base() {}

  using future_shared_state_base::abandon;
  using future_shared_state_base::is_ready;
  using future_shared_state_base::set_continuation;
  using future_shared_state_base::set_exception;
  using future_shared_state_base::wait;
  using future_shared_state_base::wait_for;
  using future_shared_state_base::wait_until;

  /// The implementation details for `future<void>::get()`
  void get() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this] { return is_ready_unlocked(); });
    if (current_state_ == state::has_exception) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
      std::rethrow_exception(exception_);
#else
      google::cloud::Terminate(
          "future<void>::get() had an exception but exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    }
  }

  /// The implementation details for `promise<void>::set_value()`
  void set_value() {
    std::unique_lock<std::mutex> lk(mu_);
    set_value(lk);
    notify_now(std::move(lk));
  }

  /**
   * Create a continuation object wrapping the given functor.
   *
   * Unlike most member functions in template classes this is defined out of
   * line. We need to do so because:
   *
   * * This function creates a `continuation<void>`.
   * * `continuation<void>` needs to see the full definition of `future<T>`.
   * * Therefore `continuation<T>` cannot be defined before this point.
   *
   * @tparam F the functor type.
   * @param self the object that will hold the continuation.
   * @param functor the continuation type.
   * @return A shared pointer to the shared state that will store the results
   *     of the continuation.
   */
  template <typename F>
  static std::shared_ptr<
      typename internal::continuation_helper<F, void>::state_t>
  make_continuation(std::shared_ptr<future_shared_state> self, F&& functor);

  /**
   * Create a continuation object wrapping the given functor.
   *
   * Unlike most member functions in template classes this is defined out of
   * line. We need to do so because:
   *
   * * This function creates a `continuation<void>`.
   * * `continuation<void>` needs to see the full definition of `future<T>`.
   * * Therefore `continuation<T>` cannot be defined before this point.
   *
   * @tparam F the functor type.
   * @param self the object that will hold the continuation.
   * @param functor the continuation type.
   * @return A shared pointer to the shared state that will store the results
   *     of the continuation.
   */
  template <typename F>
  static std::shared_ptr<
      typename internal::unwrapping_continuation_helper<F, void>::state_t>
  make_continuation(std::shared_ptr<future_shared_state> self, F&& functor,
                    std::true_type);

  /**
   * The implementation details for `promise<void>::get_future()`.
   *
   * `promise<void>::get_future()` can be called exactly once, this function
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
    future_shared_state_base::mark_retrieved(sh.get());
  }

 private:
  void set_value(std::unique_lock<std::mutex> const& lk) {
    if (is_ready_unlocked()) {
      ThrowFutureError(std::future_errc::promise_already_satisfied, __func__);
    }
    current_state_ = state::has_value;
  }
};

/**
 * Calls a functor passing `future<T>` as an argument and stores the results in
 * a `future_shared_state<R>`.
 *
 * @tparam Functor the type of the functor.
 * @param functor the callable to invoke.
 * @param input the input shared state, it must be satisfied when this function
 *     is called.
 * @param output the output shared state, it will become satisfied by passing
 *     the results of calling `functor`
 */
template <typename Functor, typename R, typename T>
void continuation_execute_delegate(
    Functor& functor, std::shared_ptr<future_shared_state<T>> input,
    future_shared_state<R>& output, std::false_type) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  try {
    output.set_value(functor(std::move(input)));
  } catch (std::future_error const&) {
    // failing to set the output with a future_error is non-recoverable, raise
    // immediately.
    throw;
  } catch (...) {
    // Other errors can be reported via the promise.
    output.set_exception(std::current_exception());
  }
#else
  output.set_value(functor(std::move(input)));
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * Calls a functor passing `future<T>` as an argument and stores the results in
 * a `future_shared_state<void>`.
 *
 * This is an specialization of `continuation_execute_delegate` for `void`
 * results. If the output value of `future<T>::then()` is a `void`, we must call
 * `.set_value()` without parameters. The generic version does not work in that
 * case.
 *
 * @tparam Functor the type of the functor.
 * @param functor the callable to invoke.
 * @param input the input shared state, it must be satisfied when this function
 *     is called.
 * @param output the output shared state, it will become satisfied by passing
 *     the results of calling `functor`
 */
template <typename Functor, typename T>
void continuation_execute_delegate(
    Functor& functor, std::shared_ptr<future_shared_state<T>> input,
    future_shared_state<void>& output, std::false_type) {
#ifdef GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  try {
    functor(std::move(input));
    output.set_value();
  } catch (std::future_error const&) {
    // failing to set the output with a future_error is non-recoverable, raise
    // immediately.
    throw;
  } catch (...) {
    // Other errors can be reported via the promise.
    output.set_exception(std::current_exception());
  }
#else
  functor(std::move(input));
  output.set_value();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * Implement continuations for `future<R>::then()`.
 *
 * Calling `future<R>::then()` creates a new shared state. When the `future<R>`
 * is satisfied the functor parameter pass to `.then()` is called and the newly
 * created shared state is satisfied with the result of calling the functor.
 *
 * This class holds both the functor to call, and the shared state to store the
 * results of calling said functor.
 *
 * @tparam R the value type for the input future.
 * @tparam Functor the type of the functor parameter, it must meet the
 *   `is_invocable<Functor, future_shared_state<R>>` requirement.
 */
template <typename Functor, typename R>
struct continuation : public continuation_base {
  using result_t = typename continuation_helper<Functor, R>::result_t;
  using input_shared_state_t = future_shared_state<R>;
  using output_shared_state_t = future_shared_state<result_t>;
  using requires_unwrap_t =
      typename continuation_helper<Functor, R>::requires_unwrap_t;

  continuation(Functor&& f, std::shared_ptr<input_shared_state_t> s)
      : functor(std::move(f)),
        input(std::move(s)),
        output(std::make_shared<future_shared_state<result_t>>()) {}

  continuation(Functor&& f, std::shared_ptr<input_shared_state_t> s,
               std::shared_ptr<output_shared_state_t> o)
      : functor(std::move(f)), input(std::move(s)), output(std::move(o)) {}

  void execute() override {
    auto tmp = input.lock();
    if (!tmp) {
      output->set_exception(std::make_exception_ptr(
          std::future_error(std::future_errc::no_state)));
      return;
    }
    // The transfer of the state depends on the types involved, delegate to
    // some helper functions.
    continuation_execute_delegate(functor, std::move(tmp), *output,
                                  requires_unwrap_t{});
    output.reset();
  }

  /// The functor called when `input` is satisfied.
  Functor functor;

  /// The shared state that must be satisfied before calling `functor`.
  std::weak_ptr<input_shared_state_t> input;

  /// The shared state that will hold the results of calling `functor`.
  std::shared_ptr<output_shared_state_t> output;
};

/**
 * Implement continuations for `future<R>::then()`.
 *
 * Calling `future<R>::then()` creates a new shared state. When the `future<R>`
 * is satisfied the functor parameter pass to `.then()` is called and the newly
 * created shared state is satisfied with the result of calling the functor.
 *
 * This class holds both the functor to call, and the shared state to store the
 * results of calling said functor.
 *
 * @tparam R the value type for the input future.
 * @tparam Functor the type of the functor parameter, it must meet the
 *   `is_invocable<Functor, future_shared_state<R>>` requirement.
 */
template <typename Functor, typename T>
struct unwrapping_continuation : public continuation_base {
  using R = typename unwrapping_continuation_helper<Functor, T>::result_t;
  using input_shared_state_t = future_shared_state<T>;
  using output_shared_state_t = future_shared_state<R>;
  using intermediate_shared_state_t = future_shared_state<R>;

  unwrapping_continuation(Functor&& f, std::shared_ptr<input_shared_state_t> s)
      : functor(std::move(f)),
        input(std::move(s)),
        intermediate(),
        output(std::make_shared<output_shared_state_t>()) {}

  void execute() override {
    auto tmp = input.lock();
    if (!tmp) {
      output->set_exception(std::make_exception_ptr(
          std::future_error(std::future_errc::no_state)));
      return;
    }
    // The transfer of the state depends on the types involved, delegate to
    // some helper functions.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
    try {
      intermediate = functor(std::move(tmp));
    } catch (...) {
      output->set_exception(std::current_exception());
      return;
    }
#else
    intermediate = functor(std::move(tmp));
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

    if (!intermediate) {
      output->set_exception(std::make_exception_ptr(
          std::future_error(std::future_errc::broken_promise)));
      return;
    }

    auto unwrapper = [](std::shared_ptr<intermediate_shared_state_t> r) {
      return r->get();
    };
    using continuation_type = internal::continuation<decltype(unwrapper), R>;
    auto continuation = google::cloud::internal::make_unique<continuation_type>(
        std::move(unwrapper), intermediate, output);
    // assert(intermediate->continuation_ == nullptr)
    // If intermediate has a continuation then the associated future would have
    // been invalid, and we never get here.
    intermediate->set_continuation(
        std::unique_ptr<continuation_base>(std::move(continuation)));
  }

  /// The functor called when `input` is satisfied.
  Functor functor;

  /// The shared state that must be satisfied before calling `functor`.
  std::weak_ptr<input_shared_state_t> input;

  /// The shared state that will hold the results of calling `functor`.
  std::shared_ptr<output_shared_state_t> intermediate;

  /// The shared state that will hold the unwrapped of calling `functor`.
  std::shared_ptr<output_shared_state_t> output;
};

// Implement the helper function to create a shared state for continuations.
template <typename T>
template <typename F>
std::shared_ptr<typename internal::continuation_helper<F, T>::state_t>
future_shared_state<T>::make_continuation(
    std::shared_ptr<future_shared_state<T>> self, F&& functor) {
  using continuation_type = internal::continuation<F, T>;
  auto continuation = google::cloud::internal::make_unique<continuation_type>(
      std::forward<F>(functor), self);
  auto result = continuation->output;
  self->set_continuation(
      std::unique_ptr<continuation_base>(std::move(continuation)));
  return result;
}

// Implement the helper function to create a shared state for continuations.
template <typename T>
template <typename F>
std::shared_ptr<
    typename internal::unwrapping_continuation_helper<F, T>::state_t>
future_shared_state<T>::make_continuation(
    std::shared_ptr<future_shared_state<T>> self, F&& functor, std::true_type) {
  // This is the unwrapped result type.
  using R = typename internal::unwrapping_continuation_helper<F, T>::result_t;
  // The type continuation that executes `F` on `self`:
  using continuation_type = internal::unwrapping_continuation<F, T>;

  // First create a continuation that calls the functor, and stores the result
  // in a `future_shared_state<future_shared_state<R>>`
  auto continuation = google::cloud::internal::make_unique<continuation_type>(
      std::forward<F>(functor), self);
  // Save the value of `continuation->output`, because the move will make it
  // inaccessible.
  std::shared_ptr<future_shared_state<R>> result = continuation->output;
  self->set_continuation(
      std::unique_ptr<continuation_base>(std::move(continuation)));
  return result;
}

// Implement the helper function to create a shared state for continuations.
template <typename F>
std::shared_ptr<typename internal::continuation_helper<F, void>::state_t>
future_shared_state<void>::make_continuation(
    std::shared_ptr<future_shared_state<void>> self, F&& functor) {
  using continuation_type = internal::continuation<F, void>;
  auto continuation = google::cloud::internal::make_unique<continuation_type>(
      std::forward<F>(functor), self);
  // Save the value of `continuation->output`, because the move will make it
  // inaccessible.
  auto result = continuation->output;
  self->set_continuation(
      std::unique_ptr<continuation_base>(std::move(continuation)));
  return result;
}

// Implement the helper function to create a shared state for continuations that
// need unwrapping.
template <typename F>
std::shared_ptr<
    typename internal::unwrapping_continuation_helper<F, void>::state_t>
future_shared_state<void>::make_continuation(
    std::shared_ptr<future_shared_state<void>> self, F&& functor,
    std::true_type) {
  // This is the unwrapped result type.
  using R =
      typename internal::unwrapping_continuation_helper<F, void>::result_t;
  // The type continuation that executes `F` on `self`:
  using continuation_type = internal::unwrapping_continuation<F, void>;

  // First create a continuation that calls the functor, and stores the result
  // in a `future_shared_state<future_shared_state<R>>`
  auto continuation = google::cloud::internal::make_unique<continuation_type>(
      std::forward<F>(functor), self);
  // Save the value of `continuation->output`, because the move will make it
  // inaccessible.
  std::shared_ptr<future_shared_state<R>> result = continuation->output;
  self->set_continuation(
      std::unique_ptr<continuation_base>(std::move(continuation)));
  return result;
}

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_FUTURE_IMPL_H_
