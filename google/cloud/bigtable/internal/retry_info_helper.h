// Copyright 2024 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_RETRY_INFO_HELPER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_RETRY_INFO_HELPER_H

#include "google/cloud/bigtable/version.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/retry_policy.h"
#include "google/cloud/status.h"
#include "absl/types/optional.h"
#include <chrono>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Returns the backoff given the status, retry policy, and backoff policy.
 *
 * Takes into account whether the server has returned a `RetryInfo` in the
 * status's error details.
 *
 * Returns absl::nullopt if no backoff should be performed.
 *
 * This function is responsible for calling `retry.OnFailure()`, which might,
 * for example, increment in error based retry policy.
 */
absl::optional<std::chrono::milliseconds> BackoffOrBreak(
    bool use_server_retry_info, Status const& status, RetryPolicy& retry,
    BackoffPolicy& backoff);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_RETRY_INFO_HELPER_H
