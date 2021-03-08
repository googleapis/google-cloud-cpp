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

#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "absl/strings/str_split.h"
#include <regex>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

Status CleanupStaleTables(google::cloud::bigtable::TableAdmin admin) {
  auto const threshold =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  auto const max_table_id = RandomTableId(threshold);
  auto const re = std::regex(RandomTableIdRegex());

  auto tables = admin.ListTables(TableAdmin::NAME_ONLY);
  if (!tables) return std::move(tables).status();
  for (auto const& t : *tables) {
    std::vector<std::string> const components = absl::StrSplit(t.name(), '/');
    if (components.empty()) continue;
    auto const id = components.back();
    if (!std::regex_match(id, re)) continue;
    if (id >= max_table_id) continue;
    // Failure to cleanup is not an error.
    std::cout << "Deleting table " << id << std::endl;
    (void)admin.DeleteTable(id);
  }
  return Status{};
}

Status CleanupStaleBackups(google::cloud::bigtable::TableAdmin admin) {
  auto const threshold =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  auto const max_backup_id = RandomBackupId(threshold);
  auto const re = std::regex(RandomBackupIdRegex());

  auto backups = admin.ListBackups({});
  if (!backups) return std::move(backups).status();
  for (auto const& b : *backups) {
    std::vector<std::string> const components = absl::StrSplit(b.name(), '/');
    if (components.empty()) continue;
    auto const id = components.back();
    if (!std::regex_match(id, re)) continue;
    if (id >= max_backup_id) continue;
    // Failure to cleanup is not an error.
    std::cout << "Deleting id " << id << std::endl;
    (void)admin.DeleteBackup({b});
  }
  return Status{};
}

Status CleanupStaleInstances(google::cloud::bigtable::InstanceAdmin admin) {
  auto const threshold =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  auto const max_instance_id = RandomInstanceId(threshold);
  auto const re = std::regex(RandomInstanceIdRegex());

  auto instances = admin.ListInstances();
  if (!instances) return std::move(instances).status();
  for (auto const& i : instances->instances) {
    std::vector<std::string> const components = absl::StrSplit(i.name(), '/');
    if (components.empty()) continue;
    auto const id = components.back();
    if (!std::regex_match(id, re)) continue;
    if (id >= max_instance_id) continue;
    // Failure to cleanup is not an error.
    std::cout << "Deleting instance " << id << std::endl;
    (void)admin.DeleteInstance(id);
  }
  return Status{};
}

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
