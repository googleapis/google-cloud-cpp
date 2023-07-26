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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_PROJECT_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_PROJECT_CLIENT_H

#include "google/cloud/bigquery/v2/minimal/internal/project_connection.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/// BigQuery Project Client.
///
/// The Project client uses the BigQuery Project API to read project information
/// from BigQuery.
///
class ProjectClient {
 public:
  explicit ProjectClient(std::shared_ptr<ProjectConnection> connection,
                         Options opts = {});
  ~ProjectClient() = default;

  ProjectClient(ProjectClient const&) = default;
  ProjectClient& operator=(ProjectClient const&) = default;
  ProjectClient(ProjectClient&&) = default;
  ProjectClient& operator=(ProjectClient&&) = default;

  friend bool operator==(ProjectClient const& a, ProjectClient const& b) {
    return a.connection_ == b.connection_;
  }
  friend bool operator!=(ProjectClient const& a, ProjectClient const& b) {
    return !(a == b);
  }

  /// Lists all projects for a user.
  ///
  /// @see https://cloud.google.com/bigquery/docs/resource-hierarchy#projects
  /// for more details on BigQuery projects.
  ///
  /// @see https://cloud.google.com/resource-manager/docs/ for more project capabilities.
  StreamRange<Project> ListProjects(ListProjectsRequest const& request,
                                    Options opts = {});

 private:
  std::shared_ptr<ProjectConnection> connection_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_PROJECT_CLIENT_H
