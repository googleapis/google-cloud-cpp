// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_RETRY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_RETRY_POLICY_H

#include "google/cloud/pubsub/version.h"
#include "google/cloud/internal/retry_policy.h"
#include "google/cloud/status.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
/// Define the gRPC status code semantics for retrying requests.
struct RetryTraits {
  static inline bool IsPermanentFailure(google::cloud::Status const& status) {
    return status.code() != StatusCode::kOk &&
           status.code() != StatusCode::kAborted &&
           status.code() != StatusCode::kInternal &&
           status.code() != StatusCode::kUnavailable &&
           status.code() != StatusCode::kResourceExhausted;
  }
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal

namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// The base class for retry policies.
using RetryPolicy = ::google::cloud::internal::TraitBasedRetryPolicy<
    pubsub_internal::RetryTraits>;

/// A retry policy that limits based on time.
using LimitedTimeRetryPolicy =
    ::google::cloud::internal::LimitedTimeRetryPolicy<
        pubsub_internal::RetryTraits>;

/// A retry policy that limits the number of times a request can fail.
using LimitedErrorCountRetryPolicy =
    google::cloud::internal::LimitedErrorCountRetryPolicy<
        pubsub_internal::RetryTraits>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_RETRY_POLICY_H
