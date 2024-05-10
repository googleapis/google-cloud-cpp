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

#include "google/cloud/internal/parse_rfc3339.h"
#include "google/cloud/internal/make_status.h"
#include "absl/time/time.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
StatusOr<std::chrono::system_clock::time_point> ParseRfc3339(
    std::string const& timestamp) {
  std::string err;
  absl::Time t;
  if (!absl::ParseTime(absl::RFC3339_full, timestamp, &t, &err)) {
    return internal::InvalidArgumentError(
        "Error parsing RFC-3339 timestamp: '" + timestamp + "': " + err,
        GCP_ERROR_INFO());
  }
  return absl::ToChronoTime(t);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
