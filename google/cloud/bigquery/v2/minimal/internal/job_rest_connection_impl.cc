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
#include "google/cloud/internal/rest_retry_loop.h"
#include <memory>
namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

BigQueryJobRestConnectionImpl::BigQueryJobRestConnectionImpl(
    std::unique_ptr<google::cloud::BackgroundThreads> background,
    std::shared_ptr<BigQueryJobRestStub> stub, Options options)
    : background_(std::move(background)),
      stub_(std::move(stub)),
      options_(internal::MergeOptions(std::move(options),
                                      BigQueryJobConnection::options())) {}

StatusOr<GetJobResponse> BigQueryJobRestConnectionImpl::GetJob(
    GetJobRequest const& request) {
  return rest_internal::RestRetryLoop(
      retry_policy(), backoff_policy(), idempotency_policy()->GetJob(request),
      [this](rest_internal::RestContext& rest_context,
             GetJobRequest const& request) {
        return stub_->GetJob(rest_context, request);
      },
      request, __func__);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
