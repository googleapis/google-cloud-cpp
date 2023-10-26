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

#include "google/cloud/bigquery/v2/minimal/internal/job_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/internal/rest_stub_utils.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status_or.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

BigQueryJobRestStub::~BigQueryJobRestStub() = default;

StatusOr<GetJobResponse> DefaultBigQueryJobRestStub::GetJob(
    rest_internal::RestContext& rest_context, GetJobRequest const& request) {
  // Prepare the RestRequest from GetJobRequest.
  auto rest_request = PrepareRestRequest<GetJobRequest>(rest_context, request);
  if (!rest_request) return rest_request.status();

  // Call the rest stub and parse the RestResponse.
  rest_internal::RestContext context;
  return ParseFromRestResponse<GetJobResponse>(
      rest_stub_->Get(context, std::move(*rest_request)));
}

StatusOr<ListJobsResponse> DefaultBigQueryJobRestStub::ListJobs(
    rest_internal::RestContext& rest_context, ListJobsRequest const& request) {
  // Prepare the RestRequest from ListJobsRequest.
  auto rest_request =
      PrepareRestRequest<ListJobsRequest>(rest_context, request);
  if (!rest_request) return rest_request.status();

  // Call the rest stub and parse the RestResponse.
  rest_internal::RestContext context;
  return ParseFromRestResponse<ListJobsResponse>(
      rest_stub_->Get(context, std::move(*rest_request)));
}

StatusOr<InsertJobResponse> DefaultBigQueryJobRestStub::InsertJob(
    rest_internal::RestContext& rest_context, InsertJobRequest const& request) {
  // Prepare the RestRequest from InsertJobRequest.
  // This  does the following:
  // 1) Sets the request url path.
  // 2) Adds any query parameters and headers.
  auto rest_request =
      PrepareRestRequest<InsertJobRequest>(rest_context, request);
  if (!rest_request) return rest_request.status();

  rest_request->AddHeader("Content-Type", "application/json");

  // 3) Get the request body as json payload.
  nlohmann::json json_payload;
  to_json(json_payload, request.job());

  // 4) Filter out any keys that are being requested to be removed.
  auto filtered_json = RemoveJsonKeysAndEmptyFields(json_payload.dump(),
                                                    request.json_filter_keys());

  // 5) Call the rest stub and parse the RestResponse.
  rest_internal::RestContext context;
  return ParseFromRestResponse<InsertJobResponse>(
      rest_stub_->Post(context, std::move(*rest_request),
                       {absl::MakeConstSpan(filtered_json.dump())}));
}

StatusOr<CancelJobResponse> DefaultBigQueryJobRestStub::CancelJob(
    rest_internal::RestContext& rest_context, CancelJobRequest const& request) {
  // Prepare the RestRequest from CancelJobRequest.
  auto rest_request =
      PrepareRestRequest<CancelJobRequest>(rest_context, request);
  if (!rest_request) return rest_request.status();

  // For cancel jobs, request body is empty:
  // https://cloud.google.com/bigquery/docs/reference/rest/v2/jobs/cancel#request-body
  absl::Span<char const> empty_span = absl::MakeConstSpan("");

  // Call the rest stub and parse the RestResponse.
  rest_internal::RestContext context;
  return ParseFromRestResponse<CancelJobResponse>(
      rest_stub_->Post(context, std::move(*rest_request), {empty_span}));
}

StatusOr<QueryResponse> DefaultBigQueryJobRestStub::Query(
    rest_internal::RestContext& rest_context, PostQueryRequest const& request) {
  // Prepare the RestRequest from PostQueryRequest.
  // This  does the following:
  // 1) Sets the request url path.
  // 2) Adds any query parameters and headers.
  auto rest_request =
      PrepareRestRequest<PostQueryRequest>(rest_context, request);
  if (!rest_request) return rest_request.status();

  rest_request->AddHeader("Content-Type", "application/json");

  // 3) Get the request body as json payload.
  nlohmann::json json_payload;
  to_json(json_payload, request.query_request());

  // 4) Filter out any keys that are being requested to be removed.
  auto filtered_json = RemoveJsonKeysAndEmptyFields(json_payload.dump(),
                                                    request.json_filter_keys());

  // 5) Call the rest stub and parse the RestResponse.
  rest_internal::RestContext context;
  return ParseFromRestResponse<QueryResponse>(
      rest_stub_->Post(context, std::move(*rest_request),
                       {absl::MakeConstSpan(filtered_json.dump())}));
}

StatusOr<GetQueryResultsResponse> DefaultBigQueryJobRestStub::GetQueryResults(
    rest_internal::RestContext& rest_context,
    GetQueryResultsRequest const& request) {
  // Prepare the RestRequest from GetQueryResultsRequest.
  auto rest_request =
      PrepareRestRequest<GetQueryResultsRequest>(rest_context, request);
  if (!rest_request) return rest_request.status();

  // Call the rest stub and parse the RestResponse.
  rest_internal::RestContext context;
  return ParseFromRestResponse<GetQueryResultsResponse>(
      rest_stub_->Get(context, std::move(*rest_request)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
