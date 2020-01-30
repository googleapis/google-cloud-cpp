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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_RETRY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_RETRY_POLICY_H

#include "google/cloud/internal/backoff_policy.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/status.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/// Defines what error codes are permanent errors.
struct StatusTraits {
  static bool IsPermanentFailure(Status const& status) {
    return status.code() != StatusCode::kDeadlineExceeded &&
           status.code() != StatusCode::kInternal &&
           status.code() != StatusCode::kUnavailable;
  }
};
}  // namespace internal

/// The retry policy base class
using RetryPolicy =
    google::cloud::internal::RetryPolicy<Status, internal::StatusTraits>;

/// Keep retrying until some time has expired.
using LimitedTimeRetryPolicy =
    google::cloud::internal::LimitedTimeRetryPolicy<Status,
                                                    internal::StatusTraits>;

/// Keep retrying until the error count has been exceeded.
using LimitedErrorCountRetryPolicy =
    google::cloud::internal::LimitedErrorCountRetryPolicy<
        Status, internal::StatusTraits>;

/// The backoff policy base class.
using BackoffPolicy = google::cloud::internal::BackoffPolicy;

/// Implement truncated exponential backoff with randomization.
using ExponentialBackoffPolicy =
    google::cloud::internal::ExponentialBackoffPolicy;

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_RETRY_POLICY_H
