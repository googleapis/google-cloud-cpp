// Copyright 2023 Google LLC
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

// Internal interface for Bigquery V2 Job resource.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_RETRY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_RETRY_POLICY_H

#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/status.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

// Rest status code semantics for retrying requests.
struct BigQueryJobRetryTraits {
  static inline bool IsPermanentFailure(google::cloud::Status const& status) {
    return status.code() != StatusCode::kDeadlineExceeded &&
           status.code() != StatusCode::kResourceExhausted &&
           status.code() != StatusCode::kUnavailable;
  }
};

using BigQueryJobRetryPolicy =
    ::google::cloud::internal::TraitBasedRetryPolicy<BigQueryJobRetryTraits>;

using BigQueryJobLimitedTimeRetryPolicy =
    ::google::cloud::internal::LimitedTimeRetryPolicy<BigQueryJobRetryTraits>;

using BigQueryJobLimitedErrorCountRetryPolicy =
    ::google::cloud::internal::LimitedErrorCountRetryPolicy<
        BigQueryJobRetryTraits>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_RETRY_POLICY_H
