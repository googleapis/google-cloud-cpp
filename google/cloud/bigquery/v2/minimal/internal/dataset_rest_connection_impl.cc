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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_rest_connection_impl.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/pagination_range.h"
#include "google/cloud/internal/rest_retry_loop.h"
#include "google/cloud/status_or.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::unique_ptr<DatasetRetryPolicy> retry_policy(Options const& options) {
  return options.get<DatasetRetryPolicyOption>()->clone();
}

std::unique_ptr<BackoffPolicy> backoff_policy(Options const& options) {
  return options.get<DatasetBackoffPolicyOption>()->clone();
}

std::unique_ptr<DatasetIdempotencyPolicy> idempotency_policy(
    Options const& options) {
  return options.get<DatasetIdempotencyPolicyOption>()->clone();
}

}  // namespace

DatasetRestConnectionImpl::DatasetRestConnectionImpl(
    std::shared_ptr<DatasetRestStub> stub, Options options)
    : stub_(std::move(stub)),
      options_(internal::MergeOptions(std::move(options),
                                      DatasetConnection::options())) {}

StatusOr<Dataset> DatasetRestConnectionImpl::GetDataset(
    GetDatasetRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto result = rest_internal::RestRetryLoop(
      retry_policy(*current), backoff_policy(*current),
      idempotency_policy(*current)->GetDataset(request),
      [this](rest_internal::RestContext& rest_context, Options const&,
             GetDatasetRequest const& request) {
        return stub_->GetDataset(rest_context, request);
      },
      *current, request, __func__);
  if (!result) return std::move(result).status();
  return result->dataset;
}

StreamRange<ListFormatDataset> DatasetRestConnectionImpl::ListDatasets(
    ListDatasetsRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto req = request;
  req.set_page_token("");

  auto& stub = stub_;
  auto retry =
      std::shared_ptr<DatasetRetryPolicy const>(retry_policy(*current));
  auto backoff = std::shared_ptr<BackoffPolicy const>(backoff_policy(*current));
  auto idempotency = idempotency_policy(*current)->ListDatasets(req);
  char const* function_name = __func__;
  return google::cloud::internal::MakePaginationRange<
      StreamRange<ListFormatDataset>>(
      std::move(current), std::move(req),
      [stub, retry, backoff, idempotency, function_name](
          Options const& options, ListDatasetsRequest const& r) {
        return rest_internal::RestRetryLoop(
            retry->clone(), backoff->clone(), idempotency,
            [stub](rest_internal::RestContext& context, Options const&,
                   ListDatasetsRequest const& request) {
              return stub->ListDatasets(context, request);
            },
            options, r, function_name);
      },
      [](ListDatasetsResponse r) {
        std::vector<ListFormatDataset> result(r.datasets.size());
        auto& messages = r.datasets;
        std::move(messages.begin(), messages.end(), result.begin());
        return result;
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
