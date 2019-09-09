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
#include "google/cloud/spanner/instance_admin_client.h"

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {
StatusOr<std::string> PickRandomInstance(
    google::cloud::internal::DefaultPRNG& generator,
    std::string const& project_id) {
  spanner::InstanceAdminClient client(spanner::MakeInstanceAdminConnection());

  std::vector<std::string> instance_ids;
  for (auto instance : client.ListInstances(project_id, "")) {
    if (!instance) return std::move(instance).status();
    auto name = instance->name();
    std::string const sep = "/instances/";
    auto pos = name.rfind(sep);
    if (pos == std::string::npos) {
      return Status(StatusCode::kInternal, "Invalid instance name " + name);
    }

    instance_ids.push_back(name.substr(pos + sep.size()));
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
