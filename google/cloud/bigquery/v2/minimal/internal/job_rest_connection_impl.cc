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

#include "google/cloud/bigquery/v2/minimal/internal/job_rest_connection_impl.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/rest_retry_loop.h"
#include "google/cloud/status_or.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

BigQueryJobRestConnectionImpl::BigQueryJobRestConnectionImpl(
    std::shared_ptr<BigQueryJobRestStub> stub, Options options)
    : stub_(std::move(stub)),
      options_(internal::MergeOptions(std::move(options),
                                      BigQueryJobConnection::options())) {}

StatusOr<Job> BigQueryJobRestConnectionImpl::GetJob(
    GetJobRequest const& request) {
  auto result = rest_internal::RestRetryLoop(
      retry_policy(), backoff_policy(), idempotency_policy()->GetJob(request),
      [this](rest_internal::RestContext& rest_context,
             GetJobRequest const& request) {
        return stub_->GetJob(rest_context, request);
      },
      request, __func__);
  if (!result) return std::move(result).status();
  return result->job;
}

StreamRange<ListFormatJob> BigQueryJobRestConnectionImpl::ListJobs(
    ListJobsRequest const& request) {
  auto req = request;
  req.set_page_token("");

  auto& stub = stub_;
  auto retry = std::shared_ptr<BigQueryJobRetryPolicy const>(retry_policy());
  auto backoff = std::shared_ptr<BackoffPolicy const>(backoff_policy());
  auto idempotency = idempotency_policy()->ListJobs(req);
  char const* function_name = __func__;
  return google::cloud::internal::MakePaginationRange<
      StreamRange<ListFormatJob>>(
      std::move(req),
      [stub, retry, backoff, idempotency,
       function_name](ListJobsRequest const& r) {
        return rest_internal::RestRetryLoop(
            retry->clone(), backoff->clone(), idempotency,
            [stub](rest_internal::RestContext& context,
                   ListJobsRequest const& request) {
              return stub->ListJobs(context, request);
            },
            r, function_name);
      },
      [](ListJobsResponse r) {
        std::vector<ListFormatJob> result(r.jobs.size());
        auto& messages = r.jobs;
        std::move(messages.begin(), messages.end(), result.begin());
        return result;
      });
}

StatusOr<Job> BigQueryJobRestConnectionImpl::InsertJob(
    InsertJobRequest const& request) {
  auto result = rest_internal::RestRetryLoop(
      retry_policy(), backoff_policy(),
      idempotency_policy()->InsertJob(request),
      [this](rest_internal::RestContext& rest_context,
             InsertJobRequest const& request) {
        return stub_->InsertJob(rest_context, request);
      },
      request, __func__);
  if (!result) return std::move(result).status();
  return result->job;
}

StatusOr<Job> BigQueryJobRestConnectionImpl::CancelJob(
    CancelJobRequest const& request) {
  auto result = rest_internal::RestRetryLoop(
      retry_policy(), backoff_policy(),
      idempotency_policy()->CancelJob(request),
      [this](rest_internal::RestContext& rest_context,
             CancelJobRequest const& request) {
        return stub_->CancelJob(rest_context, request);
      },
      request, __func__);
  if (!result) return std::move(result).status();
  return std::move(result->job);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
