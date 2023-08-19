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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_PROJECT_REST_CONNECTION_IMPL_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_PROJECT_REST_CONNECTION_IMPL_H

#include "google/cloud/bigquery/v2/minimal/internal/project_connection.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_idempotency_policy.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_options.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_rest_stub.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_retry_policy.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include "google/cloud/stream_range.h"
#include "google/cloud/version.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class ProjectRestConnectionImpl : public ProjectConnection {
 public:
  ~ProjectRestConnectionImpl() override = default;

  ProjectRestConnectionImpl(std::shared_ptr<ProjectRestStub> stub,
                            Options options);

  Options options() override { return options_; }

  StreamRange<Project> ListProjects(
      ListProjectsRequest const& request) override;

 private:
  std::shared_ptr<ProjectRestStub> stub_;
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGQUERY_V2_MINIMAL_INTERNAL_PROJECT_REST_CONNECTION_IMPL_H
