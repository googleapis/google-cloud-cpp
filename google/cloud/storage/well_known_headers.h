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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_HEADERS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_HEADERS_H_

#include "google/cloud/internal/optional.h"
#include "google/cloud/storage/version.h"
#include <cstdint>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
/**
 * Refactor definition of well-known request headers using the CRTP.
 *
 * @tparam H the type we will use to represent the header.
 * @tparam T the C++ type of the query parameter
 */
template <typename P, typename T>
class WellKnownHeader {
 public:
  WellKnownHeader() : value_{} {}
  explicit WellKnownHeader(T&& value) : value_(std::forward<T>(value)) {}

  char const* header_name() const { return P::well_known_parameter_name(); }
  bool has_value() const { return value_.has_value(); }
  T const& value() const { return value_.value(); }

 private:
  google::cloud::internal::optional<T> value_;
};

struct IfMatchEtag : public WellKnownHeader<IfMatchEtag, std::string> {
  using WellKnownHeader<IfMatchEtag, std::string>::WellKnownHeader;
  static char const* header_name() { return "If-Match"; }
};

struct IfNoneMatchEtag : public WellKnownHeader<IfNoneMatchEtag, std::string> {
  using WellKnownHeader<IfNoneMatchEtag, std::string>::WellKnownHeader;
  static char const* header_name() { return "If-None-Match"; }
};

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_WELL_KNOWN_HEADERS_H_
