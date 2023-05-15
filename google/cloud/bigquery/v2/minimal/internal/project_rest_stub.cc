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

#include "google/cloud/bigquery/v2/minimal/internal/project_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/internal/rest_stub_utils.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

ProjectRestStub::~ProjectRestStub() = default;

StatusOr<ListProjectsResponse> DefaultProjectRestStub::ListProjects(
    rest_internal::RestContext& rest_context,
    ListProjectsRequest const& request) {
  // Prepare the RestRequest from ListProjectsRequest.
  auto rest_request =
      PrepareRestRequest<ListProjectsRequest>(rest_context, request);

  // Call the rest stub and parse the RestResponse.
  return ParseFromRestResponse<ListProjectsResponse>(
      rest_stub_->Get(rest_context, std::move(*rest_request)));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
