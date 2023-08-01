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

#include "google/cloud/bigquery/v2/minimal/internal/job_connection.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_rest_connection_impl.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_rest_stub_factory.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

BigQueryJobConnection::~BigQueryJobConnection() = default;

StatusOr<Job> BigQueryJobConnection::GetJob(GetJobRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StreamRange<ListFormatJob> BigQueryJobConnection::ListJobs(
    ListJobsRequest const&) {
  return google::cloud::internal::MakeStreamRange<ListFormatJob>(
      []() -> absl::variant<Status, ListFormatJob> {
        return Status(StatusCode::kUnimplemented, "not implemented");
      });
}

StatusOr<Job> BigQueryJobConnection::InsertJob(InsertJobRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<Job> BigQueryJobConnection::CancelJob(CancelJobRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<PostQueryResults> BigQueryJobConnection::Query(
    PostQueryRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<GetQueryResults> BigQueryJobConnection::QueryResults(
    GetQueryResultsRequest const&) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

std::shared_ptr<BigQueryJobConnection> MakeBigQueryJobConnection(
    Options options) {
  internal::CheckExpectedOptions<CommonOptionList, UnifiedCredentialsOptionList,
                                 BigQueryJobPolicyOptionList>(options,
                                                              __func__);
  options = BigQueryJobDefaultOptions(std::move(options));

  auto job_rest_stub = CreateDefaultBigQueryJobRestStub(options);

  return std::make_shared<BigQueryJobRestConnectionImpl>(
      std::move(job_rest_stub), std::move(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
