// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_RETRY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_RETRY_POLICY_H

#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

namespace internal {
/// Define the gRPC status code semantics for retrying requests.
struct SafeGrpcRetry {
  static inline bool IsOk(google::cloud::Status const& status) {
    return status.ok();
  }
  static inline bool IsTransientFailure(google::cloud::Status const& status) {
    return status.code() == StatusCode::kUnavailable ||
           status.code() == StatusCode::kResourceExhausted;
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

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_RETRY_POLICY_H
