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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMPLEX_OPTION_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMPLEX_OPTION_H_

#include "google/cloud/optional.h"
#include "google/cloud/storage/version.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * A complex option is a request optional parameter that is neither a header
 * nor a query parameter.
 *
 * The majority of the request options either change a header (or group of
 * headers) or they set a query parameter. They are modeled using
 * `WellKnownParameter` or `WellKnownHeader`. A few options do neither, they
 * affect how the query is performed. Notably, we provide options where the
 * user can provide pre-computed values for the MD5 hash and CRC32C values of
 * an upload or download.
 *
 * @tparam Derived the type we will use to represent the option.
 * @tparam T the C++ type of the option value.
 */
template <typename Derived, typename T>
class ComplexOption {
 public:
  ComplexOption() : value_{} {}
  explicit ComplexOption(T value) : value_(std::move(value)) {}

  char const* option_name() const { return Derived::name(); }
  bool has_value() const { return value_.has_value(); }
  T const& value() const { return value_.value(); }

 private:
  google::cloud::optional<T> value_;
};

template <typename Derived, typename T>
std::ostream& operator<<(std::ostream& os,
                         ComplexOption<Derived, T> const& rhs) {
  if (rhs.has_value()) {
    return os << rhs.option_name() << "=" << rhs.value();
  }
  return os << rhs.option_name() << "=<not set>";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMPLEX_OPTION_H_
