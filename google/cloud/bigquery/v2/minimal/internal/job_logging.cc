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
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/log.h"
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

// google::cloud::internal::LogWrapper cannot be used here since the request is
// not a protobuf message.
StatusOr<GetJobResponse> BigQueryJobLogging::GetJob(
    rest_internal::RestContext& rest_context, GetJobRequest const& request) {
  char const* context = __func__;
  GCP_LOG(DEBUG) << context
                 << "() << GetJobRequest{project_id=" << request.project_id()
                 << ",job_id=" << request.job_id()
                 << ",location=" << request.location() << "}";

  if (!rest_context.headers().empty()) {
    GCP_LOG(DEBUG) << context << "() << RestContext{";
    for (auto const& h : rest_context.headers()) {
      GCP_LOG(DEBUG) << "[header_name=" << h.first << ", header_value={"
                     << absl::StrJoin(h.second, "&") << "}],";
    }
    GCP_LOG(DEBUG) << "}";
  }
  auto response = child_->GetJob(rest_context, request);
  if (!response.ok()) {
    GCP_LOG(DEBUG) << context << "() >> status={" << response.status() << "}";
  }
  return response;
}

StatusOr<ListJobsResponse> BigQueryJobLogging::ListJobs(
    rest_internal::RestContext& rest_context, ListJobsRequest const& request) {
  char const* context = __func__;
  GCP_LOG(DEBUG) << context
                 << "() << ListJobsRequest{project_id=" << request.project_id()
                 << ", all_users=" << request.all_users()
                 << ", max_results=" << request.max_results()
                 << ", page_token=" << request.page_token()
                 << ", projection=" << request.projection().value
                 << ", state_filter=" << request.state_filter().value
                 << ", parent_job_id=" << request.parent_job_id() << "}";

  if (!rest_context.headers().empty()) {
    GCP_LOG(DEBUG) << context << "() << RestContext{";
    for (auto const& h : rest_context.headers()) {
      GCP_LOG(DEBUG) << "[header_name=" << h.first << ", header_value={"
                     << absl::StrJoin(h.second, "&") << "}],";
    }
    GCP_LOG(DEBUG) << "}";
  }
  auto response = child_->ListJobs(rest_context, request);
  if (!response.ok()) {
    GCP_LOG(DEBUG) << context << "() >> status={" << response.status() << "}";
  }
  return response;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
