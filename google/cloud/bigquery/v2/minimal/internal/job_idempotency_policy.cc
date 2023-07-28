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

// Implementation of internal interface for Bigquery V2 Job resource.

#include "google/cloud/bigquery/v2/minimal/internal/job_idempotency_policy.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

using ::google::cloud::Idempotency;

BigQueryJobIdempotencyPolicy::~BigQueryJobIdempotencyPolicy() = default;

std::unique_ptr<BigQueryJobIdempotencyPolicy>
BigQueryJobIdempotencyPolicy::clone() const {
  return std::make_unique<BigQueryJobIdempotencyPolicy>(*this);
}

Idempotency BigQueryJobIdempotencyPolicy::GetJob(GetJobRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency BigQueryJobIdempotencyPolicy::ListJobs(ListJobsRequest const&) {
  return Idempotency::kIdempotent;
}

Idempotency BigQueryJobIdempotencyPolicy::InsertJob(InsertJobRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency BigQueryJobIdempotencyPolicy::CancelJob(CancelJobRequest const&) {
  return Idempotency::kNonIdempotent;
}

Idempotency BigQueryJobIdempotencyPolicy::Query(
    PostQueryRequest const& request) {
  // Query requests containing request_id maybe considered idempotent.
  // Please see the rules here:
  // https://cloud.google.com/bigquery/docs/reference/rest/v2/jobs/query#queryrequest
  if (!request.query_request().request_id().empty()) {
    return Idempotency::kIdempotent;
  }

  return Idempotency::kNonIdempotent;
}

Idempotency BigQueryJobIdempotencyPolicy::GetQueryResults(
    GetQueryResultsRequest const&) {
  return Idempotency::kIdempotent;
}

std::unique_ptr<BigQueryJobIdempotencyPolicy>
MakeDefaultBigQueryJobIdempotencyPolicy() {
  return std::make_unique<BigQueryJobIdempotencyPolicy>();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
