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

#include "google/cloud/bigquery/v2/minimal/internal/project_rest_connection_impl.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_options.h"
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

std::unique_ptr<ProjectRetryPolicy> retry_policy(Options const& options) {
  return options.get<ProjectRetryPolicyOption>()->clone();
}

std::unique_ptr<BackoffPolicy> backoff_policy(Options const& options) {
  return options.get<ProjectBackoffPolicyOption>()->clone();
}

std::unique_ptr<ProjectIdempotencyPolicy> idempotency_policy(
    Options const& options) {
  return options.get<ProjectIdempotencyPolicyOption>()->clone();
}

}  // namespace

ProjectRestConnectionImpl::ProjectRestConnectionImpl(
    std::shared_ptr<ProjectRestStub> stub, Options options)
    : stub_(std::move(stub)),
      options_(internal::MergeOptions(std::move(options),
                                      ProjectConnection::options())) {}

StreamRange<Project> ProjectRestConnectionImpl::ListProjects(
    ListProjectsRequest const& request) {
  auto current = google::cloud::internal::SaveCurrentOptions();
  auto req = request;
  req.set_page_token("");

  auto retry =
      std::shared_ptr<ProjectRetryPolicy const>(retry_policy(*current));
  auto backoff = std::shared_ptr<BackoffPolicy const>(backoff_policy(*current));
  auto idempotency = idempotency_policy(*current)->ListProjects(req);
  char const* function_name = __func__;
  return google::cloud::internal::MakePaginationRange<StreamRange<Project>>(
      std::move(current), std::move(req),
      [stub = stub_, retry = std::move(retry), backoff = std::move(backoff),
       idempotency,
       function_name](Options const& options, ListProjectsRequest const& r) {
        return rest_internal::RestRetryLoop(
            retry->clone(), backoff->clone(), idempotency,
            [stub](rest_internal::RestContext& context, Options const&,
                   ListProjectsRequest const& request) {
              return stub->ListProjects(context, request);
            },
            options, r, function_name);
      },
      [](ListProjectsResponse r) {
        std::vector<Project> result(r.projects.size());
        auto& messages = r.projects;
        std::move(messages.begin(), messages.end(), result.begin());
        return result;
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google
