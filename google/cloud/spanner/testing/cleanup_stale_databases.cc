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

#include "google/cloud/spanner/testing/cleanup_stale_databases.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include <iostream>
#include <regex>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

Status CleanupStaleDatabases(
    google::cloud::spanner::DatabaseAdminClient admin_client,
    std::string const& project_id, std::string const& instance_id,
    std::chrono::system_clock::time_point tp) {
  // Drop any databases more than 2 days old. This automatically cleans up
  // any databases created by a previous build that may have crashed before
  // having a chance to cleanup.
  auto const expired = RandomDatabasePrefix(tp);
  std::regex re(RandomDatabasePrefixRegex());
  for (auto const& db :
       admin_client.ListDatabases(spanner::Instance(project_id, instance_id))) {
    if (!db) return std::move(db).status();
    // Extract the database ID from the database full name.
    auto pos = db->name().find_last_of('/');
    if (pos == std::string::npos) continue;
    auto id = db->name().substr(pos + 1);
    // Skip databases that do not look like a randomly created DB.
    if (!std::regex_match(id, re)) continue;
    // Skip databases that are relatively recent
    if (id > expired) continue;
    // Drop the database and ignore errors.
    (void)admin_client.DropDatabase(
        spanner::Database(project_id, instance_id, id));
    std::cout << "Dropped DB " << db->name() << "\n";
  }
  return {};
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
