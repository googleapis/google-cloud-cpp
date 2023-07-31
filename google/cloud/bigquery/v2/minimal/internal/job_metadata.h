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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_METADATA_H

#include "google/cloud/bigquery/v2/minimal/internal/job_rest_stub.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/version.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class BigQueryJobMetadata : public BigQueryJobRestStub {
 public:
  ~BigQueryJobMetadata() override = default;
  explicit BigQueryJobMetadata(std::shared_ptr<BigQueryJobRestStub> child);

  StatusOr<GetJobResponse> GetJob(rest_internal::RestContext& context,
                                  GetJobRequest const& request) override;
  StatusOr<ListJobsResponse> ListJobs(rest_internal::RestContext& context,
                                      ListJobsRequest const& request) override;

  StatusOr<InsertJobResponse> InsertJob(
      rest_internal::RestContext& rest_context,
      InsertJobRequest const& request) override;
  StatusOr<CancelJobResponse> CancelJob(
      rest_internal::RestContext& rest_context,
      CancelJobRequest const& request) override;

  StatusOr<QueryResponse> Query(rest_internal::RestContext& rest_context,
                                PostQueryRequest const& request) override;

  StatusOr<GetQueryResultsResponse> GetQueryResults(
      rest_internal::RestContext& rest_context,
      GetQueryResultsRequest const& request) override;

 private:
  void SetMetadata(rest_internal::RestContext& context,
                   std::vector<std::string> const& params = {});

  std::shared_ptr<BigQueryJobRestStub> child_;
  std::string api_client_header_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_JOB_METADATA_H
