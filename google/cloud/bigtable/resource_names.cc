// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {

std::string InstanceName(std::string const& project_id,
                         std::string const& instance_id) {
  return absl::StrCat("projects/", project_id, "/instances/" + instance_id);
}

std::string TableName(std::string const& project_id,
                      std::string const& instance_id,
                      std::string const& table_id) {
  return absl::StrCat(InstanceName(project_id, instance_id), "/tables/",
                      table_id);
}

std::string ClusterName(std::string const& project_id,
                        std::string const& instance_id,
                        std::string const& cluster_id) {
  return absl::StrCat(InstanceName(project_id, instance_id), "/clusters/",
                      cluster_id);
}

std::string AppProfileName(std::string const& project_id,
                           std::string const& instance_id,
                           std::string const& app_profile_id) {
  return absl::StrCat(InstanceName(project_id, instance_id), "/appProfiles/",
                      app_profile_id);
}

std::string BackupName(std::string const& project_id,
                       std::string const& instance_id,
                       std::string const& cluster_id,
                       std::string const& backup_id) {
  return absl::StrCat(ClusterName(project_id, instance_id, cluster_id),
                      "/backups/", backup_id);
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
