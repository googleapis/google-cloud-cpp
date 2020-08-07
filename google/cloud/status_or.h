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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_OR_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_OR_H

#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"
#include <type_traits>
#include <utility>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * Holds a value or a `Status` indicating why there is no value.
 *
 * `StatusOr<T>` represents either a usable `T` value or a `Status` object
 * explaining why a `T` value is not present. Typical usage of `StatusOr<T>`
 * looks like usage of a smart pointer, or even a std::optional<T>, in that you
 * first check its validity using a conversion to bool (or by calling
 * `StatusOr::ok()`), then you may dereference the object to access the
 * contained value. It is undefined behavior (UB) to dereference a
 * `StatusOr<T>` that is not "ok". For example:
 *
 * @code
 * StatusOr<Foo> foo = FetchFoo();
 * if (!foo) {  // Same as !foo.ok()
 *   // handle error and probably look at foo.status()
 * } else {
 *   foo->DoSomethingFooey();  // UB if !foo
 * }
 * @endcode
 *
 * Alternatively, you may call the `StatusOr::value()` member function,
 * which is defined to throw an exception if there is no `T` value, or crash
 * the program if exceptions are disabled. It is never UB to call
 * `.value()`.
 *
 * @code
 * StatusOr<Foo> foo = FetchFoo();
 * foo.value().DoSomethingFooey();  // May throw/crash if there is no value
 * @endcode
 *
 * Functions that can fail will often return a `StatusOr<T>` instead of
 * returning an error code and taking a `T` out-param, or rather than directly
 * returning the `T` and throwing an exception on error. StatusOr is used so
 * that callers can choose whether they want to explicitly check for errors,
 * crash the program, or throw exceptions. Since constructors do not have a
 * return value, they should be designed in such a way that they cannot fail by
 * moving the object's complex initialization logic into a separate factory
 * function that itself can return a `StatusOr<T>`. For example:
 *
 * @code
 * class Bar {
 *  public:
 *   Bar(Arg arg);
 *   ...
 * };
 * StatusOr<Bar> MakeBar() {
 *   ... complicated logic that might fail
 *   return Bar(std::move(arg));
 * }
 * @endcode
 *
 * `StatusOr<T>` supports equality comparisons if the underlying type `T` does.
 *
 * TODO(...) - the current implementation is fairly naive with respect to `T`,
 *   it is unlikely to work correctly for reference types, types without default
 *   constructors, arrays.
 *
 * @tparam T the type of the value.
 */
template <typename T>
class StatusOr final {
 public:
  /**
   * Initializes with an error status (UNKNOWN).
   *
   * TODO(#548) - currently storage::Status does not define the status codes,
   *     they are simply integers, usually HTTP status codes. We need to map to
   *     the well-defined set of status codes.
   */
  StatusOr() : StatusOr(Status(StatusCode::kUnknown, "default")) {}

  /**
   * Creates a new `StatusOr<T>` holding the error condition @p rhs.
   *
   * @par Post-conditions
   * `ok() == false` and `status() == rhs`.
   *
   * @param rhs the status to initialize the object.
   * @throws std::invalid_argument if `rhs.ok()`. If exceptions are disabled the
   *     program terminates via `google::cloud::Terminate()`
   */
  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(Status rhs) : status_(std::move(rhs)) {
    if (status_.ok()) {
      google::cloud::internal::ThrowInvalidArgument(__func__);
    }
  }

  /**
   * Assigns the given non-OK Status to this `StatusOr<T>`.
   *
   * @throws std::invalid_argument if `status.ok()`. If exceptions are disabled
   *     the program terminates via `google::cloud::Terminate()`
   */
  StatusOr& operator=(Status status) {
    *this = StatusOr(std::move(status));
    return *this;
  }

  // NOLINTNEXTLINE(performance-noexcept-move-constructor)
  StatusOr(StatusOr&& rhs) noexcept(noexcept(T(std::move(*rhs))))
      : status_(std::move(rhs.status_)) {
    if (status_.ok()) {
      new (&value_) T(std::move(*rhs));
    }
  }

  // NOLINTNEXTLINE(performance-noexcept-move-constructor)
  StatusOr& operator=(StatusOr&& rhs) noexcept(noexcept(T(std::move(*rhs)))) {
    // There may be shorter ways to express this, but this is fairly readable,
    // and should be reasonably efficient. Note that we must avoid destructing
    // the destination and/or default initializing it unless really needed.
    if (!ok()) {
      if (!rhs.ok()) {
        status_ = std::move(rhs.status_);
        return *this;
      }
      new (&value_) T(std::move(*rhs));
      status_ = Status();
      return *this;
    }
    if (!rhs.ok()) {
      value_.~T();
      status_ = std::move(rhs.status_);
      return *this;
    }
    **this = *std::move(rhs);
    status_ = Status();
    return *this;
  }

  StatusOr(StatusOr const& rhs) : status_(rhs.status_) {
    if (status_.ok()) {
      new (&value_) T(*rhs);
    }
  }

