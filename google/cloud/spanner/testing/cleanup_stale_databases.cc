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

#include "google/cloud/spanner/testing/cleanup_stale_databases.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include <iostream>
#include <regex>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Status CleanupStaleDatabases(
    google::cloud::spanner_admin::DatabaseAdminClient admin_client,
    std::string const& project_id, std::string const& instance_id,
    std::chrono::system_clock::time_point tp) {
  spanner::Instance instance(Project(project_id), instance_id);
  auto const expired = RandomDatabasePrefix(tp);
  std::regex re(RandomDatabasePrefixRegex());
  for (auto const& db : admin_client.ListDatabases(instance.FullName())) {
    if (!db) return std::move(db).status();
    auto id = spanner::MakeDatabase(db->name())->database_id();
    // Skip databases that do not look like a randomly created DB.
    if (!std::regex_search(id, re)) continue;
    // Skip databases that are relatively recent
    if (id > expired) continue;
    // Drop the database and ignore errors.
    admin_client.DropDatabase(db->name());
    std::cout << "Dropped DB " << db->name() << "\n";
  }
  return {};
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
