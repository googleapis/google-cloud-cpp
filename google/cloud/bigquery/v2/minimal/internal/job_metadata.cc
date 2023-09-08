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

#include "google/cloud/bigquery/v2/minimal/internal/job_metadata.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/api_client_header.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

BigQueryJobMetadata::BigQueryJobMetadata(
    std::shared_ptr<BigQueryJobRestStub> child)
    : child_(std::move(child)),
      api_client_header_(
          google::cloud::internal::HandCraftedLibClientHeader()) {}

StatusOr<GetJobResponse> BigQueryJobMetadata::GetJob(
    rest_internal::RestContext& context, GetJobRequest const& request) {
  SetMetadata(context);
  return child_->GetJob(context, request);
}

StatusOr<ListJobsResponse> BigQueryJobMetadata::ListJobs(
    rest_internal::RestContext& context, ListJobsRequest const& request) {
  SetMetadata(context);
  return child_->ListJobs(context, request);
}

StatusOr<InsertJobResponse> BigQueryJobMetadata::InsertJob(
    rest_internal::RestContext& context, InsertJobRequest const& request) {
  SetMetadata(context);
  return child_->InsertJob(context, request);
}

StatusOr<CancelJobResponse> BigQueryJobMetadata::CancelJob(
    rest_internal::RestContext& context, CancelJobRequest const& request) {
  SetMetadata(context);
  return child_->CancelJob(context, request);
}

StatusOr<QueryResponse> BigQueryJobMetadata::Query(
    rest_internal::RestContext& context, PostQueryRequest const& request) {
  SetMetadata(context);
  return child_->Query(context, request);
}

StatusOr<GetQueryResultsResponse> BigQueryJobMetadata::GetQueryResults(
    rest_internal::RestContext& context,
    GetQueryResultsRequest const& request) {
  SetMetadata(context);
  return child_->GetQueryResults(context, request);
}

void BigQueryJobMetadata::SetMetadata(rest_internal::RestContext& rest_context,
                                      std::vector<std::string> const& params) {
  rest_context.AddHeader("x-goog-api-client", api_client_header_);
  if (!params.empty()) {
    rest_context.AddHeader("x-goog-request-params", absl::StrJoin(params, "&"));
  }
  auto const& options = internal::CurrentOptions();
  if (options.has<UserProjectOption>()) {
    rest_context.AddHeader("x-goog-user-project",
                           options.get<UserProjectOption>());
  }
  if (options.has<google::cloud::QuotaUserOption>()) {
    rest_context.AddHeader("x-goog-quota-user",
                           options.get<google::cloud::QuotaUserOption>());
  }
  if (options.has<google::cloud::ServerTimeoutOption>()) {
    auto ms_rep = absl::StrCat(
        absl::Dec(options.get<google::cloud::ServerTimeoutOption>().count(),
                  absl::kZeroPad4));
    rest_context.AddHeader("x-server-timeout",
                           ms_rep.insert(ms_rep.size() - 3, "."));
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
