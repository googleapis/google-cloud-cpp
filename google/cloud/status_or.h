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
#include "absl/types/variant.h"
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
   * A `value_type` member for use in generic programming.
   *
   * This is analogous to that of `std::optional::value_type`.
   */
  using value_type = T;

  /**
   * Initializes with an error status (UNKNOWN).
   */
  StatusOr() : StatusOr(Status(StatusCode::kUnknown, "default")) {}

  StatusOr(StatusOr const&) = default;
  StatusOr& operator=(StatusOr const&) = default;
  // NOLINTNEXTLINE(performance-noexcept-move-constructor)
  StatusOr(StatusOr&&) = default;
  // NOLINTNEXTLINE(performance-noexcept-move-constructor)
  StatusOr& operator=(StatusOr&&) = default;

  /**
   * Creates a new `StatusOr<T>` holding the error condition @p status.
   *
   * @par Post-conditions
   * `ok() == false` and `status() == status`.
   *
   * @param status the status to initialize the object.
   * @throws std::invalid_argument if `status.ok()`. If exceptions are disabled
   * the program terminates via `google::cloud::Terminate()`
   */
  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(Status status) : v_(std::move(status)) {
    if (absl::get<Status>(v_).ok())
      google::cloud::internal::ThrowInvalidArgument(__func__);
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
  operator=(U&& u) {
    v_.template emplace<T>(std::forward<U>(u));
    return *this;
  }

  /**
   * Creates a new `StatusOr<T>` holding the value @p value.
   *
   * @par Post-conditions
   * `ok() == true` and `value() == value`.
   *
   * @param value the value used to initialize the object.
   *
   * @throws only if `T`'s move constructor throws.
   */
  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(T&& value) : v_(std::move(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(T const& value) : v_(value) {}

  bool ok() const { return absl::holds_alternative<T>(v_); }
  explicit operator bool() const { return ok(); }

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
  T& operator*() & { return absl::get<T>(v_); }
  T const& operator*() const& { return absl::get<T>(v_); }
  T&& operator*() && { return absl::get<T>(std::move(v_)); }
  T const&& operator*() const&& { return absl::get<T>(std::move(v_)); }
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
  T* operator->() & { return &**this; }
  T const* operator->() const& { return &**this; }
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
   * @return A reference to the contained `Status`.
   */
  Status const& status() const& {
    static auto const* const kOk = new Status{};
    if (ok()) return *kOk;
    return absl::get<Status>(v_);
  }
  Status&& status() && {
    // OOPS: this violates the class invariant (documented below), because we
    // need to return an rvalue reference to some Status, even if that Status
    // is OK. So we do that by writing OK into v_ and returning an rvalue
    // reference to it. This is OK, because this is an rvalue-qualified member
    // function anyway. However, it's also kinda not good because an object's
    // moved-from state ideally should be "valid", and this moved-from state
    // would violate a class in variant by holding a Status that is OK.
    if (ok()) v_ = Status{};
    return absl::get<Status>(std::move(v_));
  }
  //@}

 private:
  void CheckHasValue() const& {
    if (!ok()) internal::ThrowStatus(status());
  }
  void CheckHasValue() && {
    if (!ok()) internal::ThrowStatus(std::move(*this).status());
  }

  // Class invariants:
  // 1. v_ holds a T IFF Status == OK
  // 2. v_ holds a Status IFF Status != OK
  absl::variant<Status, T> v_;
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
