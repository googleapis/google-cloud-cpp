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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPTIONAL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPTIONAL_H_

#include "google/cloud/internal/throw_delegate.h"
#include <type_traits>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
/**
 * A poor's man version of std::optional<T>.
 *
 * This project needs to support C++11 and C++14, so std::optional<> is not
 * available.  We cannot use Abseil either, see #232 for the reasons.
 * So we implement a very minimal "optional" class that documents the intent
 * and we will remove it when possible.
 *
 * @warning this is not a drop-in replacement for std::optional<>, it has at
 *   least the following defects:
 *   - it raises std::logic_error instead of std::bad_optional_access.
 *   - emplace() is a very simple version.
 *   - It does not have the full complement of assignment operators and
 *     constructors required by the standard.
 *   - It lacks operator->() and operator*()
 *   - It lacks comparison operators.
 *   - No nullopt_t.
 *   - No std::swap(), std::make_optional(), or std::hash().
 *
 *
 * TODO() - replace with absl::optional<> or std::optional<> when possible.
 */
template <typename T>
class optional {
 public:
  optional() : buffer_{}, has_value_(false) {}
  optional(T const& x) : has_value_(true) {
    new (reinterpret_cast<T*>(&buffer_)) T(x);
  }
  optional(T&& x) : has_value_(true) {
    new (reinterpret_cast<T*>(&buffer_)) T(std::move(x));
  }

  T& value() & {
    check_access();
    return *reinterpret_cast<T*>(&buffer_);
  }
  T const& value() const& {
    check_access();
    return *reinterpret_cast<T const*>(&buffer_);
  }
  T&& value() && {
    check_access();
    return *reinterpret_cast<T*>(&buffer_);
  }
  T const&& value() const&& {
    check_access();
    return *reinterpret_cast<T const*>(&buffer_);
  }

  template <typename U>
  constexpr T value_or(U&& default_value) const& {
    return bool(*this) ? **this
                       : static_cast<T>(std::forward<U>(default_value));
  }

  template <typename U>
  T value_or(U&& default_value) && {
    return bool(*this) ? std::move(**this)
                       : static_cast<T>(std::forward<U>(default_value));
  }

  operator bool() const { return has_value_; }
  bool has_value() const { return has_value_; }

  void reset() {
    if (has_value_) {
      reinterpret_cast<T*>(&buffer_)->~T();
      has_value_ = false;
    }
  }
  T& emplace(T&& value) {
    reset();
    *reinterpret_cast<T*>(&buffer_) = std::move(value);
    return **this;
  }

 private:
  void check_access() const {
    if (has_value_) {
      return;
    }
    google::cloud::internal::RaiseLogicError("access unset optional");
  }

 private:
  // We use std::aligned_storage<T> because T may not have a default
  // constructor, if we used 'T' here we could not default initialize this class
  // either.
  using aligned_storage_t = std::aligned_storage<sizeof(T), alignof(T)>;
  typename aligned_storage_t::type buffer_;
  bool has_value_;
};

}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_INTERNAL_OPTIONAL_H_
