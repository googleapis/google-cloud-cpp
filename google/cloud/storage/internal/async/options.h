// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPTIONS_H

#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <cstdint>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// Configuration for a single read range to be pre-warmed.
struct ReadRangeConfig {
  std::int64_t offset;
  std::int64_t length;
};

/// Internal option to pass the list of ranges to pre-warm from `AsyncClient` to
/// `Connection`.
struct ReadRangesOption {
  using Type = std::vector<ReadRangeConfig>;
  static char const* name() {
    return "google::cloud::storage::ReadRangesOption";
  }
};

/// Internal option to override the pacing buffer limit (in bytes) for
/// pre-warmed ranges. Primarily used in unit tests to test pacing limits
/// without generating large payloads.
struct PreWarmBufferLimitOption {
  using Type = std::size_t;
  static char const* name() {
    return "google::cloud::storage::PreWarmBufferLimitOption";
  }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_ASYNC_OPTIONS_H
