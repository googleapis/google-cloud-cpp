// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_CORS_ENTRY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_CORS_ENTRY_H

#include "google/cloud/storage/version.h"
#include "absl/types/optional.h"
#include <iosfwd>
#include <tuple>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * An entry in the CORS list.
 *
 * CORS (Cross-Origin Resource Sharing) is a mechanism to enable client-side
 * cross-origin requests. An entry in the configuration has a maximum age and a
 * list of allowed origin and methods, as well as a list of returned response
 * headers.
 *
 * @see https://en.wikipedia.org/wiki/Cross-origin_resource_sharing for general
 *     information on CORS.
 *
 * @see https://cloud.google.com/storage/docs/cross-origin for general
 *     information about CORS in the context of Google Cloud Storage.
 *
 * @see https://cloud.google.com/storage/docs/configuring-cors for information
 *     on how to set and troubleshoot CORS settings.
 */
struct CorsEntry {
  absl::optional<std::int64_t> max_age_seconds;
  std::vector<std::string> method;
  std::vector<std::string> origin;
  std::vector<std::string> response_header;
};

//@{
/// @name Comparison operators for CorsEntry.
inline bool operator==(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::tie(lhs.max_age_seconds, lhs.method, lhs.origin,
                  lhs.response_header) == std::tie(rhs.max_age_seconds,
                                                   rhs.method, rhs.origin,
                                                   rhs.response_header);
}

inline bool operator<(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::tie(lhs.max_age_seconds, lhs.method, lhs.origin,
                  lhs.response_header) < std::tie(rhs.max_age_seconds,
                                                  rhs.method, rhs.origin,
                                                  rhs.response_header);
}

inline bool operator!=(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(CorsEntry const& lhs, CorsEntry const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}
//@}

std::ostream& operator<<(std::ostream& os, CorsEntry const& rhs);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_BUCKET_CORS_ENTRY_H
