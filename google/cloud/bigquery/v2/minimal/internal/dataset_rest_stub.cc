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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/internal/rest_stub_utils.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

DatasetRestStub::~DatasetRestStub() = default;

StatusOr<GetDatasetResponse> DefaultDatasetRestStub::GetDataset(
    rest_internal::RestContext& rest_context,
    GetDatasetRequest const& request) {
  // Prepare the RestRequest from GetDatasetRequest.
  auto rest_request =
      PrepareRestRequest<GetDatasetRequest>(rest_context, request);

  // Call the rest stub and parse the RestResponse.
  return ParseFromRestResponse<GetDatasetResponse>(
      rest_stub_->Get(rest_context, std::move(*rest_request)));
}

StatusOr<ListDatasetsResponse> DefaultDatasetRestStub::ListDatasets(
    rest_internal::RestContext& rest_context,
    ListDatasetsRequest const& request) {
  // Prepare the RestRequest from ListDatasetsRequest.
  auto rest_request =
      PrepareRestRequest<ListDatasetsRequest>(rest_context, request);

  // Call the rest stub and parse the RestResponse.
  return ParseFromRestResponse<ListDatasetsResponse>(
      rest_stub_->Get(rest_context, std::move(*rest_request)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
