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
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/debug_string.h"
#include "google/cloud/internal/debug_string_status.h"
#include "google/cloud/internal/invoke_result.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "absl/strings/string_view.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

template <typename Functor, typename Request,
          typename Result = google::cloud::internal::invoke_result_t<
              Functor, rest_internal::RestContext&, Request const&>>
Result JobLogWrapper(Functor&& functor, rest_internal::RestContext& context,
                     Request const& request, char const* where,
                     absl::string_view request_name,
                     absl::string_view response_name,
                     TracingOptions const& options) {
  auto formatter = [options](std::string* out, auto const& header) {
    absl::StrAppend(
        out, " { name: \"", header.first, "\" value: \"",
        internal::DebugString(absl::StrJoin(header.second, "&"), options),
        "\" } ");
  };
  GCP_LOG(DEBUG) << where << "() << "
                 << request.DebugString(request_name, options) << ", Context {"
                 << absl::StrJoin(context.headers(), ", ", formatter) << " }";

  auto response = functor(context, request);
  if (!response) {
    GCP_LOG(DEBUG) << where << "() >> status="
                   << internal::DebugString(response.status(), options);
  } else {
    GCP_LOG(DEBUG) << where << "() >> response="
                   << response->DebugString(response_name, options);
  }

  return response;
}

BigQueryJobLogging::BigQueryJobLogging(
    std::shared_ptr<BigQueryJobRestStub> child, TracingOptions tracing_options,
    std::set<std::string> components)
    : child_(std::move(child)),
      tracing_options_(std::move(tracing_options)),
      components_(std::move(components)) {}

// Customized LogWrapper is used here since GetJobRequest is
// not a protobuf message.
StatusOr<GetJobResponse> BigQueryJobLogging::GetJob(
    rest_internal::RestContext& rest_context, GetJobRequest const& request) {
  return JobLogWrapper(
      [this](rest_internal::RestContext& rest_context,
             GetJobRequest const& request) {
        return child_->GetJob(rest_context, request);
      },
      rest_context, request, __func__,
      "google.cloud.bigquery.v2.minimal.internal.GetJobRequest",
      "google.cloud.bigquery.v2.minimal.internal.GetJobResponse",
      tracing_options_);
}

// Customized LogWrapper is used here since ListJobsRequest is
// not a protobuf message.
StatusOr<ListJobsResponse> BigQueryJobLogging::ListJobs(
    rest_internal::RestContext& rest_context, ListJobsRequest const& request) {
  return JobLogWrapper(
      [this](rest_internal::RestContext& rest_context,
             ListJobsRequest const& request) {
        return child_->ListJobs(rest_context, request);
      },
      rest_context, request, __func__,
      "google.cloud.bigquery.v2.minimal.internal.ListJobsRequest",
      "google.cloud.bigquery.v2.minimal.internal.ListJobsResponse",
      tracing_options_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
