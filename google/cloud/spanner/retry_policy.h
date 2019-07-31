// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_RETRY_POLICY_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_RETRY_POLICY_H_

#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/status.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
/// Define the gRPC status code semantics for retrying requests.
struct SafeGrpcRetry {
  static inline bool IsTransientFailure(google::cloud::StatusCode code) {
    return code == StatusCode::kAborted || code == StatusCode::kUnavailable ||
           code == StatusCode::kDeadlineExceeded;
  }

  static inline bool IsOk(google::cloud::Status const& status) {
    return status.ok();
  }
  static inline bool IsTransientFailure(google::cloud::Status const& status) {
    return IsTransientFailure(status.code());
  }
  static inline bool IsPermanentFailure(google::cloud::Status const& status) {
    return !IsOk(status) && !IsTransientFailure(status);
  }
};
}  // namespace internal

/// The base class for retry policies.
using RetryPolicy =
    google::cloud::internal::RetryPolicy<google::cloud::Status,
                                         internal::SafeGrpcRetry>;

/// A retry policy that limits based on time.
using LimitedTimeRetryPolicy =
    google::cloud::internal::LimitedTimeRetryPolicy<google::cloud::Status,
                                                    internal::SafeGrpcRetry>;

/// A retry policy that limits the number of times a request can fail.
using LimitedErrorCountRetryPolicy =
    google::cloud::internal::LimitedErrorCountRetryPolicy<
        google::cloud::Status, internal::SafeGrpcRetry>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_RETRY_POLICY_H_
