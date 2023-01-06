// Copyright 2020 Google LLC
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

#include "google/cloud/spanner/testing/pick_instance_config.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::string PickInstanceConfig(
    Project const& project, google::cloud::internal::DefaultPRNG& generator,
    std::function<bool(
        google::spanner::admin::instance::v1::InstanceConfig const&)> const&
        predicate) {
  std::string instance_config_name;
  std::vector<std::string> instance_configs;
  spanner_admin::InstanceAdminClient client(
      spanner_admin::MakeInstanceAdminConnection());
  for (auto const& instance_config :
       client.ListInstanceConfigs(project.FullName())) {
    if (instance_config) {
      if (!instance_config->base_config().empty()) {
        continue;  // skip non-base configurations
      }
      if (instance_config_name.empty()) {
        // The fallback for when nothing satisfies the predicate, which is
        // only really useful for the emulator, which has a single config.
        instance_config_name = instance_config->name();
      }
      if (predicate(*instance_config)) {
        instance_configs.push_back(instance_config->name());
      }
    }
  }
  if (!instance_configs.empty()) {
    auto random_index = std::uniform_int_distribution<std::size_t>(
        0, instance_configs.size() - 1);
    instance_config_name = std::move(instance_configs[random_index(generator)]);
  }
  return instance_config_name;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
