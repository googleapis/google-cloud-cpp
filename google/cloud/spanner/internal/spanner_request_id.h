// Copyright 2026 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_REQUEST_ID_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_REQUEST_ID_H

#include "google/cloud/spanner/version.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Generates a 16-character zero-padded lowercase hex random ID from a 64-bit
/// random integer. On POSIX systems, re-generates the ID if a process fork is
/// detected. On non-POSIX systems (e.g. Windows), generates once per process
/// lifecycle.
std::string ProcessRandomId();

/// Returns the next sequential process-wide client ID (thread-safe).
std::uint64_t NextClientId();

/// Formats the static 3-field prefix: "<version>.<process_id>.<client_id>."
std::string FormatSpannerRequestStaticPrefix(std::uint32_t version,
                                             std::string_view process_random_id,
                                             std::uint64_t client_id);

/// Formats full request ID using the cached static prefix:
/// "<static_prefix><channel_id>.<request_index>.<attempt_index>"
std::string FormatSpannerRequestId(std::string_view static_prefix,
                                   std::uint32_t channel_id,
                                   std::uint64_t request_index,
                                   std::uint32_t attempt_index);

/// Convenience overload formatting all 6 fields:
/// "<version>.<process_id>.<client_id>.<channel_id>.<request_index>.<attempt_index>"
std::string FormatSpannerRequestId(std::uint32_t version,
                                   std::string_view process_random_id,
                                   std::uint64_t client_id,
                                   std::uint32_t channel_id,
                                   std::uint64_t request_index,
                                   std::uint32_t attempt_index);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_SPANNER_REQUEST_ID_H
