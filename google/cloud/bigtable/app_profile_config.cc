// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/app_profile_config.h"
#include "google/cloud/internal/algorithm.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
AppProfileConfig AppProfileConfig::MultiClusterUseAny(std::string profile_id) {
  AppProfileConfig tmp;
  tmp.proto_.set_app_profile_id(std::move(profile_id));
  tmp.proto_.mutable_app_profile()
      ->mutable_multi_cluster_routing_use_any()
      ->Clear();
  return tmp;
}

AppProfileConfig AppProfileConfig::SingleClusterRouting(
    std::string profile_id, std::string cluster_id,
    bool allow_transactional_writes) {
  AppProfileConfig tmp;
  tmp.proto_.set_app_profile_id(std::move(profile_id));
  auto& routing =
      *tmp.proto_.mutable_app_profile()->mutable_single_cluster_routing();
  routing.set_cluster_id(std::move(cluster_id));
  routing.set_allow_transactional_writes(allow_transactional_writes);
  return tmp;
}

void AppProfileUpdateConfig::AddPathIfNotPresent(std::string field_name) {
  if (!google::cloud::internal::Contains(proto_.update_mask().paths(),
                                         field_name)) {
    proto_.mutable_update_mask()->add_paths(std::move(field_name));
  }
}

void AppProfileUpdateConfig::RemoveIfPresent(std::string const& field_name) {
  auto& paths = *proto_.mutable_update_mask()->mutable_paths();
  auto i = std::find(paths.begin(), paths.end(), field_name);
  if (paths.end() == i) {
    return;
  }
  paths.erase(i);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
