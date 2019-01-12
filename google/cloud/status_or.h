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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_OR_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_OR_H_

#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/status.h"
#include <type_traits>
#include <utility>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * `StatusOr<T>` is used to return a value or an error status.
 *
 * If the library is compiled with exceptions disabled there is no standard C++
 * mechanism to report errors. In that case we use this class to wrap the values
 * returned to the application.
 *
 * The application typically calls an API in the library and must check the
 * returned error status:
 *
 * @code
 * namespace gcs = google::cloud::storage;
 * void AppCode(gcs::noex::Client client) {
 *   gcs::StatusOr<gcs::BucketMetadata> meta_err = client.GetBucketMetadata(
 *       "my-bucket-name");
 *   if (not meta_err) {
 *       std::cerr << "Error in GetBucketMetadata: " << meta_err.status()
 *                 << std::endl;
 *       return;
 *   }
 *   gcs::BucketMetadata meta = *meta_err;
 *   // Do useful work here.
 * }
 * @endcode
 *
 * Note that the storage client retries most requests for you, resending the
 * request after an error is probably not useful. You should consider changing
 * the retry policies instead.
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

  StatusOr(StatusOr&& rhs) : status_(std::move(rhs.status_)) {
    if (status_.ok()) {
      new (&value_) T(std::move(*rhs));
    }
  }

  StatusOr& operator=(StatusOr&& rhs) {
    // There may be shorter ways to express this, but this is fairly readable,
    // and should be reasonably efficient. Note that we must avoid destructing
    // the destination and/or default initializing it unless really needed.
    if (not ok()) {
      if (not rhs.ok()) {
        status_ = std::move(rhs.status_);
        return *this;
      }
      new (&value_) T(std::move(*rhs));
      status_ = Status();
      return *this;
    }
    if (not rhs.ok()) {
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
    if (not ok()) {
      if (not rhs.ok()) {
        status_ = rhs.status_;
        return *this;
      }
      new (&value_) T(*rhs);
      status_ = rhs.status_;
      return *this;
    }
    if (not rhs.ok()) {
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
  typename std::enable_if<
      not std::is_same<StatusOr, typename std::decay<U>::type>::value,
      StatusOr>::type&
  operator=(U&& rhs) {
    // There may be shorter ways to express this, but this is fairly readable,
    // and should be reasonably efficient. Note that we must avoid destructing
    // the destination and/or default initializing it unless really needed.
    if (not ok()) {
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
  StatusOr(T&& rhs) : status_() { new (&value_) T(std::move(rhs)); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(T const& rhs) : status_() { new (&value_) T(rhs); }

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

#if GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
  T const&& operator*() const&& { return std::move(value_); }
#endif  // GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
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

#if GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
  T const&& value() const&& {
    CheckHasValue();
    return std::move(**this);
  }
#endif  // GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
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
#if GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
  Status const&& status() const&& { return std::move(status_); }
#endif  // GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
        //@}

 private:
  void CheckHasValue() const& {
    if (not ok()) {
      internal::ThrowStatus(status_);
    }
  }

  // When possible, do not copy the status.
  void CheckHasValue() && {
    if (not ok()) {
      internal::ThrowStatus(std::move(status_));
    }
  }

  Status status_;
  union {
    T value_;
  };
};

/**
 * `StatusOr<void>` is used to return an error or nothing.
 *
 * `StatusOr<T>` does not work for `T = void` because some of the member
 * functions (`StatusOr<T>(T)`) make no sense for `void`. Likewise, the class
 * cannot contain an object of type `void` because there is no such thing.
 *
 * The application typically calls an API in the library and must check the
 * returned error status:
 *
 * @code
 * namespace gcs = google::cloud::storage;
 * void AppCode(gcs::noex::Client client) {
 *   gcs::StatusOr<void> delete_err = client.DeleteBucket("my-bucket-name");
 *   if (not delete_err.ok()) {
 *       std::cerr << "Error in DeleteBucket: " << meta_err.status()
 *                 << std::endl;
 *       return;
 *   }
 *   // Do useful work here.
 * }
 * @endcode
 *
 * Note that the storage client retries most requests for you, resending the
 * request after an error is probably not useful. You should consider changing
 * the retry policies instead.
 */
template <>
class StatusOr<void> final {
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
   * Creates a new `StatusOr<void>` holding the status @p rhs.
   *
   * When `rhs.ok() == true` the object is treated as if it held a `void` value.
   *
   * @param rhs the status to initialize the object.
   */
  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(Status rhs) : status_(std::move(rhs)) {}

  bool ok() const { return status_.ok(); }
  explicit operator bool() const { return ok(); }

  //@{
  /**
   * @name Deference operators.
   *
   * These are provided mostly so generic code can use `StatusOr<void>` just
   * like `StatusOr<T>`.
   */
  void operator*() & {}
  void operator*() const& {}
  void operator*() && {}
#if GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
  void operator*() const&& {}
#endif  // GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
  //@}

  //@{
  /**
   * @name Member access operators.
   *
   * These are provided mostly so generic code can use `StatusOr<void>` just
   * like `StatusOr<T>`.
   */
  void* operator->() & { return nullptr; }
  void const* operator->() const& { return nullptr; }
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
  void value() & { CheckHasValue(); }

  void value() const& { CheckHasValue(); }

  void value() && { CheckHasValue(); }

#if GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
  void value() const&& { CheckHasValue(); }
#endif  // GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
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
#if GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
  Status const&& status() const&& { return std::move(status_); }
#endif  // GOOGLE_CLOUD_CPP_HAVE_CONST_REF_REF
        //@}

 private:
  void CheckHasValue() const& {
    if (not ok()) {
      internal::ThrowStatus(Status(status_));
    }
  }

  // When possible, do not copy the status.
  void CheckHasValue() && {
    if (not ok()) {
      internal::ThrowStatus(std::move(status_));
    }
  }

  Status status_;
};

template <typename T>
StatusOr<T> make_status_or(T rhs) {
  return StatusOr<T>(std::move(rhs));
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STATUS_OR_H_