  StatusOr& operator=(StatusOr const& rhs) {
    // There may be shorter ways to express this, but this is fairly readable,
    // and should be reasonably efficient. Note that we must avoid destructing
    // the destination and/or default initializing it unless really needed.
    if (!ok()) {
      if (!rhs.ok()) {
        status_ = rhs.status_;
        return *this;
      }
      new (&value_) T(*rhs);
      status_ = rhs.status_;
      return *this;
    }
    if (!rhs.ok()) {
      value_.~T();
      status_ = rhs.status_;
      return *this;
    }
    **this = *rhs;
    status_ = rhs.status_;
    return *this;
  }

  ~StatusOr() {
    if (ok()) {
      value_.~T();
    }
  }

  /**
   * Assign a `T` (or anything convertible to `T`) into the `StatusOr`.
   */
  // Disable this assignment if U==StatusOr<T>. Well, really if U is a
  // cv-qualified version of StatusOr<T>, so we need to apply std::decay<> to
  // it first.
  template <typename U = T>
  typename std::enable_if<  // NOLINT(misc-unconventional-assign-operator)
      !std::is_same<StatusOr, typename std::decay<U>::type>::value,
      StatusOr>::type&
  operator=(U&& rhs) {
    // There may be shorter ways to express this, but this is fairly readable,
    // and should be reasonably efficient. Note that we must avoid destructing
    // the destination and/or default initializing it unless really needed.
    if (!ok()) {
      new (&value_) T(std::forward<U>(rhs));
      status_ = Status();
      return *this;
    }
    **this = std::forward<U>(rhs);
    status_ = Status();
    return *this;
  }

  /**
   * Creates a new `StatusOr<T>` holding the value @p rhs.
   *
   * @par Post-conditions
   * `ok() == true` and `value() == rhs`.
   *
   * @param rhs the value used to initialize the object.
   *
   * @throws only if `T`'s move constructor throws.
   */
  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(T&& rhs) { new (&value_) T(std::move(rhs)); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(T const& rhs) { new (&value_) T(rhs); }

  bool ok() const { return status_.ok(); }
  explicit operator bool() const { return status_.ok(); }

  //@{
  /**
   * @name Deference operators.
   *
   * @warning Using these operators when `ok() == false` results in undefined
   *     behavior.
   *
   * @return All these return a (properly ref and const-qualified) reference to
   *     the underlying value.
   */
  T& operator*() & { return value_; }

  T const& operator*() const& { return value_; }

  T&& operator*() && { return std::move(value_); }

  T const&& operator*() const&& { return std::move(value_); }
  //@}

  //@{
  /**
   * @name Member access operators.
   *
   * @warning Using these operators when `ok() == false` results in undefined
   *     behavior.
   *
   * @return All these return a (properly ref and const-qualified) pointer to
   *     the underlying value.
   */
  T* operator->() & { return &value_; }

  T const* operator->() const& { return &value_; }
  //@}

  //@{
  /**
   * @name Value accessors.
   *
   * @return All these member functions return a (properly ref and
   *     const-qualified) reference to the underlying value.
   *
   * @throws `RuntimeStatusError` with the contents of `status()` if the object
   *   does not contain a value, i.e., if `ok() == false`.
   */
  T& value() & {
    CheckHasValue();
    return **this;
  }

  T const& value() const& {
    CheckHasValue();
    return **this;
  }

  T&& value() && {
    CheckHasValue();
    return std::move(**this);
  }

  T const&& value() const&& {
    CheckHasValue();
    return std::move(**this);
  }
  //@}

  //@{
  /**
   * @name Status accessors.
   *
   * @return All these member functions return the (properly ref and
   *     const-qualified) status. If the object contains a value then
   *     `status().ok() == true`.
   */
  Status& status() & { return status_; }
  Status const& status() const& { return status_; }
  Status&& status() && { return std::move(status_); }
  Status const&& status() const&& { return std::move(status_); }
  //@}

 private:
  void CheckHasValue() const& {
    if (!ok()) {
      internal::ThrowStatus(status_);
    }
  }

  // When possible, do not copy the status.
  void CheckHasValue() && {
    if (!ok()) {
      internal::ThrowStatus(std::move(status_));
    }
  }

  Status status_;
  union {
    T value_;
  };
};

// Returns true IFF both `StatusOr<T>` objects hold an equal `Status` or an
// equal instance  of `T`. This function requires that `T` supports equality.
template <typename T>
bool operator==(StatusOr<T> const& a, StatusOr<T> const& b) {
  if (!a || !b) return a.status() == b.status();
  return *a == *b;
}

// Returns true of `a` and `b` are not equal. See `operator==` docs above for
// the definition of equal.
template <typename T>
bool operator!=(StatusOr<T> const& a, StatusOr<T> const& b) {
  return !(a == b);
}

template <typename T>
StatusOr<T> make_status_or(T rhs) {
  return StatusOr<T>(std::move(rhs));
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_OR_H
