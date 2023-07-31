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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_IDEMPOTENCY_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_IDEMPOTENCY_POLICY_H

#include "google/cloud/bigquery/v2/minimal/internal/job_request.h"
#include "google/cloud/idempotency.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class BigQueryJobIdempotencyPolicy {
 public:
  virtual ~BigQueryJobIdempotencyPolicy();

  virtual std::unique_ptr<BigQueryJobIdempotencyPolicy> clone() const;

  virtual google::cloud::Idempotency GetJob(GetJobRequest const& request);

  virtual google::cloud::Idempotency ListJobs(ListJobsRequest const& request);

  virtual google::cloud::Idempotency InsertJob(InsertJobRequest const& request);

  virtual google::cloud::Idempotency CancelJob(CancelJobRequest const& request);

  virtual google::cloud::Idempotency Query(PostQueryRequest const& request);

  virtual google::cloud::Idempotency GetQueryResults(
      GetQueryResultsRequest const& request);
};

std::unique_ptr<BigQueryJobIdempotencyPolicy>
MakeDefaultBigQueryJobIdempotencyPolicy();

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_IDEMPOTENCY_POLICY_H
