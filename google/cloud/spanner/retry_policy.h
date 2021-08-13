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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_RETRY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_RETRY_POLICY_H

#include "google/cloud/spanner/internal/status_utils.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/status.h"
#include "absl/strings/match.h"

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

/// Define the gRPC status code semantics for retrying requests.
struct SafeGrpcRetry {
  static inline bool IsOk(google::cloud::Status const& status) {
    return status.ok();
  }
  static inline bool IsTransientFailure(google::cloud::Status const& status) {
    if (status.code() == StatusCode::kUnavailable ||
        status.code() == StatusCode::kResourceExhausted) {
      return true;
    }
    // Treat the unexpected termination of the gRPC connection as retryable.
    // There is no explicit indication of this, but it will result in an
    // INTERNAL status with one of the `kTransientFailureMessages`.
    if (status.code() == StatusCode::kInternal) {
      constexpr char const* kTransientFailureMessages[] = {
          "Received unexpected EOS on DATA frame from server",
          "Connection closed with unknown cause",
          "HTTP/2 error code: INTERNAL_ERROR", "RST_STREAM"};
      for (auto const& message : kTransientFailureMessages) {
        if (absl::StrContains(status.message(), message)) return true;
      }
    }
    return false;
  }
  static inline bool IsPermanentFailure(google::cloud::Status const& status) {
    return !IsOk(status) && !IsTransientFailure(status);
  }
};

/// Define the gRPC status code semantics for rerunning transactions.
struct SafeTransactionRerun {
  static inline bool IsOk(google::cloud::Status const& status) {
    return status.ok();
  }
  static inline bool IsTransientFailure(google::cloud::Status const& status) {
    return status.code() == StatusCode::kAborted || IsSessionNotFound(status);
  }
  static inline bool IsPermanentFailure(google::cloud::Status const& status) {
    return !IsOk(status) && !IsTransientFailure(status);
  }
};
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/// The base class for retry policies.
using RetryPolicy = ::google::cloud::internal::TraitBasedRetryPolicy<
    spanner_internal::SafeGrpcRetry>;

/// A retry policy that limits based on time.
using LimitedTimeRetryPolicy =
    ::google::cloud::internal::LimitedTimeRetryPolicy<
        spanner_internal::SafeGrpcRetry>;

/// A retry policy that limits the number of times a request can fail.
using LimitedErrorCountRetryPolicy =
    google::cloud::internal::LimitedErrorCountRetryPolicy<
        spanner_internal::SafeGrpcRetry>;

/// The base class for transaction rerun policies.
using TransactionRerunPolicy = ::google::cloud::internal::TraitBasedRetryPolicy<
    spanner_internal::SafeTransactionRerun>;

/// A transaction rerun policy that limits the duration of the rerun loop.
using LimitedTimeTransactionRerunPolicy =
    google::cloud::internal::LimitedTimeRetryPolicy<
        spanner_internal::SafeTransactionRerun>;

/// A transaction rerun policy that limits the number of failures.
using LimitedErrorCountTransactionRerunPolicy =
    google::cloud::internal::LimitedErrorCountRetryPolicy<
        spanner_internal::SafeTransactionRerun>;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_RETRY_POLICY_H
