// Copyright 2020 Google LLC
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

#include "google/cloud/spanner/testing/cleanup_stale_instances.h"
#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/project.h"
#include <chrono>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

Status CleanupStaleInstances(std::string const& project_id,
                             std::regex const& instance_name_regex) {
  Project project(project_id);
  spanner_admin::InstanceAdminClient instance_admin_client(
      spanner_admin::MakeInstanceAdminConnection());
  std::vector<std::string> instances = [&]() -> std::vector<std::string> {
    std::vector<std::string> instances;
    for (auto const& instance :
         instance_admin_client.ListInstances(project.FullName())) {
      if (!instance) break;
      auto name = instance->name();
      std::smatch m;
      if (std::regex_match(name, m, instance_name_regex)) {
        auto date_str = m[2];
        auto cutoff_date =
            google::cloud::internal::FormatRfc3339(
                std::chrono::system_clock::now() - std::chrono::hours(24))
                .substr(0, 10);
        // Compare the strings
        if (date_str < cutoff_date) {
          instances.push_back(name);
        }
      }
    }
    return instances;
  }();
  // Let it fail if we have too many leaks.
  if (instances.size() > 20) {
    return Status(StatusCode::kInternal, "too many stale instances");
  }
  spanner_admin::DatabaseAdminClient database_admin_client(
      spanner_admin::MakeDatabaseAdminConnection());
  // We ignore failures here.
  for (auto const& instance : instances) {
    for (auto const& backup : database_admin_client.ListBackups(instance)) {
      database_admin_client.DeleteBackup(backup->name());
    }
    instance_admin_client.DeleteInstance(instance);
  }
  return Status();
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
