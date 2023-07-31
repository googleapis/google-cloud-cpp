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

#include "google/cloud/bigquery/v2/minimal/internal/job_logging.h"
#include "google/cloud/bigquery/v2/minimal/internal/log_wrapper.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

BigQueryJobLogging::BigQueryJobLogging(
    std::shared_ptr<BigQueryJobRestStub> child, TracingOptions tracing_options,
    std::set<std::string> components)
    : child_(std::move(child)),
      tracing_options_(std::move(tracing_options)),
      components_(std::move(components)) {}

StatusOr<GetJobResponse> BigQueryJobLogging::GetJob(
    rest_internal::RestContext& rest_context, GetJobRequest const& request) {
  return LogWrapper(
      [this](rest_internal::RestContext& rest_context,
             GetJobRequest const& request) {
        return child_->GetJob(rest_context, request);
      },
      rest_context, request, __func__,
      "google.cloud.bigquery.v2.minimal.internal.GetJobRequest",
      "google.cloud.bigquery.v2.minimal.internal.GetJobResponse",
      tracing_options_);
}

StatusOr<ListJobsResponse> BigQueryJobLogging::ListJobs(
    rest_internal::RestContext& rest_context, ListJobsRequest const& request) {
  return LogWrapper(
      [this](rest_internal::RestContext& rest_context,
             ListJobsRequest const& request) {
        return child_->ListJobs(rest_context, request);
      },
      rest_context, request, __func__,
      "google.cloud.bigquery.v2.minimal.internal.ListJobsRequest",
      "google.cloud.bigquery.v2.minimal.internal.ListJobsResponse",
      tracing_options_);
}

StatusOr<InsertJobResponse> BigQueryJobLogging::InsertJob(
    rest_internal::RestContext& rest_context, InsertJobRequest const& request) {
  return LogWrapper(
      [this](rest_internal::RestContext& rest_context,
             InsertJobRequest const& request) {
        return child_->InsertJob(rest_context, request);
      },
      rest_context, request, __func__,
      "google.cloud.bigquery.v2.minimal.internal.InsertJobRequest",
      "google.cloud.bigquery.v2.minimal.internal.InsertJobResponse",
      tracing_options_);
}

StatusOr<CancelJobResponse> BigQueryJobLogging::CancelJob(
    rest_internal::RestContext& rest_context, CancelJobRequest const& request) {
  return LogWrapper(
      [this](rest_internal::RestContext& rest_context,
             CancelJobRequest const& request) {
        return child_->CancelJob(rest_context, request);
      },
      rest_context, request, __func__,
      "google.cloud.bigquery.v2.minimal.internal.CancelJobRequest",
      "google.cloud.bigquery.v2.minimal.internal.CancelJobResponse",
      tracing_options_);
}

StatusOr<QueryResponse> BigQueryJobLogging::Query(
    rest_internal::RestContext& rest_context, PostQueryRequest const& request) {
  return LogWrapper(
      [this](rest_internal::RestContext& rest_context,
             PostQueryRequest const& request) {
        return child_->Query(rest_context, request);
      },
      rest_context, request, __func__,
      "google.cloud.bigquery.v2.minimal.internal.PostQueryRequest",
      "google.cloud.bigquery.v2.minimal.internal.QueryResponse",
      tracing_options_);
}

StatusOr<GetQueryResultsResponse> BigQueryJobLogging::GetQueryResults(
    rest_internal::RestContext& rest_context,
    GetQueryResultsRequest const& request) {
  return LogWrapper(
      [this](rest_internal::RestContext& rest_context,
             GetQueryResultsRequest const& request) {
        return child_->GetQueryResults(rest_context, request);
      },
      rest_context, request, __func__,
      "google.cloud.bigquery.v2.minimal.internal.GetQueryResultsRequest",
      "google.cloud.bigquery.v2.minimal.internal.GetQueryResultsResponse",
      tracing_options_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
