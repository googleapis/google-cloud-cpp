// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"
#include "google/cloud/spanner/create_instance_request_builder.h"
#include "google/cloud/spanner/instance.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/project.h"
#include "absl/strings/match.h"
#include <vector>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<std::string> PickRandomInstance(
    google::cloud::internal::DefaultPRNG& generator,
    std::string const& project_id, std::string const& filter) {
  Project project(project_id);
  spanner_admin::InstanceAdminClient client(
      spanner_admin::MakeInstanceAdminConnection());

  // We only pick instance IDs starting with "test-instance-" for isolation
  // from tests that create/delete their own instances (in particular from
  // tests calling `RandomInstanceName()`, which uses "temporary-instance-").
  auto const instance_prefix = spanner::Instance(project, "").FullName();
  auto full_filter = "name:" + instance_prefix + "test-instance-";
  if (!filter.empty()) full_filter.append(" AND (" + filter + ")");

  std::vector<std::string> instance_ids;
  google::spanner::admin::instance::v1::ListInstancesRequest request;
  request.set_parent(project.FullName());
  request.set_filter(std::move(full_filter));
  for (auto& instance : client.ListInstances(request)) {
    if (!instance) return std::move(instance).status();
    auto instance_id = instance->name().substr(instance_prefix.size());
    if (!absl::StartsWith(instance_id, "test-instance-")) {
      auto emulator = google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST");
      if (emulator.has_value()) continue;  // server-side filter not supported
      return Status(StatusCode::kInternal,
                    "ListInstances erroneously returned " + instance_id);
    }
    instance_ids.push_back(std::move(instance_id));
  }

  if (instance_ids.empty()) {
    auto emulator = google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST");
    if (emulator.has_value()) {
      // We expect test instances to exist when running against real services,
      // but if we are running against the emulator we're happy to create one.
      spanner::Instance in(project_id, "test-instance-a");
      auto create_instance_request =
          spanner::CreateInstanceRequestBuilder(
              in, in.project().FullName() + "/instanceConfigs/emulator-config")
              .Build();
      StatusOr<google::spanner::admin::instance::v1::Instance> instance =
          client.CreateInstance(create_instance_request).get();
      if (instance || instance.status().code() == StatusCode::kAlreadyExists) {
        instance_ids.push_back(in.instance_id());
      }
    }
  }

  if (instance_ids.empty()) {
    return internal::UnavailableError("No available instances",
                                      GCP_ERROR_INFO());
  }

  auto random_index =
      std::uniform_int_distribution<std::size_t>(0, instance_ids.size() - 1);
  return instance_ids[random_index(generator)];
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
