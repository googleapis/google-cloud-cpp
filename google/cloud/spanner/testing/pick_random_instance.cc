// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/spanner/create_instance_request_builder.h"
#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/internal/getenv.h"

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {
StatusOr<std::string> PickRandomInstance(
    google::cloud::internal::DefaultPRNG& generator,
    std::string const& project_id) {
  spanner::InstanceAdminClient client(spanner::MakeInstanceAdminConnection());

  /**
   * We started to have integration tests for InstanceAdminClient which creates
   * and deletes temporary instances. If we pick one of those instances here,
   * the following tests will fail after the deletion.
   * InstanceAdminClient's integration tests are using instances prefixed with
   * "temporary-instances-", so we only pick instances prefixed with
   * "test-instance-" here for isolation.
   */
  std::vector<std::string> instance_ids;
  for (auto& instance : client.ListInstances(project_id, "")) {
    if (!instance) return std::move(instance).status();
    auto name = instance->name();
    std::string const sep = "/instances/";
    std::string const valid_instance_name = sep + "test-instance-";
    auto pos = name.rfind(valid_instance_name);
    if (pos != std::string::npos) {
      instance_ids.push_back(name.substr(pos + sep.size()));
    }
  }

  if (instance_ids.empty()) {
    auto emulator = google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST");
    if (emulator.has_value()) {
      // We expect test instances to exist when running against real services,
      // but if we are running against the emulator we're happy to create one.
      spanner::Instance in(project_id, "test-instance-a");
      auto create_instance_request =
          spanner::CreateInstanceRequestBuilder(
              in, "projects/" + in.project_id() +
                      "/instanceConfigs/emulator-config")
              .Build();
      StatusOr<google::spanner::admin::instance::v1::Instance> instance =
          client.CreateInstance(create_instance_request).get();
      if (instance || instance.status().code() == StatusCode::kAlreadyExists) {
        instance_ids.push_back(in.instance_id());
      }
    }
  }
  if (instance_ids.empty()) {
    return Status(StatusCode::kInternal, "No available instances for test");
  }

  auto random_index =
      std::uniform_int_distribution<std::size_t>(0, instance_ids.size() - 1);
  return instance_ids[random_index(generator)];
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
