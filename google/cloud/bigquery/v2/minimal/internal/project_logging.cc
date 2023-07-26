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

#include "google/cloud/bigquery/v2/minimal/internal/project_logging.h"
#include "google/cloud/bigquery/v2/minimal/internal/log_wrapper.h"

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

ProjectLogging::ProjectLogging(std::shared_ptr<ProjectRestStub> child,
                               TracingOptions tracing_options,
                               std::set<std::string> components)
    : child_(std::move(child)),
      tracing_options_(std::move(tracing_options)),
      components_(std::move(components)) {}

// Customized LogWrapper is used here since ListProjectsRequest is
// not a protobuf message.
StatusOr<ListProjectsResponse> ProjectLogging::ListProjects(
    rest_internal::RestContext& rest_context,
    ListProjectsRequest const& request) {
  return LogWrapper(
      [this](rest_internal::RestContext& rest_context,
             ListProjectsRequest const& request) {
        return child_->ListProjects(rest_context, request);
      },
      rest_context, request, __func__,
      "google.cloud.bigquery.v2.minimal.internal.ListProjectsRequest",
      "google.cloud.bigquery.v2.minimal.internal.ListProjectsResponse",
      tracing_options_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
