// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_WELL_KNOWN_PARAMETERS_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_WELL_KNOWN_PARAMETERS_IMPL_H

#include "google/cloud/internal/ios_flags_saver.h"
#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

/**
 * Defines well-known request headers using the CRTP.
 *
 * @tparam P the type we will use to represent the query parameter.
 * @tparam T the C++ type of the query parameter
 */
template <typename P, typename T>
class WellKnownParameter {
 public:
  WellKnownParameter() = default;
  explicit WellKnownParameter(T&& value) : value_(std::forward<T>(value)) {}
  explicit WellKnownParameter(T const& value) : value_(value) {}

  char const* parameter_name() const { return P::well_known_parameter_name(); }
  bool has_value() const { return value_.has_value(); }
  T const& value() const { return value_.value(); }
  template <typename U>
  T value_or(U&& default_val) const {
    return value_.value_or(std::forward<U>(default_val));
  }

 private:
  absl::optional<T> value_;
};

template <typename P, typename T>
std::ostream& operator<<(std::ostream& os,
                         WellKnownParameter<P, T> const& rhs) {
  if (rhs.has_value()) {
    return os << rhs.parameter_name() << "=" << rhs.value();
  }
  return os << rhs.parameter_name() << "=<not set>";
}

template <typename P>
std::ostream& operator<<(std::ostream& os,
                         WellKnownParameter<P, bool> const& rhs) {
  if (rhs.has_value()) {
    google::cloud::internal::IosFlagsSaver saver(os);
    return os << rhs.parameter_name() << "=" << std::boolalpha << rhs.value();
  }
  return os << rhs.parameter_name() << "=<not set>";
}

}  // namespace internal

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_WELL_KNOWN_PARAMETERS_IMPL_H
