// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/project.h"
#include "absl/strings/str_split.h"
#include <regex>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

Status CleanupStaleTables(
    std::shared_ptr<bigtable_admin::BigtableTableAdminConnection> c,
    std::string const& project_id, std::string const& instance_id) {
  auto const threshold =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  auto const max_table_id = RandomTableId(threshold);
  auto const re = std::regex(RandomTableIdRegex());

  google::bigtable::admin::v2::ListTablesRequest r;
  r.set_parent(InstanceName(project_id, instance_id));
  r.set_view(google::bigtable::admin::v2::Table::NAME_ONLY);
  auto admin = bigtable_admin::BigtableTableAdminClient(std::move(c));
  auto tables = admin.ListTables(std::move(r));
  for (auto const& t : tables) {
    if (!t) return std::move(t).status();
    std::vector<std::string> const components = absl::StrSplit(t->name(), '/');
    if (components.empty()) continue;
    auto const& id = components.back();
    if (!std::regex_match(id, re)) continue;
    if (id >= max_table_id) continue;
    // Failure to cleanup is not an error.
    std::cout << "Deleting table " << id << std::endl;
    (void)admin.DeleteTable(t->name());
  }
  return Status{};
}

Status CleanupStaleBackups(
    std::shared_ptr<bigtable_admin::BigtableTableAdminConnection> c,
    std::string const& project_id, std::string const& instance_id) {
  auto const threshold =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  auto const max_backup_id = RandomBackupId(threshold);
  auto const re = std::regex(RandomBackupIdRegex());

  auto admin = bigtable_admin::BigtableTableAdminClient(std::move(c));
  auto backups = admin.ListBackups(ClusterName(project_id, instance_id, "-"));
  for (auto const& b : backups) {
    if (!b) return std::move(b).status();
    std::vector<std::string> const components = absl::StrSplit(b->name(), '/');
    if (components.empty()) continue;
    auto const& id = components.back();
    if (!std::regex_match(id, re)) continue;
    if (id >= max_backup_id) continue;
    // Failure to cleanup is not an error.
    std::cout << "Deleting id " << id << std::endl;
    (void)admin.DeleteBackup(b->name());
  }
  return Status{};
}

Status CleanupStaleInstances(
    std::shared_ptr<bigtable_admin::BigtableInstanceAdminConnection> c,
    std::string const& project_id) {
  auto const threshold =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  auto const max_instance_id = RandomInstanceId(threshold);
  auto const re = std::regex(RandomInstanceIdRegex());

  auto admin = bigtable_admin::BigtableInstanceAdminClient(std::move(c));
  auto instances = admin.ListInstances(Project(project_id).FullName());
  if (!instances) return std::move(instances).status();
  for (auto const& i : instances->instances()) {
    std::vector<std::string> const components = absl::StrSplit(i.name(), '/');
    if (components.empty()) continue;
    auto const& id = components.back();
    if (!std::regex_match(id, re)) continue;
    if (id >= max_instance_id) continue;
    // Failure to cleanup is not an error.
    std::cout << "Deleting instance " << id << std::endl;
    (void)admin.DeleteInstance(i.name());
  }
  return Status{};
}

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
