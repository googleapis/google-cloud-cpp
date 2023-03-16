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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_OPTIONS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_OPTIONS_H

#include "google/cloud/bigquery/v2/minimal/internal/job_idempotency_policy.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_retry_policy.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/options.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Use with `google::cloud::Options` to configure the retry policy.
 */
struct BigQueryJobRetryPolicyOption {
  using Type = std::shared_ptr<BigQueryJobRetryPolicy>;
};

/**
 * Use with `google::cloud::Options` to configure the backoff policy.
 */
struct BigQueryJobBackoffPolicyOption {
  using Type = std::shared_ptr<BackoffPolicy>;
};

/**
 * Use with `google::cloud::Options` to configure which operations are retried.
 */
struct BigQueryJobIdempotencyPolicyOption {
  using Type = std::shared_ptr<BigQueryJobIdempotencyPolicy>;
};

/**
 * Use with `google::cloud::Options` to configure the connection pool size for
 * rest client.
 */
struct BigQueryJobConnectionPoolSizeOption {
  using Type = std::size_t;
};

/**
 *  The options applicable to BigQueryJob.
 */
using BigQueryJobPolicyOptionList =
    OptionList<BigQueryJobRetryPolicyOption, BigQueryJobBackoffPolicyOption,
               BigQueryJobIdempotencyPolicyOption,
               BigQueryJobConnectionPoolSizeOption>;

/**
 * Default options for
 */
Options BigQueryJobDefaultOptions(Options options);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_OPTIONS_H
