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

//! [START spanner_quickstart]
#include "google/cloud/spanner/client.h"
//! [END spanner_quickstart]
#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/backup.h"
#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/spanner/create_instance_request_builder.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/policies.h"
#include "google/cloud/spanner/testing/random_backup_name.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/spanner/testing/random_instance_name.h"
#include "google/cloud/spanner/update_instance_request_builder.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "absl/types/optional.h"
#include <google/protobuf/util/time_util.h>
#include <chrono>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

namespace {

//! [get-instance]
void GetInstance(google::cloud::spanner::InstanceAdminClient client,
                 std::string const& project_id,
                 std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  auto instance = client.GetInstance(in);
  if (!instance) throw std::runtime_error(instance.status().message());

  std::cout << "The instance " << instance->name()
            << " exists and its metadata is:\n"
            << instance->DebugString() << "\n";
}
//! [get-instance]

void GetInstanceCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw std::runtime_error("get-instance <project-id> <instance-id>");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  GetInstance(std::move(client), argv[0], argv[1]);
}

//! [START spanner_create_instance] [create-instance]
void CreateInstance(google::cloud::spanner::InstanceAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id,
                    std::string const& display_name,
                    std::string const& region) {
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  namespace spanner = google::cloud::spanner;
  spanner::Instance in(project_id, instance_id);

  std::string region_id = region.empty() ? "us-central1" : region;
  std::string instance_config =
      "projects/" + project_id + "/instanceConfigs/regional-" + region_id;
  future<StatusOr<google::spanner::admin::instance::v1::Instance>> f =
      client.CreateInstance(
          spanner::CreateInstanceRequestBuilder(in, instance_config)
              .SetDisplayName(display_name)
              .SetNodeCount(1)
              .SetLabels({{"cloud_spanner_samples", "true"}})
              .Build());
  auto instance = f.get();
  if (!instance) throw std::runtime_error(instance.status().message());
  std::cout << "Created instance [" << in << "]\n";
}
//! [END spanner_create_instance] [create-instance]

void CreateInstanceCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3 && argv.size() != 4) {
    throw std::runtime_error(
        "create-instance <project-id> <instance-id> <display_name> "
        "[instance_config]");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  std::string instance_config = argv.size() == 4 ? argv[3] : "";
  CreateInstance(std::move(client), argv[0], argv[1], argv[2], instance_config);
}

void PickLocationAndCreateInstance(
    google::cloud::spanner::InstanceAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& display_name) {
  // Pick instance config that matches the regex, if there's no match, pick the
  // first one.
  std::string instance_config = [client, project_id]() mutable {
    std::string ret{};
    std::regex filter = std::regex(".*us-west.*");
    for (auto const& instance_config : client.ListInstanceConfigs(project_id)) {
      if (!instance_config) break;
      if (ret.empty()) {
        // fallback to the first element.
        ret = instance_config->name();
      }
      if (std::regex_match(instance_config->name(), filter)) {
        return instance_config->name();
      }
    }
    return ret;
  }();
  if (instance_config.empty()) {
    throw std::runtime_error("could not pick an instance config");
  }
  namespace spanner = google::cloud::spanner;
  spanner::Instance in(project_id, instance_id);
  auto instance = client
                      .CreateInstance(spanner::CreateInstanceRequestBuilder(
                                          in, instance_config)
                                          .SetDisplayName(display_name)
                                          .SetNodeCount(1)
                                          .Build())
                      .get();
  if (!instance) throw std::runtime_error(instance.status().message());
  std::cout << "Created instance [" << in << "]\n";
}

//! [update-instance]
void UpdateInstance(google::cloud::spanner::InstanceAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id,
                    std::string const& new_display_name) {
  google::cloud::spanner::Instance in(project_id, instance_id);

  auto f = client.UpdateInstance(
      google::cloud::spanner::UpdateInstanceRequestBuilder(in)
          .SetDisplayName(new_display_name)
          .Build());
  auto instance = f.get();
  if (!instance) throw std::runtime_error(instance.status().message());
  std::cout << "Updated instance [" << in << "]\n";
}
//! [update-instance]

void UpdateInstanceCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "update-instance <project-id> <instance-id> <new_display_name>");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  UpdateInstance(std::move(client), argv[0], argv[1], argv[2]);
}

//! [delete-instance]
void DeleteInstance(google::cloud::spanner::InstanceAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);

  auto status = client.DeleteInstance(in);
  if (!status.ok()) throw std::runtime_error(status.message());
  std::cout << "Deleted instance [" << in << "]\n";
}
//! [delete-instance]

void DeleteInstanceCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw std::runtime_error("delete-instance <project-id> <instance-id>");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  DeleteInstance(std::move(client), argv[0], argv[1]);
}

//! [list-instance-configs]
void ListInstanceConfigs(google::cloud::spanner::InstanceAdminClient client,
                         std::string const& project_id) {
  int count = 0;
  for (auto const& instance_config : client.ListInstanceConfigs(project_id)) {
    if (!instance_config) {
      throw std::runtime_error(instance_config.status().message());
    }
    ++count;
    std::cout << "Instance config [" << count << "]:\n"
              << instance_config->DebugString() << "\n";
  }
  if (count == 0) {
    std::cout << "No instance configs found in project " << project_id;
  }
}
//! [list-instance-configs]

void ListInstanceConfigsCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 1) {
    throw std::runtime_error("list-instance-configs <project-id>");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  ListInstanceConfigs(std::move(client), argv[0]);
}

//! [get-instance-config]
void GetInstanceConfig(google::cloud::spanner::InstanceAdminClient client,
                       std::string const& project_id,
                       std::string const& instance_config_name) {
  auto instance_config = client.GetInstanceConfig(
      "projects/" + project_id + "/instanceConfigs/" + instance_config_name);
  if (!instance_config) {
    throw std::runtime_error(instance_config.status().message());
  }

  std::cout << "The instanceConfig " << instance_config->name()
            << " exists and its metadata is:\n"
            << instance_config->DebugString() << "\n";
}
//! [get-instance-config]

void GetInstanceConfigCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw std::runtime_error(
        "get-instance-config <project-id> <instance-config-name>");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  GetInstanceConfig(std::move(client), argv[0], argv[1]);
}

//! [list-instances]
void ListInstances(google::cloud::spanner::InstanceAdminClient client,
                   std::string const& project_id) {
  int count = 0;
  for (auto const& instance : client.ListInstances(project_id, "")) {
    if (!instance) throw std::runtime_error(instance.status().message());
    ++count;
    std::cout << "Instance [" << count << "]:\n"
              << instance->DebugString() << "\n";
  }
  if (count == 0) {
    std::cout << "No instances found in project " << project_id;
  }
}
//! [list-instances]

void ListInstancesCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 1) {
    throw std::runtime_error("list-instances <project-id>");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  ListInstances(std::move(client), argv[0]);
}

//! [instance-get-iam-policy]
void InstanceGetIamPolicy(google::cloud::spanner::InstanceAdminClient client,
                          std::string const& project_id,
                          std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  auto actual = client.GetIamPolicy(in);
  if (!actual) throw std::runtime_error(actual.status().message());

  std::cout << "The IAM policy for instance " << instance_id << " is:\n"
            << actual->DebugString() << "\n";
}
//! [instance-get-iam-policy]

void InstanceGetIamPolicyCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw std::runtime_error(
        "instance-get-iam-policy <project-id> <instance-id>");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  InstanceGetIamPolicy(std::move(client), argv[0], argv[1]);
}

//! [add-database-reader]
void AddDatabaseReader(google::cloud::spanner::InstanceAdminClient client,
                       std::string const& project_id,
                       std::string const& instance_id,
                       std::string const& new_reader) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  auto result = client.SetIamPolicy(
      in,
      [&new_reader](google::iam::v1::Policy current)
          -> absl::optional<google::iam::v1::Policy> {
        // Find (or create) the binding for "roles/spanner.databaseReader".
        auto& binding = [&current]() -> google::iam::v1::Binding& {
          auto role_pos = std::find_if(
              current.mutable_bindings()->begin(),
              current.mutable_bindings()->end(),
              [](google::iam::v1::Binding const& b) {
                return b.role() == "roles/spanner.databaseReader" &&
                       !b.has_condition();
              });
          if (role_pos != current.mutable_bindings()->end()) {
            return *role_pos;
          }
          auto& binding = *current.add_bindings();
          binding.set_role("roles/spanner.databaseReader");
          return binding;
        }();

        auto member_pos = std::find(binding.members().begin(),
                                    binding.members().end(), new_reader);
        if (member_pos != binding.members().end()) {
          std::cout << "The entity " << new_reader
                    << " is already a database reader:\n"
                    << current.DebugString();
          return {};
        }
        binding.add_members(new_reader);
        return current;
      });

  if (!result) throw std::runtime_error(result.status().message());
  std::cout << "Successfully added " << new_reader
            << " to the database reader role:\n"
            << result->DebugString() << "\n";
}
//! [add-database-reader]

void AddDatabaseReaderCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "add-database-reader <project-id> <instance-id> <new-reader>");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  AddDatabaseReader(std::move(client), argv[0], argv[1], argv[2]);
}

//! [remove-database-reader]
void RemoveDatabaseReader(google::cloud::spanner::InstanceAdminClient client,
                          std::string const& project_id,
                          std::string const& instance_id,
                          std::string const& reader) {
  google::cloud::spanner::Instance in(project_id, instance_id);

  auto result = client.SetIamPolicy(
      in,
      [&reader](google::iam::v1::Policy current)
          -> absl::optional<google::iam::v1::Policy> {
        // Find the binding for "roles/spanner.databaseReader".
        auto role_pos =
            std::find_if(current.mutable_bindings()->begin(),
                         current.mutable_bindings()->end(),
                         [](google::iam::v1::Binding const& b) {
                           return b.role() == "roles/spanner.databaseReader" &&
                                  !b.has_condition();
                         });
        if (role_pos == current.mutable_bindings()->end()) {
          std::cout << "Nothing to do as the roles/spanner.databaseReader role "
                       "is empty\n";
          return {};
        }
        role_pos->mutable_members()->erase(
            std::remove(role_pos->mutable_members()->begin(),
                        role_pos->mutable_members()->end(), reader),
            role_pos->mutable_members()->end());

        return current;
      });

  if (!result) throw std::runtime_error(result.status().message());
  std::cout << "Successfully removed " << reader
            << " from the database reader role:\n"
            << result->DebugString() << "\n";
}
//! [remove-database-reader]

void RemoveDatabaseReaderCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "remove-database-reader <project-id> <instance-id> <existing-reader>");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  RemoveDatabaseReader(std::move(client), argv[0], argv[1], argv[2]);
}

//! [instance-test-iam-permissions]
void InstanceTestIamPermissions(
    google::cloud::spanner::InstanceAdminClient client,
    std::string const& project_id, std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  auto actual = client.TestIamPermissions(in, {"spanner.databases.list"});
  if (!actual) throw std::runtime_error(actual.status().message());

  char const* msg = actual->permissions().empty() ? "does not" : "does";

  std::cout
      << "The caller " << msg
      << " have permission to list databases on the Cloud Spanner instance "
      << in.instance_id() << "\n";
}
//! [instance-test-iam-permissions]

void InstanceTestIamPermissionsCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw std::runtime_error(
        "instance-test-iam-permissions <project-id> <instance-id>");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  InstanceTestIamPermissions(std::move(client), argv[0], argv[1]);
}

//! [create-database] [START spanner_create_database]
void CreateDatabase(google::cloud::spanner::DatabaseAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id,
                    std::string const& database_id) {
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  std::vector<std::string> extra_statements;
  extra_statements.emplace_back(R"""(
      CREATE TABLE Singers (
          SingerId   INT64 NOT NULL,
          FirstName  STRING(1024),
          LastName   STRING(1024),
          SingerInfo BYTES(MAX)
      ) PRIMARY KEY (SingerId))""");
  extra_statements.emplace_back(R"""(
      CREATE TABLE Albums (
          SingerId     INT64 NOT NULL,
          AlbumId      INT64 NOT NULL,
          AlbumTitle   STRING(MAX)
      ) PRIMARY KEY (SingerId, AlbumId),
          INTERLEAVE IN PARENT Singers ON DELETE CASCADE)""");
  future<StatusOr<google::spanner::admin::database::v1::Database>> f =
      client.CreateDatabase(database, std::move(extra_statements));
  StatusOr<google::spanner::admin::database::v1::Database> db = f.get();
  if (!db) throw std::runtime_error(db.status().message());
  std::cout << "Created database [" << database << "]\n";
}
//! [create-database] [END spanner_create_database]

// [START spanner_create_table_with_datatypes]
void CreateTableWithDatatypes(
    google::cloud::spanner::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
      f = client.UpdateDatabase(database, {R"""(
            CREATE TABLE Venues (
                VenueId         INT64 NOT NULL,
                VenueName       STRING(100),
                VenueInfo       BYTES(MAX),
                Capacity        INT64,
                AvailableDates  ARRAY<DATE>,
                LastContactDate DATE,
                OutdoorVenue    BOOL,
                PopularityScore FLOAT64,
                LastUpdateTime  TIMESTAMP NOT NULL OPTIONS
                    (allow_commit_timestamp=true)
            ) PRIMARY KEY (VenueId))"""});
  StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
      metadata = f.get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`Venues` table created, new DDL:\n"
            << metadata->DebugString() << "\n";
}
// [END spanner_create_table_with_datatypes]

// [START spanner_create_table_with_timestamp_column]
void CreateTableWithTimestamp(
    google::cloud::spanner::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
      f = client.UpdateDatabase(database, {R"""(
            CREATE TABLE Performances (
                SingerId        INT64 NOT NULL,
                VenueId         INT64 NOT NULL,
                EventDate       Date,
                Revenue         INT64,
                LastUpdateTime  TIMESTAMP NOT NULL OPTIONS
                    (allow_commit_timestamp=true)
            ) PRIMARY KEY (SingerId, VenueId, EventDate),
                INTERLEAVE IN PARENT Singers ON DELETE CASCADE)"""});
  StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
      metadata = f.get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`Performances` table created, new DDL:\n"
            << metadata->DebugString() << "\n";
}
// [END spanner_create_table_with_timestamp_column]

// [START spanner_create_index]
void AddIndex(google::cloud::spanner::DatabaseAdminClient client,
              std::string const& project_id, std::string const& instance_id,
              std::string const& database_id) {
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
      f = client.UpdateDatabase(
          database, {"CREATE INDEX AlbumsByAlbumTitle ON Albums(AlbumTitle)"});
  StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
      metadata = f.get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`AlbumsByAlbumTitle` Index successfully added, new DDL:\n"
            << metadata->DebugString() << "\n";
}
// [END spanner_create_index]

//! [get-database]
void GetDatabase(google::cloud::spanner::DatabaseAdminClient client,
                 std::string const& project_id, std::string const& instance_id,
                 std::string const& database_id) {
  namespace spanner = ::google::cloud::spanner;
  auto database = client.GetDatabase(
      spanner::Database(project_id, instance_id, database_id));
  if (!database) throw std::runtime_error(database.status().message());
  std::cout << "Database metadata is:\n" << database->DebugString() << "\n";
}
//! [get-database]

//! [get-database-ddl]
void GetDatabaseDdl(google::cloud::spanner::DatabaseAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id,
                    std::string const& database_id) {
  namespace spanner = ::google::cloud::spanner;
  auto database = client.GetDatabaseDdl(
      spanner::Database(project_id, instance_id, database_id));
  if (!database) throw std::runtime_error(database.status().message());
  std::cout << "Database metadata is:\n" << database->DebugString() << "\n";
}
//! [get-database-ddl]

//! [update-database] [START spanner_add_column]
void AddColumn(google::cloud::spanner::DatabaseAdminClient client,
               std::string const& project_id, std::string const& instance_id,
               std::string const& database_id) {
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
      f = client.UpdateDatabase(
          database, {"ALTER TABLE Albums ADD COLUMN MarketingBudget INT64"});
  StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
      metadata = f.get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "Added MarketingBudget column\n";
}
//! [update-database] [END spanner_add_column]

// [START spanner_add_timestamp_column]
void AddTimestampColumn(google::cloud::spanner::DatabaseAdminClient client,
                        std::string const& project_id,
                        std::string const& instance_id,
                        std::string const& database_id) {
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
      f = client.UpdateDatabase(
          database, {"ALTER TABLE Albums ADD COLUMN LastUpdateTime TIMESTAMP "
                     "OPTIONS (allow_commit_timestamp=true)"});
  StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
      metadata = f.get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "Added LastUpdateTime column\n";
}
// [END spanner_add_timestamp_column]

// [START spanner_create_storing_index]
void AddStoringIndex(google::cloud::spanner::DatabaseAdminClient client,
                     std::string const& project_id,
                     std::string const& instance_id,
                     std::string const& database_id) {
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
      f = client.UpdateDatabase(database, {R"""(
            CREATE INDEX AlbumsByAlbumTitle2 ON Albums(AlbumTitle)
                STORING (MarketingBudget))"""});
  StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
      metadata = f.get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`AlbumsByAlbumTitle2` Index successfully added, new DDL:\n"
            << metadata->DebugString() << "\n";
}
// [END spanner_create_storing_index]

//! [list-databases]
void ListDatabases(google::cloud::spanner::DatabaseAdminClient client,
                   std::string const& project_id,
                   std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  int count = 0;
  for (auto const& database : client.ListDatabases(in)) {
    if (!database) throw std::runtime_error(database.status().message());
    std::cout << "Database " << database->name() << " full metadata:\n"
              << database->DebugString() << "\n";
    ++count;
  }
  if (count == 0) {
    std::cout << "No databases found in instance " << instance_id
              << " for project << " << project_id << "\n";
  }
}
//! [list-databases]

void ListDatabasesCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw std::runtime_error("list-databases <project-id> <instance-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  ListDatabases(std::move(client), argv[0], argv[1]);
}

//! [create-backup] [START spanner_create_backup]
void CreateBackup(google::cloud::spanner::DatabaseAdminClient client,
                  std::string const& project_id, std::string const& instance_id,
                  std::string const& database_id,
                  std::string const& backup_id) {
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto backup = client
                    .CreateBackup(database, backup_id,
                                  std::chrono::system_clock::now() +
                                      std::chrono::hours(7))
                    .get();
  if (!backup.ok()) throw std::runtime_error(backup.status().message());
  std::cout << "Backup '" << backup->name() << "' of size "
            << backup->size_bytes() << " bytes was created at "
            << google::protobuf::util::TimeUtil::ToString(backup->create_time())
            << ".\n";
}
//! [create-backup] [END spanner_create_backup]

void CreateBackupCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw std::runtime_error(
        "create-backup <project-id> <instance-id>"
        " <database-id> <backup-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  CreateBackup(std::move(client), argv[0], argv[1], argv[2], argv[3]);
}

//! [restore-database] [START spanner_restore_backup]
void RestoreDatabase(google::cloud::spanner::DatabaseAdminClient client,
                     std::string const& project_id,
                     std::string const& instance_id,
                     std::string const& database_id,
                     std::string const& backup_id) {
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  google::cloud::spanner::Backup backup(
      google::cloud::spanner::Instance(project_id, instance_id), backup_id);
  auto restored_db = client.RestoreDatabase(database, backup).get();
  if (!restored_db.ok())
    throw std::runtime_error(restored_db.status().message());
  std::cout << "Database '" << restored_db->name() << "' was restored from "
            << " backup '" << backup.FullName() << "'.\n";
}
//! [restore-database] [END spanner_restore_backup]

void RestoreDatabaseCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw std::runtime_error(
        "restore-backup <project-id> <instance-id>"
        " <database-id> <backup-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  RestoreDatabase(std::move(client), argv[0], argv[1], argv[2], argv[3]);
}

//! [get-backup] [START spanner_get_backup]
void GetBackup(google::cloud::spanner::DatabaseAdminClient client,
               std::string const& project_id, std::string const& instance_id,
               std::string const& backup_id) {
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Backup backup_name(
      google::cloud::spanner::Instance(project_id, instance_id), backup_id);
  auto backup = client.GetBackup(backup_name);
  if (!backup.ok()) throw std::runtime_error(backup.status().message());
  std::cout << "Backup '" << backup->name() << "' of size "
            << backup->size_bytes() << " bytes was created at "
            << google::protobuf::util::TimeUtil::ToString(backup->create_time())
            << ".\n";
}
//! [get-backup] [END spanner_get_backup]

void GetBackupCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "get-backup <project-id> <instance-id> <backup-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  GetBackup(std::move(client), argv[0], argv[1], argv[2]);
}

//! [update-backup] [START spanner_update_backup]
void UpdateBackup(google::cloud::spanner::DatabaseAdminClient client,
                  std::string const& project_id, std::string const& instance_id,
                  std::string const& backup_id) {
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Backup backup_name(
      google::cloud::spanner::Instance(project_id, instance_id), backup_id);
  auto backup = client.UpdateBackupExpireTime(
      backup_name, std::chrono::system_clock::now() + std::chrono::hours(7));
  if (!backup.ok()) throw std::runtime_error(backup.status().message());
  std::cout << "Backup '" << backup->name() << "' updated to new expire_time "
            << google::protobuf::util::TimeUtil::ToString(backup->expire_time())
            << ".\n";
}
//! [update-backup] [END spanner_update_backup]

void UpdateBackupCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "update-backup <project-id> <instance-id> <backup-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  UpdateBackup(std::move(client), argv[0], argv[1], argv[2]);
}

//! [delete-backup] [START spanner_delete_backup]
void DeleteBackup(google::cloud::spanner::DatabaseAdminClient client,
                  std::string const& project_id, std::string const& instance_id,
                  std::string const& backup_id) {
  using ::google::cloud::Status;
  google::cloud::spanner::Backup backup(
      google::cloud::spanner::Instance(project_id, instance_id), backup_id);
  Status status = client.DeleteBackup(backup);
  if (!status.ok()) throw std::runtime_error(status.message());
  std::cout << "Backup '" << backup.FullName() << "' was deleted.\n";
}
//! [delete-backup] [END spanner_delete_backup]

void DeleteBackupCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "delete-backup <project-id> <instance-id>"
        " <backup-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  DeleteBackup(std::move(client), argv[0], argv[1], argv[2]);
}

// [START spanner_cancel_backup_create]
void CreateBackupAndCancel(google::cloud::spanner::DatabaseAdminClient client,
                           std::string const& project_id,
                           std::string const& instance_id,
                           std::string const& database_id,
                           std::string const& backup_id) {
  using ::google::cloud::future;
  using ::google::cloud::Status;
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto f = client.CreateBackup(
      database, backup_id,
      std::chrono::system_clock::now() + std::chrono::hours(7));
  f.cancel();
  auto backup = f.get();
  if (backup.ok()) {
    Status status = client.DeleteBackup(backup.value());
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Backup '" << backup->name() << "' was deleted.\n";
  } else {
    std::cout << "CreateBackup operation was cancelled with the message '"
              << backup.status().message() << "'.\n";
  }
}
// [END spanner_cancel_backup_create]

void CreateBackupAndCancelCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw std::runtime_error(
        "create-backup-and-cancel <project-id> <instance-id>"
        " <database-id> <backup-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  CreateBackupAndCancel(std::move(client), argv[0], argv[1], argv[2], argv[3]);
}

//! [list-backups] [START spanner_list_backups]
void ListBackups(google::cloud::spanner::DatabaseAdminClient client,
                 std::string const& project_id,
                 std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  std::cout << "All backups:\n";
  for (auto const& backup : client.ListBackups(in)) {
    if (!backup) throw std::runtime_error(backup.status().message());
    std::cout << "Backup " << backup->name() << " on database "
              << backup->database() << " with size : " << backup->size_bytes()
              << " bytes.\n";
  }
}
//! [list-backups] [END spanner_list_backups]

void ListBackupsCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw std::runtime_error("list-backups <project-id> <instance-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  ListBackups(std::move(client), argv[0], argv[1]);
}

//! [list-backup-operations] [START spanner_list_backup_operations]
void ListBackupOperations(google::cloud::spanner::DatabaseAdminClient client,
                          std::string const& project_id,
                          std::string const& instance_id,
                          std::string const& database_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  auto filter = std::string("(metadata.database:") + database_id + ") AND " +
                "(metadata.@type:type.googleapis.com/" +
                "google.spanner.admin.database.v1.CreateBackupMetadata)";
  for (auto const& operation : client.ListBackupOperations(in, filter)) {
    if (!operation) throw std::runtime_error(operation.status().message());
    google::spanner::admin::database::v1::CreateBackupMetadata metadata;
    operation->metadata().UnpackTo(&metadata);
    std::cout << "Backup " << metadata.name() << " on database "
              << metadata.database()
              << " progress: " << metadata.progress().progress_percent()
              << "% complete.";
  }
}
//! [list-backup-operations] [END spanner_list_backup_operations]

//! [list-database-operations] [START spanner_list_database_operations]
void ListDatabaseOperations(google::cloud::spanner::DatabaseAdminClient client,
                            std::string const& project_id,
                            std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  auto filter = std::string(
      "(metadata.@type:type.googleapis.com/"
      "google.spanner.admin.database.v1.OptimizeRestoredDatabaseMetadata)");
  for (auto const& operation : client.ListDatabaseOperations(in, filter)) {
    if (!operation) throw std::runtime_error(operation.status().message());
    google::spanner::admin::database::v1::OptimizeRestoredDatabaseMetadata
        metadata;
    operation->metadata().UnpackTo(&metadata);
    std::cout << "Database " << metadata.name() << " restored from backup is "
              << metadata.progress().progress_percent() << "% optimized.";
  }
}
//! [list-database-operations] [END spanner_list_database_operations]

void ListDatabaseOperationsCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 2) {
    throw std::runtime_error(
        "list-database-operations <project-id> <instance-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  ListDatabaseOperations(std::move(client), argv[0], argv[1]);
}

//! [drop-database] [START spanner_drop_database]
void DropDatabase(google::cloud::spanner::DatabaseAdminClient client,
                  std::string const& project_id, std::string const& instance_id,
                  std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto status = client.DropDatabase(database);
  if (!status.ok()) throw std::runtime_error(status.message());
  std::cout << "Database " << database << " successfully dropped\n";
}
//! [drop-database] [END spanner_drop_database]

//! [database-get-iam-policy]
void DatabaseGetIamPolicy(google::cloud::spanner::DatabaseAdminClient client,
                          std::string const& project_id,
                          std::string const& instance_id,
                          std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto actual = client.GetIamPolicy(database);
  if (!actual) throw std::runtime_error(actual.status().message());

  std::cout << "The IAM policy for database " << database_id << " is:\n"
            << actual->DebugString() << "\n";
}
//! [database-get-iam-policy]

//! [add-database-reader-on-database]
void AddDatabaseReaderOnDatabase(
    google::cloud::spanner::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id, std::string const& new_reader) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto current = client.GetIamPolicy(database);
  if (!current) throw std::runtime_error(current.status().message());

  // Find (or create) the binding for "roles/spanner.databaseReader".
  auto& binding = [&current]() -> google::iam::v1::Binding& {
    auto role_pos =
        std::find_if(current->mutable_bindings()->begin(),
                     current->mutable_bindings()->end(),
                     [](google::iam::v1::Binding const& b) {
                       return b.role() == "roles/spanner.databaseReader" &&
                              !b.has_condition();
                     });
    if (role_pos != current->mutable_bindings()->end()) {
      return *role_pos;
    }
    auto& binding = *current->add_bindings();
    binding.set_role("roles/spanner.databaseReader");
    return binding;
  }();

  auto member_pos =
      std::find(binding.members().begin(), binding.members().end(), new_reader);
  if (member_pos != binding.members().end()) {
    std::cout << "The entity " << new_reader
              << " is already a database reader:\n"
              << current->DebugString();
    return;
  }

  binding.add_members(new_reader);
  auto result = client.SetIamPolicy(database, *std::move(current));
  if (!result) throw std::runtime_error(result.status().message());

  std::cout << "Successfully added " << new_reader
            << " to the database reader role:\n"
            << result->DebugString() << "\n";
}
//! [add-database-reader-on-database]

void AddDatabaseReaderOnDatabaseCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw std::runtime_error(
        "add-database-reader-on-database <project-id> <instance-id>"
        " <database-id> <new-reader>");
  }
  google::cloud::spanner::DatabaseAdminClient client(
      google::cloud::spanner::MakeDatabaseAdminConnection());
  AddDatabaseReaderOnDatabase(std::move(client), argv[0], argv[1], argv[2],
                              argv[3]);
}

//! [database-test-iam-permissions]
void DatabaseTestIamPermissions(
    google::cloud::spanner::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id, std::string const& permission) {
  google::cloud::spanner::Database db(project_id, instance_id, database_id);
  auto actual = client.TestIamPermissions(db, {permission});
  if (!actual) throw std::runtime_error(actual.status().message());

  char const* msg = actual->permissions().empty() ? "does not" : "does";

  std::cout << "The caller " << msg << " have permission '" << permission
            << "' on the Cloud Spanner database " << db.database_id() << "\n";
}
//! [database-test-iam-permissions]

void DatabaseTestIamPermissionsCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw std::runtime_error(
        "database-test-iam-permissions <project-id> <instance-id>"
        " <database-id> <permission>");
  }
  google::cloud::spanner::DatabaseAdminClient client(
      google::cloud::spanner::MakeDatabaseAdminConnection());
  DatabaseTestIamPermissions(std::move(client), argv[0], argv[1], argv[2],
                             argv[3]);
}

//! [quickstart] [START spanner_quickstart]
void Quickstart(std::string const& project_id, std::string const& instance_id,
                std::string const& database_id) {
  namespace spanner = ::google::cloud::spanner;

  //! [START init_client]
  auto database = spanner::Database(project_id, instance_id, database_id);
  auto connection = spanner::MakeConnection(database);
  auto client = spanner::Client(connection);
  //! [END init_client]

  auto rows =
      client.ExecuteQuery(spanner::SqlStatement("SELECT 'Hello World'"));

  using RowType = std::tuple<std::string>;
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << std::get<0>(*row) << "\n";
  }
}
//! [quickstart] [END spanner_quickstart]

void QuickstartCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "quickstart <project-id> <instance-id> <database-id>");
  }
  Quickstart(argv[0], argv[1], argv[2]);
}

google::cloud::spanner::Client MakeSampleClient(
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  namespace spanner = ::google::cloud::spanner;
  return spanner::Client(spanner::MakeConnection(
      spanner::Database(project_id, instance_id, database_id)));
}

//! [START spanner_insert_data]
void InsertData(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto insert_singers = spanner::InsertMutationBuilder(
                            "Singers", {"SingerId", "FirstName", "LastName"})
                            .EmplaceRow(1, "Marc", "Richards")
                            .EmplaceRow(2, "Catalina", "Smith")
                            .EmplaceRow(3, "Alice", "Trentor")
                            .EmplaceRow(4, "Lea", "Martin")
                            .EmplaceRow(5, "David", "Lomond")
                            .Build();

  auto insert_albums = spanner::InsertMutationBuilder(
                           "Albums", {"SingerId", "AlbumId", "AlbumTitle"})
                           .EmplaceRow(1, 1, "Total Junk")
                           .EmplaceRow(1, 2, "Go, Go, Go")
                           .EmplaceRow(2, 1, "Green")
                           .EmplaceRow(2, 2, "Forever Hold Your Peace")
                           .EmplaceRow(2, 3, "Terrified")
                           .Build();

  auto commit_result =
      client.Commit(spanner::Mutations{insert_singers, insert_albums});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful [spanner_insert_data]\n";
}
//! [END spanner_insert_data]

//! [START spanner_update_data]
void UpdateData(google::cloud::spanner::Client client) {
  //! [commit-with-mutations]
  namespace spanner = ::google::cloud::spanner;
  auto commit_result = client.Commit(spanner::Mutations{
      spanner::UpdateMutationBuilder("Albums",
                                     {"SingerId", "AlbumId", "MarketingBudget"})
          .EmplaceRow(1, 1, 100000)
          .EmplaceRow(2, 2, 500000)
          .Build()});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  //! [commit-with-mutations]
  std::cout << "Update was successful [spanner_update_data]\n";
}
//! [END spanner_update_data]

//! [START spanner_delete_data]
void DeleteData(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  // Delete the albums with key (2,1) and (2,3).
  //! [make-key] [keyset-add-key]
  auto delete_albums = spanner::DeleteMutationBuilder(
                           "Albums", spanner::KeySet()
                                         .AddKey(spanner::MakeKey(2, 1))
                                         .AddKey(spanner::MakeKey(2, 3)))
                           .Build();
  //! [make-key] [keyset-add-key]

  // Delete some singers using the keys in the range [3, 5]
  //! [make-keybound-closed]
  auto delete_singers_range =
      spanner::DeleteMutationBuilder(
          "Singers", spanner::KeySet().AddRange(spanner::MakeKeyBoundClosed(3),
                                                spanner::MakeKeyBoundOpen(5)))
          .Build();
  //! [make-keybound-closed]

  // Deletes remaining rows from the Singers table and the Albums table, because
  // the Albums table is defined with ON DELETE CASCADE.
  auto delete_singers_all =
      spanner::MakeDeleteMutation("Singers", spanner::KeySet::All());

  auto commit_result = client.Commit(spanner::Mutations{
      delete_albums, delete_singers_range, delete_singers_all});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Delete was successful [spanner_delete_data]\n";
}
//! [END spanner_delete_data]

//! [START spanner_insert_datatypes_data]
void InsertDatatypesData(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  std::vector<absl::CivilDay> available_dates1 = {absl::CivilDay(2020, 12, 1),
                                                  absl::CivilDay(2020, 12, 2),
                                                  absl::CivilDay(2020, 12, 3)};
  std::vector<absl::CivilDay> available_dates2 = {absl::CivilDay(2020, 11, 1),
                                                  absl::CivilDay(2020, 11, 5),
                                                  absl::CivilDay(2020, 11, 15)};
  std::vector<absl::CivilDay> available_dates3 = {absl::CivilDay(2020, 10, 1),
                                                  absl::CivilDay(2020, 10, 7)};
  auto insert_venues =
      spanner::InsertMutationBuilder(
          "Venues", {"VenueId", "VenueName", "VenueInfo", "Capacity",
                     "AvailableDates", "LastContactDate", "OutdoorVenue",
                     "PopularityScore", "LastUpdateTime"})
          .EmplaceRow(4, "Venue 4", spanner::Bytes("Hello World 1"), 1800,
                      available_dates1, absl::CivilDay(2018, 9, 2), false,
                      0.85543, spanner::CommitTimestamp())
          .EmplaceRow(19, "Venue 19", spanner::Bytes("Hello World 2"), 6300,
                      available_dates2, absl::CivilDay(2019, 1, 15), true,
                      0.98716, spanner::CommitTimestamp())
          .EmplaceRow(42, "Venue 42", spanner::Bytes("Hello World 3"), 3000,
                      available_dates3, absl::CivilDay(2018, 10, 1), false,
                      0.72598, spanner::CommitTimestamp())
          .Build();

  auto commit_result = client.Commit(spanner::Mutations{insert_venues});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful [spanner_insert_datatypes_data]\n";
}
//! [END spanner_insert_datatypes_data]

//! [START spanner_query_with_array_parameter]
void QueryWithArrayParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  std::vector<absl::CivilDay> example_array = {absl::CivilDay(2020, 10, 1),
                                               absl::CivilDay(2020, 11, 1)};
  spanner::SqlStatement select(
      "SELECT VenueId, VenueName, AvailableDate FROM Venues v,"
      " UNNEST(v.AvailableDates) as AvailableDate "
      " WHERE AvailableDate in UNNEST(@available_dates)",
      {{"available_dates", spanner::Value(example_array)}});
  using RowType = std::tuple<std::int64_t, absl::optional<std::string>,
                             absl::optional<absl::CivilDay>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "VenueId: " << std::get<0>(*row) << "\t";
    std::cout << "VenueName: " << std::get<1>(*row).value() << "\t";
    std::cout << "AvailableDate: " << std::get<2>(*row).value() << "\n";
  }
  std::cout << "Query completed for [spanner_query_with_array_parameter]\n";
}
//! [END spanner_query_with_array_parameter]

//! [START spanner_query_with_bool_parameter]
void QueryWithBoolParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  bool example_bool = true;
  spanner::SqlStatement select(
      "SELECT VenueId, VenueName, OutdoorVenue FROM Venues"
      " WHERE OutdoorVenue = @outdoor_venue",
      {{"outdoor_venue", spanner::Value(example_bool)}});
  using RowType = std::tuple<std::int64_t, absl::optional<std::string>,
                             absl::optional<bool>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "VenueId: " << std::get<0>(*row) << "\t";
    std::cout << "VenueName: " << std::get<1>(*row).value() << "\t";
    std::cout << "OutdoorVenue: " << std::get<2>(*row).value() << "\n";
  }
  std::cout << "Query completed for [spanner_query_with_bool_parameter]\n";
}
//! [END spanner_query_with_bool_parameter]

//! [START spanner_query_with_bytes_parameter]
void QueryWithBytesParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  spanner::Bytes example_bytes("Hello World 1");
  spanner::SqlStatement select(
      "SELECT VenueId, VenueName FROM Venues"
      " WHERE VenueInfo = @venue_info",
      {{"venue_info", spanner::Value(example_bytes)}});
  using RowType = std::tuple<std::int64_t, absl::optional<std::string>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "VenueId: " << std::get<0>(*row) << "\t";
    std::cout << "VenueName: " << std::get<1>(*row).value() << "\n";
  }
  std::cout << "Query completed for [spanner_query_with_bytes_parameter]\n";
}
//! [END spanner_query_with_bytes_parameter]

//! [START spanner_query_with_date_parameter]
void QueryWithDateParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  absl::CivilDay example_date(2019, 1, 1);
  spanner::SqlStatement select(
      "SELECT VenueId, VenueName, LastContactDate FROM Venues"
      " WHERE LastContactDate < @last_contact_date",
      {{"last_contact_date", spanner::Value(example_date)}});
  using RowType = std::tuple<std::int64_t, absl::optional<std::string>,
                             absl::optional<absl::CivilDay>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "VenueId: " << std::get<0>(*row) << "\t";
    std::cout << "VenueName: " << std::get<1>(*row).value() << "\t";
    std::cout << "LastContactDate: " << std::get<2>(*row).value() << "\n";
  }
  std::cout << "Query completed for [spanner_query_with_date_parameter]\n";
}
//! [END spanner_query_with_date_parameter]

//! [START spanner_query_with_float_parameter]
void QueryWithFloatParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  double example_float = 0.8;
  spanner::SqlStatement select(
      "SELECT VenueId, VenueName, PopularityScore FROM Venues"
      " WHERE PopularityScore > @popularity_score",
      {{"popularity_score", spanner::Value(example_float)}});
  using RowType = std::tuple<std::int64_t, absl::optional<std::string>,
                             absl::optional<double>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "VenueId: " << std::get<0>(*row) << "\t";
    std::cout << "VenueName: " << std::get<1>(*row).value() << "\t";
    std::cout << "PopularityScore: " << std::get<2>(*row).value() << "\n";
  }
  std::cout << "Query completed for [spanner_query_with_float_parameter]\n";
}
//! [END spanner_query_with_float_parameter]

//! [START spanner_query_with_int_parameter]
void QueryWithIntParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  int example_int = 3000;
  spanner::SqlStatement select(
      "SELECT VenueId, VenueName, Capacity FROM Venues"
      " WHERE Capacity >= @capacity",
      {{"capacity", spanner::Value(example_int)}});
  using RowType = std::tuple<std::int64_t, absl::optional<std::string>,
                             absl::optional<std::int64_t>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "VenueId: " << std::get<0>(*row) << "\t";
    std::cout << "VenueName: " << std::get<1>(*row).value() << "\t";
    std::cout << "Capacity: " << std::get<2>(*row).value() << "\n";
  }
  std::cout << "Query completed for [spanner_query_with_int_parameter]\n";
}
//! [END spanner_query_with_int_parameter]

//! [START spanner_query_with_string_parameter]
void QueryWithStringParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  std::string example_string = "Venue 42";
  spanner::SqlStatement select(
      "SELECT VenueId, VenueName FROM Venues"
      " WHERE VenueName = @venue_name",
      {{"venue_name", spanner::Value(example_string)}});
  using RowType = std::tuple<std::int64_t, absl::optional<std::string>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "VenueId: " << std::get<0>(*row) << "\t";
    std::cout << "VenueName: " << std::get<1>(*row).value() << "\n";
  }
  std::cout << "Query completed for [spanner_query_with_string_parameter]\n";
}
//! [END spanner_query_with_string_parameter]

//! [START spanner_query_with_timestamp_parameter]
void QueryWithTimestampParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto example_timestamp =
      spanner::MakeTimestamp(std::chrono::system_clock::now()).value();
  spanner::SqlStatement select(
      "SELECT VenueId, VenueName, LastUpdateTime FROM Venues"
      " WHERE LastUpdateTime <= @last_update_time",
      {{"last_update_time", spanner::Value(example_timestamp)}});
  using RowType = std::tuple<std::int64_t, absl::optional<std::string>,
                             absl::optional<spanner::Timestamp>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "VenueId: " << std::get<0>(*row) << "\t";
    std::cout << "VenueName: " << std::get<1>(*row).value() << "\t";
    std::cout << "LastUpdateTime: " << std::get<2>(*row).value() << "\n";
  }
  std::cout << "Query completed for [spanner_query_with_timestamp_parameter]\n";
}
//! [END spanner_query_with_timestamp_parameter]

//! [keyset-all]
void DeleteAll(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  // Delete all the performances, venues, albums and singers.
  auto commit = client.Commit(spanner::Mutations{
      spanner::MakeDeleteMutation("Performances", spanner::KeySet::All()),
      spanner::MakeDeleteMutation("Venues", spanner::KeySet::All()),
      spanner::MakeDeleteMutation("Albums", spanner::KeySet::All()),
      spanner::MakeDeleteMutation("Singers", spanner::KeySet::All()),
  });
  if (!commit) throw std::runtime_error(commit.status().message());
  std::cout << "delete-all was successful\n";
}
//! [keyset-all]

//! [insert-mutation-builder]
void InsertMutationBuilder(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit = client.Commit(
      spanner::Mutations{spanner::InsertMutationBuilder(
                             "Singers", {"SingerId", "FirstName", "LastName"})
                             .EmplaceRow(1, "Marc", "Richards")
                             .EmplaceRow(2, "Catalina", "Smith")
                             .EmplaceRow(3, "Alice", "Trentor")
                             .EmplaceRow(4, "Lea", "Martin")
                             .EmplaceRow(5, "David", "Lomond")
                             .Build(),
                         spanner::InsertMutationBuilder(
                             "Albums", {"SingerId", "AlbumId", "AlbumTitle"})
                             .EmplaceRow(1, 1, "Total Junk")
                             .EmplaceRow(1, 2, "Go, Go, Go")
                             .EmplaceRow(2, 1, "Green")
                             .EmplaceRow(2, 2, "Forever Hold Your Peace")
                             .EmplaceRow(2, 3, "Terrified")
                             .Build()});
  if (!commit) throw std::runtime_error(commit.status().message());
  std::cout << "insert-mutation-builder was successful\n";
}
//! [insert-mutation-builder]

//! [make-insert-mutation]
void MakeInsertMutation(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  // Delete each of the albums by individual key, then delete all the singers
  // using a key range.
  auto commit_result = client.Commit(spanner::Mutations{
      spanner::InsertOrUpdateMutationBuilder(
          "Performances",
          {"SingerId", "VenueId", "EventDate", "Revenue", "LastUpdateTime"})
          .EmplaceRow(1, 4, absl::CivilDay(2017, 10, 5), 11000,
                      spanner::CommitTimestamp{})
          .EmplaceRow(1, 19, absl::CivilDay(2017, 11, 2), 15000,
                      spanner::CommitTimestamp{})
          .Build()});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "make-insert-mutation was successful\n";
}
//! [make-insert-mutation]

//! [update-mutation-builder]
void UpdateMutationBuilder(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit = client.Commit(spanner::Mutations{
      spanner::UpdateMutationBuilder("Albums",
                                     {"SingerId", "AlbumId", "MarketingBudget"})
          .EmplaceRow(1, 1, 100000)
          .EmplaceRow(2, 2, 500000)
          .Build()});
  if (!commit) throw std::runtime_error(commit.status().message());
  std::cout << "update-mutation-builder was successful\n";
}
//! [update-mutation-builder]

//! [make-update-mutation]
void MakeUpdateMutation(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit = client.Commit(spanner::Mutations{
      spanner::MakeUpdateMutation(
          "Albums", {"SingerId", "AlbumId", "MarketingBudget"}, 1, 1, 200000),
  });
  if (!commit) throw std::runtime_error(commit.status().message());
  std::cout << "make-update-mutation was succesful\n";
}
//! [make-update-mutation]

//! [insert-or-update-mutation-builder]
void InsertOrUpdateMutationBuilder(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit = client.Commit(spanner::Mutations{
      spanner::InsertOrUpdateMutationBuilder(
          "Albums", {"SingerId", "AlbumId", "AlbumTitle", "MarketingBudget"})
          .EmplaceRow(1, 1, "Total Junk", 100000)
          .EmplaceRow(1, 2, "Go, Go, Go", 200000)
          .EmplaceRow(2, 1, "Green", 300000)
          .EmplaceRow(2, 2, "Forever Hold Your Peace", 400000)
          .EmplaceRow(2, 3, "Terrified", 500000)
          .Build()});
  if (!commit) throw std::runtime_error(commit.status().message());

  std::cout << "insert-or-update-mutation-builder was successful\n";
}
//! [insert-or-update-mutation-builder]

//! [make-insert-or-update-mutation]
void MakeInsertOrUpdateMutation(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit = client.Commit(spanner::Mutations{
      spanner::MakeInsertOrUpdateMutation(
          "Albums", {"SingerId", "AlbumId", "AlbumTitle", "MarketingBudget"}, 1,
          1, "Total Junk", 200000),
  });
  if (!commit) throw std::runtime_error(commit.status().message());

  std::cout << "make-insert-or-update-mutation was successful\n";
}
//! [make-insert-or-update-mutation]

//! [replace-mutation-builder]
void ReplaceMutationBuilder(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit = client.Commit(spanner::Mutations{
      spanner::ReplaceMutationBuilder(
          "Albums", {"SingerId", "AlbumId", "AlbumTitle", "MarketingBudget"})
          .EmplaceRow(1, 1, "Total Junk", 500000)
          .EmplaceRow(1, 2, "Go, Go, Go", 400000)
          .EmplaceRow(2, 1, "Green", 300000)
          .Build()});
  if (!commit) throw std::runtime_error(commit.status().message());

  std::cout << "replace-mutation-builder was successful\n";
}
//! [replace-mutation-builder]

//! [make-replace-mutation]
void MakeReplaceMutation(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit = client.Commit(spanner::Mutations{
      spanner::MakeReplaceMutation(
          "Albums", {"SingerId", "AlbumId", "AlbumTitle", "MarketingBudget"}, 1,
          1, "Go, Go, Go", 600000),
  });
  if (!commit) throw std::runtime_error(commit.status().message());

  std::cout << "make-replace-mutation was successful\n";
}
//! [make-replace-mutation]

//! [delete-mutation-builder]
void DeleteMutationBuilder(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit = client.Commit(
      spanner::Mutations{spanner::DeleteMutationBuilder(
                             "Albums", spanner::KeySet()
                                           .AddKey(spanner::MakeKey(1, 1))
                                           .AddKey(spanner::MakeKey(1, 2)))
                             .Build()});
  if (!commit) throw std::runtime_error(commit.status().message());

  std::cout << "delete-mutation-builder was successful\n";
}
//! [delete-mutation-builder]

//! [make-delete-mutation]
void MakeDeleteMutation(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit = client.Commit(spanner::Mutations{
      spanner::MakeDeleteMutation("Albums", spanner::KeySet::All()),
  });
  if (!commit) throw std::runtime_error(commit.status().message());

  std::cout << "make-delete-mutation was successful\n";
}
//! [make-delete-mutation]

// [START spanner_insert_data_with_timestamp_column]
void InsertDataWithTimestamp(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit_result = client.Commit(spanner::Mutations{
      spanner::InsertOrUpdateMutationBuilder(
          "Performances",
          {"SingerId", "VenueId", "EventDate", "Revenue", "LastUpdateTime"})
          .EmplaceRow(1, 4, absl::CivilDay(2017, 10, 5), 11000,
                      spanner::CommitTimestamp{})
          .EmplaceRow(1, 19, absl::CivilDay(2017, 11, 2), 15000,
                      spanner::CommitTimestamp{})
          .EmplaceRow(2, 42, absl::CivilDay(2017, 12, 23), 7000,
                      spanner::CommitTimestamp{})
          .Build()});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout
      << "Update was successful [spanner_insert_data_with_timestamp_column]\n";
}
// [END spanner_insert_data_with_timestamp_column]

// [START spanner_update_data_with_timestamp_column]
void UpdateDataWithTimestamp(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit_result = client.Commit(spanner::Mutations{
      spanner::UpdateMutationBuilder(
          "Albums",
          {"SingerId", "AlbumId", "MarketingBudget", "LastUpdateTime"})
          .EmplaceRow(1, 1, 1000000, spanner::CommitTimestamp{})
          .EmplaceRow(2, 2, 750000, spanner::CommitTimestamp{})
          .Build()});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout
      << "Update was successful [spanner_update_data_with_timestamp_column]\n";
}
// [END spanner_update_data_with_timestamp_column]

// [START spanner_query_data_with_timestamp_column]
void QueryDataWithTimestamp(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  spanner::SqlStatement select(
      "SELECT SingerId, AlbumId, MarketingBudget, LastUpdateTime"
      "  FROM Albums"
      " ORDER BY LastUpdateTime DESC");
  using RowType =
      std::tuple<std::int64_t, std::int64_t, absl::optional<std::int64_t>,
                 absl::optional<spanner::Timestamp>>;

  auto rows = client.ExecuteQuery(select);
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << std::get<0>(*row) << " " << std::get<1>(*row);
    auto marketing_budget = std::get<2>(*row);
    if (!marketing_budget) {
      std::cout << " NULL";
    } else {
      std::cout << ' ' << *marketing_budget;
    }
    auto last_update_time = std::get<3>(*row);
    if (!last_update_time) {
      std::cout << " NULL";
    } else {
      std::cout << ' ' << *last_update_time;
    }
    std::cout << "\n";
  }
}
// [END spanner_query_data_with_timestamp_column]

// [START spanner_add_numeric_column]
void AddNumericColumn(google::cloud::spanner::DatabaseAdminClient client,
                      std::string const& project_id,
                      std::string const& instance_id,
                      std::string const& database_id) {
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
      f = client.UpdateDatabase(database, {R"""(
            ALTER TABLE Venues ADD COLUMN Revenue NUMERIC)"""});
  StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
      metadata = f.get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`Venues` table altered, new DDL:\n"
            << metadata->DebugString() << "\n";
}
// [END spanner_add_numeric_column]

//! [START spanner_update_data_with_numeric_column]
void UpdateDataWithNumeric(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto insert_venues =
      spanner::InsertMutationBuilder(
          "Venues", {"VenueId", "VenueName", "Revenue", "LastUpdateTime"})
          .EmplaceRow(1, "Venue 1", spanner::MakeNumeric(35000).value(),
                      spanner::CommitTimestamp())
          .EmplaceRow(6, "Venue 6", spanner::MakeNumeric(104500).value(),
                      spanner::CommitTimestamp())
          .EmplaceRow(
              14, "Venue 14",
              spanner::MakeNumeric("99999999999999999999999999999.99").value(),
              spanner::CommitTimestamp())
          .Build();

  auto commit_result = client.Commit(spanner::Mutations{insert_venues});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful [spanner_update_data_with_numeric]\n";
}
//! [END spanner_update_data_with_numeric_column]

// [START spanner_query_with_numeric_parameter]
void QueryWithNumericParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  auto revenue = spanner::MakeNumeric(100000).value();
  spanner::SqlStatement select(
      "SELECT VenueId, Revenue"
      "  FROM Venues"
      " WHERE Revenue < @revenue",
      {{"revenue", spanner::Value(std::move(revenue))}});
  using RowType = std::tuple<std::int64_t, absl::optional<spanner::Numeric>>;

  auto rows = client.ExecuteQuery(select);
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "VenueId: " << std::get<0>(*row) << "\t";
    auto revenue = std::get<1>(*row).value();
    std::cout << "Revenue: " << revenue.ToString()
              << " (d.16=" << std::setprecision(16)
              << spanner::ToDouble(revenue)
              << ", i*10^2=" << spanner::ToInteger<int>(revenue, 2).value()
              << ")\n";
  }
}
// [END spanner_query_with_numeric_parameter]

//! [START spanner_read_only_transaction]
void ReadOnlyTransaction(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto read_only = spanner::MakeReadOnlyTransaction();

  spanner::SqlStatement select(
      "SELECT SingerId, AlbumId, AlbumTitle FROM Albums");
  using RowType = std::tuple<std::int64_t, std::int64_t, std::string>;

  // Read#1.
  auto rows1 = client.ExecuteQuery(read_only, select);
  std::cout << "Read 1 results\n";
  for (auto const& row : spanner::StreamOf<RowType>(rows1)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row)
              << " AlbumId: " << std::get<1>(*row)
              << " AlbumTitle: " << std::get<2>(*row) << "\n";
  }
  // Read#2. Even if changes occur in-between the reads the transaction ensures
  // that Read #1 and Read #2 return the same data.
  auto rows2 = client.ExecuteQuery(read_only, select);
  std::cout << "Read 2 results\n";
  for (auto const& row : spanner::StreamOf<RowType>(rows2)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row)
              << " AlbumId: " << std::get<1>(*row)
              << " AlbumTitle: " << std::get<2>(*row) << "\n";
  }
}
//! [END spanner_read_only_transaction]

//! [START spanner_read_stale_data]
void ReadStaleData(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto opts = spanner::Transaction::ReadOnlyOptions(std::chrono::seconds(15));
  auto read_only = spanner::MakeReadOnlyTransaction(opts);

  spanner::SqlStatement select(
      "SELECT SingerId, AlbumId, AlbumTitle FROM Albums");
  using RowType = std::tuple<std::int64_t, std::int64_t, std::string>;

  auto rows = client.ExecuteQuery(read_only, select);
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row)
              << " AlbumId: " << std::get<1>(*row)
              << " AlbumTitle: " << std::get<2>(*row) << "\n";
  }
}
//! [END spanner_read_stale_data]

//! [START spanner_batch_client]
void UsePartitionQuery(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto txn = spanner::MakeReadOnlyTransaction();

  spanner::SqlStatement select(
      "SELECT SingerId, FirstName, LastName FROM Singers");
  using RowType = std::tuple<std::int64_t, std::string, std::string>;

  auto partitions = client.PartitionQuery(std::move(txn), select, {});
  if (!partitions) throw std::runtime_error(partitions.status().message());
  int number_of_rows = 0;
  for (auto const& partition : *partitions) {
    auto rows = client.ExecuteQuery(partition);
    for (auto const& row : spanner::StreamOf<RowType>(rows)) {
      if (!row) throw std::runtime_error(row.status().message());
      number_of_rows++;
    }
  }
  std::cout << "Number of partitions: " << partitions->size() << "\n"
            << "Number of rows: " << number_of_rows << "\n";
  std::cout << "Read completed for [spanner_batch_client]\n";
}
//! [END spanner_batch_client]

//! [START spanner_read_data_with_index]
void ReadDataWithIndex(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  spanner::ReadOptions read_options;
  read_options.index_name = "AlbumsByAlbumTitle";
  auto rows = client.Read("Albums", google::cloud::spanner::KeySet::All(),
                          {"AlbumId", "AlbumTitle"}, read_options);
  using RowType = std::tuple<std::int64_t, std::string>;
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "AlbumId: " << std::get<0>(*row) << "\t";
    std::cout << "AlbumTitle: " << std::get<1>(*row) << "\n";
  }
  std::cout << "Read completed for [spanner_read_data_with_index]\n";
}
//! [END spanner_read_data_with_index]

//! [START spanner_query_data_with_new_column]
void QueryNewColumn(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  spanner::SqlStatement select(
      "SELECT SingerId, AlbumId, MarketingBudget FROM Albums");

  using RowType =
      std::tuple<std::int64_t, std::int64_t, absl::optional<std::int64_t>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\t";
    std::cout << "AlbumId: " << std::get<1>(*row) << "\t";
    auto marketing_budget = std::get<2>(*row);
    if (marketing_budget) {
      std::cout << "MarketingBudget: " << marketing_budget.value() << "\n";
    } else {
      std::cout << "MarketingBudget: NULL\n";
    }
  }
  std::cout << "Read completed for [spanner_read_data_with_new_column]\n";
}
//! [END spanner_query_data_with_new_column]

void ProfileQuery(google::cloud::spanner::Client client) {
  //! [profile-query]
  namespace spanner = ::google::cloud::spanner;
  spanner::SqlStatement select(
      "SELECT AlbumId, AlbumTitle, MarketingBudget"
      " FROM Albums"
      " WHERE AlbumTitle >= 'Aardvark' AND AlbumTitle < 'Goo'");
  auto profile_query_result = client.ProfileQuery(std::move(select));
  for (auto const& row : profile_query_result) {
    if (!row) throw std::runtime_error(row.status().message());
    // Discard rows for brevity in this example.
  }

  // Stats are only available after all rows from the result have been read.
  auto execution_stats = profile_query_result.ExecutionStats();
  if (execution_stats) {
    for (auto const& stat : *execution_stats) {
      std::cout << stat.first << ":\t" << stat.second << "\n";
    }
  }
  //! [profile-query]
}

//! [START spanner_query_data_with_index]
void QueryUsingIndex(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  spanner::SqlStatement select(
      "SELECT AlbumId, AlbumTitle, MarketingBudget"
      " FROM Albums@{FORCE_INDEX=AlbumsByAlbumTitle}"
      " WHERE AlbumTitle >= @start_title AND AlbumTitle < @end_title",
      {{"start_title", spanner::Value("Aardvark")},
       {"end_title", spanner::Value("Goo")}});
  using RowType =
      std::tuple<std::int64_t, std::string, absl::optional<std::int64_t>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "AlbumId: " << std::get<0>(*row) << "\t";
    std::cout << "AlbumTitle: " << std::get<1>(*row) << "\t";
    auto marketing_budget = std::get<2>(*row);
    if (marketing_budget) {
      std::cout << "MarketingBudget: " << marketing_budget.value() << "\n";
    } else {
      std::cout << "MarketingBudget: NULL\n";
    }
  }
  std::cout << "Read completed for [spanner_query_data_with_index]\n";
}
//! [END spanner_query_data_with_index]

void CreateClientWithQueryOptions(std::string const& project_id,
                                  std::string const& instance_id,
                                  std::string const& db_id) {
  auto db = ::google::cloud::spanner::Database(project_id, instance_id, db_id);
  //! [START spanner_create_client_with_query_options]
  namespace spanner = ::google::cloud::spanner;
  spanner::Client client(
      spanner::MakeConnection(db),
      spanner::ClientOptions().set_query_options(
          spanner::QueryOptions().set_optimizer_version("1")));
  //! [END spanner_create_client_with_query_options]
}

void CreateClientWithQueryOptionsCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "create-client-with-query-options <project-id> <instance-id> "
        "<database-id>");
  }
  CreateClientWithQueryOptions(argv[0], argv[1], argv[2]);
}

//! [START spanner_query_with_query_options]
void QueryWithQueryOptions(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto sql = spanner::SqlStatement("SELECT SingerId, FirstName FROM Singers");
  auto opts = spanner::QueryOptions().set_optimizer_version("1");
  auto rows = client.ExecuteQuery(std::move(sql), std::move(opts));

  using RowType = std::tuple<std::int64_t, std::string>;
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\t";
    std::cout << "FirstName: " << std::get<1>(*row) << "\n";
  }
  std::cout << "Read completed for [spanner_query_with_query_options]\n";
}
//! [END spanner_query_with_query_options]

//! [START spanner_read_data_with_storing_index]
void ReadDataWithStoringIndex(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  spanner::ReadOptions read_options;
  read_options.index_name = "AlbumsByAlbumTitle2";
  auto rows =
      client.Read("Albums", google::cloud::spanner::KeySet::All(),
                  {"AlbumId", "AlbumTitle", "MarketingBudget"}, read_options);
  using RowType =
      std::tuple<std::int64_t, std::string, absl::optional<std::int64_t>>;
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "AlbumId: " << std::get<0>(*row) << "\t";
    std::cout << "AlbumTitle: " << std::get<1>(*row) << "\t";
    auto marketing_budget = std::get<2>(*row);
    if (marketing_budget) {
      std::cout << "MarketingBudget: " << marketing_budget.value() << "\n";
    } else {
      std::cout << "MarketingBudget: NULL\n";
    }
  }
  std::cout << "Read completed for [spanner_read_data_with_storing_index]\n";
}
//! [END spanner_read_data_with_storing_index]

//! [START spanner_read_write_transaction]
void ReadWriteTransaction(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  using ::google::cloud::StatusOr;

  // A helper to read a single album MarketingBudget.
  auto get_current_budget =
      [](spanner::Client client, spanner::Transaction txn,
         std::int64_t singer_id,
         std::int64_t album_id) -> StatusOr<std::int64_t> {
    auto key = spanner::KeySet().AddKey(spanner::MakeKey(singer_id, album_id));
    auto rows = client.Read(std::move(txn), "Albums", std::move(key),
                            {"MarketingBudget"});
    using RowType = std::tuple<std::int64_t>;
    auto row = spanner::GetSingularRow(spanner::StreamOf<RowType>(rows));
    if (!row) return std::move(row).status();
    return std::get<0>(*std::move(row));
  };

  auto commit = client.Commit(
      [&client, &get_current_budget](
          spanner::Transaction const& txn) -> StatusOr<spanner::Mutations> {
        auto b1 = get_current_budget(client, txn, 1, 1);
        if (!b1) return std::move(b1).status();
        auto b2 = get_current_budget(client, txn, 2, 2);
        if (!b2) return std::move(b2).status();
        std::int64_t transfer_amount = 200000;

        return spanner::Mutations{
            spanner::UpdateMutationBuilder(
                "Albums", {"SingerId", "AlbumId", "MarketingBudget"})
                .EmplaceRow(1, 1, *b1 + transfer_amount)
                .EmplaceRow(2, 2, *b2 - transfer_amount)
                .Build()};
      });

  if (!commit) throw std::runtime_error(commit.status().message());
  std::cout << "Transfer was successful [spanner_read_write_transaction]\n";
}
//! [END spanner_read_write_transaction]

//! [START spanner_dml_standard_insert]
void DmlStandardInsert(google::cloud::spanner::Client client) {
  //! [execute-dml]
  using ::google::cloud::StatusOr;
  namespace spanner = ::google::cloud::spanner;
  std::int64_t rows_inserted;
  auto commit_result = client.Commit(
      [&client, &rows_inserted](
          spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto insert = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "INSERT INTO Singers (SingerId, FirstName, LastName)"
                "  VALUES (10, 'Virginia', 'Watson')"));
        if (!insert) return insert.status();
        rows_inserted = insert->RowsModified();
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Rows inserted: " << rows_inserted;
  //! [execute-dml]
  std::cout << "Insert was successful [spanner_dml_standard_insert]\n";
}
//! [END spanner_dml_standard_insert]

//! [START spanner_dml_standard_update]
void DmlStandardUpdate(google::cloud::spanner::Client client) {
  //! [commit-with-mutator]
  using ::google::cloud::StatusOr;
  namespace spanner = ::google::cloud::spanner;
  auto commit_result = client.Commit(
      [&client](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto update = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "UPDATE Albums SET MarketingBudget = MarketingBudget * 2"
                " WHERE SingerId = 1 AND AlbumId = 1"));
        if (!update) return update.status();
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  //! [commit-with-mutator]
  std::cout << "Update was successful [spanner_dml_standard_update]\n";
}
//! [END spanner_dml_standard_update]

//! [commit-with-policies]
void CommitWithPolicies(google::cloud::spanner::Client client) {
  using ::google::cloud::StatusOr;
  namespace spanner = ::google::cloud::spanner;
  auto commit = client.Commit(
      [&client](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto update = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "UPDATE Albums SET MarketingBudget = MarketingBudget * 2"
                " WHERE SingerId = 1 AND AlbumId = 1"));
        if (!update) return update.status();
        return spanner::Mutations{};
      },
      // Retry for up to 42 minutes.
      spanner::LimitedTimeTransactionRerunPolicy(std::chrono::minutes(42))
          .clone(),
      // After a failure backoff for 2 seconds (with jitter), then triple the
      // backoff time on each retry, up to 5 minutes.
      spanner::ExponentialBackoffPolicy(std::chrono::seconds(2),
                                        std::chrono::minutes(5), 3.0)
          .clone());
  if (!commit) throw std::runtime_error(commit.status().message());
  std::cout << "commit-with-policies was successful\n";
}
//! [commit-with-policies]

void ProfileDmlStandardUpdate(google::cloud::spanner::Client client) {
  using ::google::cloud::StatusOr;
  namespace spanner = ::google::cloud::spanner;
  //! [profile-dml]
  spanner::ProfileDmlResult dml_result;
  auto commit_result = client.Commit(
      [&client,
       &dml_result](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto update = client.ProfileDml(
            std::move(txn),
            spanner::SqlStatement(
                "UPDATE Albums SET MarketingBudget = MarketingBudget * 2"
                " WHERE SingerId = 1 AND AlbumId = 1"));
        if (!update) return update.status();
        dml_result = *std::move(update);
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }

  // Stats only available after statement has been executed.
  std::cout << "Rows modified: " << dml_result.RowsModified();
  auto execution_stats = dml_result.ExecutionStats();
  if (execution_stats) {
    for (auto const& stat : *execution_stats) {
      std::cout << stat.first << ":\t" << stat.second << "\n";
    }
  }
  //! [profile-dml]
}

// [START spanner_dml_standard_update_with_timestamp]
void DmlStandardUpdateWithTimestamp(google::cloud::spanner::Client client) {
  using ::google::cloud::StatusOr;
  namespace spanner = ::google::cloud::spanner;
  auto commit_result = client.Commit(
      [&client](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto update = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "UPDATE Albums SET LastUpdateTime = PENDING_COMMIT_TIMESTAMP() "
                "WHERE SingerId = 1"));
        if (!update) return update.status();
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Update was successful "
            << "[spanner_dml_standard_update_with_timestamp]\n";
}
// [END spanner_dml_standard_update_with_timestamp]

//! [START spanner_dml_write_then_read]
void DmlWriteThenRead(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  using ::google::cloud::StatusOr;

  auto commit_result = client.Commit(
      [&client](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto insert = client.ExecuteDml(
            txn, spanner::SqlStatement(
                     "INSERT INTO Singers (SingerId, FirstName, LastName)"
                     "  VALUES (11, 'Timothy', 'Campbell')"));
        if (!insert) return insert.status();
        // Read newly inserted record.
        spanner::SqlStatement select(
            "SELECT FirstName, LastName FROM Singers where SingerId = 11");
        using RowType = std::tuple<std::string, std::string>;
        auto rows = client.ExecuteQuery(std::move(txn), std::move(select));
        for (auto const& row : spanner::StreamOf<RowType>(rows)) {
          if (!row) return row.status();
          std::cout << "FirstName: " << std::get<0>(*row) << "\t";
          std::cout << "LastName: " << std::get<1>(*row) << "\n";
        }
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Write then read suceeded [spanner_dml_write_then_read]\n";
}
//! [END spanner_dml_write_then_read]

//! [START spanner_dml_standard_delete]
void DmlStandardDelete(google::cloud::spanner::Client client) {
  using ::google::cloud::StatusOr;
  namespace spanner = ::google::cloud::spanner;
  auto commit_result = client.Commit([&client](spanner::Transaction txn)
                                         -> StatusOr<spanner::Mutations> {
    auto dele = client.ExecuteDml(
        std::move(txn),
        spanner::SqlStatement("DELETE FROM Singers WHERE FirstName = 'Alice'"));
    if (!dele) return dele.status();
    return spanner::Mutations{};
  });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Delete was successful [spanner_dml_standard_delete]\n";
}
//! [END spanner_dml_standard_delete]

//! [execute-sql-partitioned] [START spanner_dml_partitioned_delete]
void DmlPartitionedDelete(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto result = client.ExecutePartitionedDml(
      spanner::SqlStatement("DELETE FROM Singers WHERE SingerId > 10"));
  if (!result) throw std::runtime_error(result.status().message());
  std::cout << "Delete was successful [spanner_dml_partitioned_delete]\n";
}
//! [execute-sql-partitioned] [END spanner_dml_partitioned_delete]

//! [START spanner_dml_partitioned_update]
void DmlPartitionedUpdate(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto result = client.ExecutePartitionedDml(
      spanner::SqlStatement("UPDATE Albums SET MarketingBudget = 100000"
                            " WHERE SingerId > 1"));
  if (!result) throw std::runtime_error(result.status().message());
  std::cout << "Update was successful [spanner_dml_partitioned_update]\n";
}
//! [END spanner_dml_partitioned_update]

//! [START spanner_dml_batch_update]
void DmlBatchUpdate(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  auto commit_result =
      client.Commit([&client](spanner::Transaction const& txn)
                        -> google::cloud::StatusOr<spanner::Mutations> {
        std::vector<spanner::SqlStatement> statements = {
            spanner::SqlStatement("INSERT INTO Albums"
                                  " (SingerId, AlbumId, AlbumTitle,"
                                  " MarketingBudget)"
                                  " VALUES (1, 3, 'Test Album Title', 10000)"),
            spanner::SqlStatement("UPDATE Albums"
                                  " SET MarketingBudget = MarketingBudget * 2"
                                  " WHERE SingerId = 1 and AlbumId = 3")};
        auto result = client.ExecuteBatchDml(txn, statements);
        if (!result) return result.status();
        for (std::size_t i = 0; i < result->stats.size(); ++i) {
          std::cout << result->stats[i].row_count << " rows affected"
                    << " for the statement " << (i + 1) << ".\n";
        }
        // Batch operations may have partial failures, in which case
        // ExecuteBatchDml returns with success, but the application should
        // verify that all statements completed successfully
        if (!result->status.ok()) return result->status;
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Update was successful [spanner_dml_batch_update]\n";
}
//! [END spanner_dml_batch_update]

//! [execute-batch-dml] [START spanner_dml_structs]
void DmlStructs(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  std::int64_t rows_modified = 0;
  auto commit_result =
      client.Commit([&client, &rows_modified](spanner::Transaction const& txn)
                        -> google::cloud::StatusOr<spanner::Mutations> {
        auto singer_info = std::make_tuple("Marc", "Richards");
        auto sql = spanner::SqlStatement(
            "UPDATE Singers SET FirstName = 'Keith' WHERE "
            "STRUCT<FirstName String, LastName String>(FirstName, LastName) "
            "= @name",
            {{"name", spanner::Value(std::move(singer_info))}});
        auto dml_result = client.ExecuteDml(txn, std::move(sql));
        if (!dml_result) return dml_result.status();
        rows_modified = dml_result->RowsModified();
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << rows_modified
            << " update was successful [spanner_dml_structs]\n";
}
//! [execute-batch-dml] [END spanner_dml_structs]

//! [START spanner_write_data_for_struct_queries]
void WriteDataForStructQueries(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto commit_result = client.Commit(
      spanner::Mutations{spanner::InsertMutationBuilder(
                             "Singers", {"SingerId", "FirstName", "LastName"})
                             .EmplaceRow(6, "Elena", "Campbell")
                             .EmplaceRow(7, "Gabriel", "Wright")
                             .EmplaceRow(8, "Benjamin", "Martinez")
                             .EmplaceRow(9, "Hannah", "Harris")
                             .Build()});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful "
            << "[spanner_write_data_for_struct_queries]\n";
}
//! [END spanner_write_data_for_struct_queries]

//! [START spanner_query_data] [spanner-query-data]
void QueryData(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  spanner::SqlStatement select("SELECT SingerId, LastName FROM Singers");
  using RowType = std::tuple<std::int64_t, std::string>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\t";
    std::cout << "LastName: " << std::get<1>(*row) << "\n";
  }

  std::cout << "Query completed for [spanner_query_data]\n";
}
//! [END spanner_query_data] [spanner-query-data]

//! [START spanner_dml_getting_started_insert]
void DmlGettingStartedInsert(google::cloud::spanner::Client client) {
  using ::google::cloud::StatusOr;
  namespace spanner = ::google::cloud::spanner;

  auto commit_result = client.Commit(
      [&client](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto insert = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "INSERT INTO Singers (SingerId, FirstName, LastName) VALUES"
                " (12, 'Melissa', 'Garcia'),"
                " (13, 'Russell', 'Morales'),"
                " (14, 'Jacqueline', 'Long'),"
                " (15, 'Dylan', 'Shaw')"));
        if (!insert) return insert.status();
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful [spanner_dml_getting_started_insert]\n";
}
//! [END spanner_dml_getting_started_insert]

//! [START spanner_dml_getting_started_update]
void DmlGettingStartedUpdate(google::cloud::spanner::Client client) {
  using ::google::cloud::StatusOr;
  namespace spanner = ::google::cloud::spanner;

  // A helper to read the budget for the given album and singer.
  auto get_budget = [&](spanner::Transaction txn, std::int64_t album_id,
                        std::int64_t singer_id) -> StatusOr<std::int64_t> {
    auto key = spanner::KeySet().AddKey(spanner::MakeKey(album_id, singer_id));
    auto rows = client.Read(std::move(txn), "Albums", key, {"MarketingBudget"});
    using RowType = std::tuple<absl::optional<std::int64_t>>;
    auto row = spanner::GetSingularRow(spanner::StreamOf<RowType>(rows));
    if (!row) return row.status();
    auto const budget = std::get<0>(*row);
    return budget ? *budget : 0;
  };

  // A helper to update the budget for the given album and singer.
  auto update_budget = [&](spanner::Transaction txn, std::int64_t album_id,
                           std::int64_t singer_id, std::int64_t budget) {
    auto sql = spanner::SqlStatement(
        "UPDATE Albums SET MarketingBudget = @AlbumBudget "
        "WHERE SingerId = @SingerId AND AlbumId = @AlbumId",
        {{"AlbumBudget", spanner::Value(budget)},
         {"AlbumId", spanner::Value(album_id)},
         {"SingerId", spanner::Value(singer_id)}});
    return client.ExecuteDml(std::move(txn), std::move(sql));
  };

  auto const transfer_amount = 20000;
  auto commit_result = client.Commit(
      [&](spanner::Transaction const& txn) -> StatusOr<spanner::Mutations> {
        auto budget1 = get_budget(txn, 1, 1);
        if (!budget1) return budget1.status();
        if (*budget1 < transfer_amount) {
          return google::cloud::Status(
              google::cloud::StatusCode::kUnknown,
              "cannot transfer " + std::to_string(transfer_amount) +
                  " from budget of " + std::to_string(*budget1));
        }
        auto budget2 = get_budget(txn, 2, 2);
        if (!budget2) return budget2.status();
        auto update = update_budget(txn, 1, 1, *budget1 - transfer_amount);
        if (!update) return update.status();
        update = update_budget(txn, 2, 2, *budget2 + transfer_amount);
        if (!update) return update.status();
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Update was successful [spanner_dml_getting_started_update]\n";
}
//! [END spanner_dml_getting_started_update]

//! [START spanner_query_with_parameter]
void QueryWithParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  spanner::SqlStatement select(
      "SELECT SingerId, FirstName, LastName FROM Singers"
      " WHERE LastName = @last_name",
      {{"last_name", spanner::Value("Garcia")}});
  using RowType = std::tuple<std::int64_t, std::string, std::string>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\t";
    std::cout << "FirstName: " << std::get<1>(*row) << "\t";
    std::cout << "LastName: " << std::get<2>(*row) << "\n";
  }

  std::cout << "Query completed for [spanner_query_with_parameter]\n";
}
//! [END spanner_query_with_parameter]

//! [read-data] [START spanner_read_data]
void ReadData(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  auto rows = client.Read("Albums", google::cloud::spanner::KeySet::All(),
                          {"SingerId", "AlbumId", "AlbumTitle"});
  using RowType = std::tuple<std::int64_t, std::int64_t, std::string>;
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\t";
    std::cout << "AlbumId: " << std::get<1>(*row) << "\t";
    std::cout << "AlbumTitle: " << std::get<2>(*row) << "\n";
  }

  std::cout << "Read completed for [spanner_read_data]\n";
}
//! [read-data] [END spanner_read_data]

//! [spanner-query-data-select-star]
void QueryDataSelectStar(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  // With a "SELECT *" query, we don't know the order in which the columns will
  // be returned (nor the number of columns). Therefore, we look up each value
  // based on the column name rather than its position.
  spanner::SqlStatement select_star("SELECT * FROM Singers");
  auto rows = client.ExecuteQuery(std::move(select_star));
  for (auto const& row : rows) {
    if (!row) throw std::runtime_error(row.status().message());

    if (auto singer_id = row->get<std::int64_t>("SingerId")) {
      std::cout << "SingerId: " << *singer_id << "\t";
    } else {
      std::cerr << singer_id.status();
    }

    if (auto last_name = row->get<std::string>("LastName")) {
      std::cout << "LastName: " << *last_name;
    } else {
      std::cerr << last_name.status();
    }
    std::cout << "\n";
  }
  std::cout << "Query completed for [spanner_query_data_select_star]\n";
}
//! [spanner-query-data-select-star]

//! [START spanner_query_data_with_struct]
void QueryDataWithStruct(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  //! [spanner-sql-statement-params] [START spanner_create_struct_with_data]
  //! [START spanner_create_user_defined_struct]
  // Cloud Spanner STRUCT<> types are represented by std::tuple<...>. The
  // following represents a STRUCT<> with two unnamed STRING fields.
  using NameType = std::tuple<std::string, std::string>;
  //! [END spanner_create_user_defined_struct]
  auto singer_info = NameType{"Elena", "Campbell"};
  //! [END spanner_create_struct_with_data]
  auto rows = client.ExecuteQuery(spanner::SqlStatement(
      "SELECT SingerId FROM Singers WHERE (FirstName, LastName) = @name",
      {{"name", spanner::Value(singer_info)}}));
  //! [spanner-sql-statement-params]

  for (auto const& row : spanner::StreamOf<std::tuple<std::int64_t>>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\n";
  }
  std::cout << "Query completed for [spanner_query_data_with_struct]\n";
}
//! [END spanner_query_data_with_struct]

//! [START spanner_query_data_with_array_of_struct]
void QueryDataWithArrayOfStruct(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  // [START spanner_create_array_of_struct_with_data]
  // Cloud Spanner STRUCT<> types with named fields are represented by
  // std::tuple<std::pair<std::string, T>...>, create an alias to make it easier
  // to follow this code.
  using SingerName = std::tuple<std::pair<std::string, std::string>,
                                std::pair<std::string, std::string>>;
  auto make_name = [](std::string first_name, std::string last_name) {
    return std::make_tuple(std::make_pair("FirstName", std::move(first_name)),
                           std::make_pair("LastName", std::move(last_name)));
  };
  std::vector<SingerName> singer_info{
      make_name("Elena", "Campbell"),
      make_name("Gabriel", "Wright"),
      make_name("Benjamin", "Martinez"),
  };
  // [END spanner_create_array_of_struct_with_data]

  auto rows = client.ExecuteQuery(spanner::SqlStatement(
      "SELECT SingerId FROM Singers"
      " WHERE STRUCT<FirstName STRING, LastName STRING>(FirstName, LastName)"
      "    IN UNNEST(@names)",
      {{"names", spanner::Value(singer_info)}}));

  for (auto const& row : spanner::StreamOf<std::tuple<std::int64_t>>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\n";
  }
  std::cout << "Query completed for"
            << " [spanner_query_data_with_array_of_struct]\n";
}
//! [END spanner_query_data_with_array_of_struct]

//! [START spanner_field_access_on_struct_parameters]
void FieldAccessOnStructParameters(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  // Cloud Spanner STRUCT<> with named fields is represented as
  // tuple<pair<string, T>...>. Create a type alias for this example:
  using SingerName = std::tuple<std::pair<std::string, std::string>,
                                std::pair<std::string, std::string>>;
  SingerName name({"FirstName", "Elena"}, {"LastName", "Campbell"});

  auto rows = client.ExecuteQuery(spanner::SqlStatement(
      "SELECT SingerId FROM Singers WHERE FirstName = @name.FirstName",
      {{"name", spanner::Value(name)}}));

  for (auto const& row : spanner::StreamOf<std::tuple<std::int64_t>>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\n";
  }
  std::cout << "Query completed for"
            << " [spanner_field_access_on_struct_parameters]\n";
}
//! [END spanner_field_access_on_struct_parameters]

//! [START spanner_field_access_on_nested_struct_parameters]
void FieldAccessOnNestedStruct(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  // Cloud Spanner STRUCT<> with named fields is represented as
  // tuple<pair<string, T>...>. Create a type alias for this example:
  using SingerFullName = std::tuple<std::pair<std::string, std::string>,
                                    std::pair<std::string, std::string>>;
  auto make_name = [](std::string fname, std::string lname) {
    return SingerFullName({"FirstName", std::move(fname)},
                          {"LastName", std::move(lname)});
  };
  using SongInfo =
      std::tuple<std::pair<std::string, std::string>,
                 std::pair<std::string, std::vector<SingerFullName>>>;
  auto songinfo = SongInfo(
      {"SongName", "Imagination"},
      {"ArtistNames",
       {make_name("Elena", "Campbell"), make_name("Hannah", "Harris")}});

  auto rows = client.ExecuteQuery(spanner::SqlStatement(
      "SELECT SingerId, @songinfo.SongName FROM Singers"
      " WHERE STRUCT<FirstName STRING, LastName STRING>(FirstName, LastName)"
      "    IN UNNEST(@songinfo.ArtistNames)",
      {{"songinfo", spanner::Value(songinfo)}}));

  using RowType = std::tuple<std::int64_t, std::string>;
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row)
              << " SongName: " << std::get<1>(*row) << "\n";
  }
  std::cout << "Query completed for [spanner_field_access_on_nested_struct]\n";
}
//! [END spanner_field_access_on_nested_struct_parameters]

void ExampleStatusOr(google::cloud::spanner::Client client) {
  //! [example-status-or]
  namespace spanner = ::google::cloud::spanner;
  [](spanner::Client client) {
    auto rows = client.Read("Albums", spanner::KeySet::All(), {"AlbumTitle"});
    // The actual type of `row` is google::cloud::StatusOr<spanner::Row>, but
    // we expect it'll most often be declared with auto like this.
    for (auto const& row : rows) {
      // Use `row` like a smart pointer; check it before dereferencing
      if (!row) {
        // `row` doesn't contain a value, so `.status()` will contain error info
        std::cerr << row.status();
        break;
      }

      // The actual type of `song` is google::cloud::StatusOr<std::string>, but
      // again we expect it'll be commonly declared with auto as we show here.
      auto song = row->get<std::string>("AlbumTitle");

      // Instead of checking then dereferencing `song` as we did with `row`
      // above, here we demonstrate use of the `.value()` member, which will
      // return a reference to the contained `T` if it exists, otherwise it
      // will throw an exception (or terminate if compiled without exceptions).
      std::cout << "SongName: " << song.value() << "\n";
    }
  }
  //! [example-status-or]
  (std::move(client));
}

void CustomRetryPolicy(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "custom-retry-policy <project-id> <instance-id> <database-id>");
  }
  //! [custom-retry-policy]
  namespace spanner = ::google::cloud::spanner;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& database_id) {
    auto client = spanner::Client(spanner::MakeConnection(
        spanner::Database(project_id, instance_id, database_id),
        spanner::ConnectionOptions{}, spanner::SessionPoolOptions{},
        // Retry for at most 25 minutes.
        spanner::LimitedTimeRetryPolicy(
            /*maximum_duration=*/std::chrono::minutes(25))
            .clone(),
        // Use an truncated exponential backoff with jitter to wait between
        // retries:
        //   https://en.wikipedia.org/wiki/Exponential_backoff
        //   https://cloud.google.com/storage/docs/exponential-backoff
        spanner::ExponentialBackoffPolicy(
            /*initial_delay=*/std::chrono::seconds(2),
            /*maximum_delay=*/std::chrono::minutes(10),
            /*scaling=*/1.5)
            .clone()));

    auto rows =
        client.ExecuteQuery(spanner::SqlStatement("SELECT 'Hello World'"));

    for (auto const& row : spanner::StreamOf<std::tuple<std::string>>(rows)) {
      if (!row) throw std::runtime_error(row.status().message());
      std::cout << std::get<0>(*row) << "\n";
    }
  }
  //! [custom-retry-policy]
  (argv[0], argv[1], argv[2]);
}

void CustomInstanceAdminPolicies(std::vector<std::string> argv) {
  if (argv.size() != 1) {
    throw std::runtime_error("custom-instance-admin-policies <project-id>");
  }
  //! [custom-instance-admin-policies]
  namespace spanner = ::google::cloud::spanner;
  [](std::string const& project_id) {
    // An instance admin client is controlled by three policies. The retry
    // policy determines for how long the client will retry transient failures.
    auto retry_policy = spanner::LimitedTimeRetryPolicy(
                            /*maximum_duration=*/std::chrono::minutes(25))
                            .clone();
    // The backoff policy controls how long does the client waits to retry after
    // a transient failure. Here we configure a truncated exponential backoff
    // with jitter:
    //   https://en.wikipedia.org/wiki/Exponential_backoff
    //   https://cloud.google.com/storage/docs/exponential-backoff
    auto backoff_policy = spanner::ExponentialBackoffPolicy(
                              /*initial_delay=*/std::chrono::seconds(2),
                              /*maximum_delay=*/std::chrono::minutes(10),
                              /*scaling=*/2.0)
                              .clone();
    // The polling policy controls how the client waits for long-running
    // operations (such as creating new instances). `GenericPollingPolicy<>`
    // combines existing policies.  In this case, keep polling until the
    // operation completes (with success or error) or 45 minutes, whichever
    // happens first. Initially pause for 10 seconds between polling requests,
    // increasing the pause by a factor of 4 until it becomes 2 minutes.
    auto polling_policy =
        spanner::GenericPollingPolicy<>(
            spanner::LimitedTimeRetryPolicy(
                /*maximum_duration=*/std::chrono::minutes(45)),
            spanner::ExponentialBackoffPolicy(
                /*initial_delay=*/std::chrono::seconds(10),
                /*maximum_delay=*/std::chrono::minutes(2),
                /*scaling=*/4.0))
            .clone();
    auto client =
        spanner::InstanceAdminClient(spanner::MakeInstanceAdminConnection(
            spanner::ConnectionOptions{}, std::move(retry_policy),
            std::move(backoff_policy), std::move(polling_policy)));

    // Use the client as usual.
    std::cout << "Available configs for project " << project_id << "\n";
    for (auto const& cfg : client.ListInstanceConfigs(project_id)) {
      if (!cfg) throw std::runtime_error(cfg.status().message());
      std::cout << cfg->name() << "\n";
    }
    std::cout << "End of available configs\n";
  }
  //! [custom-instance-admin-policies]
  (argv[0]);
}

void CustomDatabaseAdminPolicies(std::vector<std::string> argv) {
  if (argv.size() != 2) {
    throw std::runtime_error(
        "custom-database-admin-policies <project-id> <instance-id>");
  }
  //! [custom-database-admin-policies]
  namespace spanner = ::google::cloud::spanner;
  [](std::string const& project_id, std::string const& instance_id) {
    // A database admin client is controlled by three policies. The retry
    // policy determines for how long the client will retry transient failures.
    auto retry_policy = spanner::LimitedTimeRetryPolicy(
                            /*maximum_duration=*/std::chrono::minutes(25))
                            .clone();
    // The backoff policy controls how long does the client waits to retry after
    // a transient failure. Here we configure a truncated exponential backoff
    // with jitter:
    //   https://en.wikipedia.org/wiki/Exponential_backoff
    //   https://cloud.google.com/storage/docs/exponential-backoff
    auto backoff_policy = spanner::ExponentialBackoffPolicy(
                              /*initial_delay=*/std::chrono::seconds(2),
                              /*maximum_delay=*/std::chrono::minutes(10),
                              /*scaling=*/2.0)
                              .clone();
    // The polling policy controls how the client waits for long-running
    // operations (such as creating new instances). `GenericPollingPolicy<>`
    // combines existing policies.  In this case, keep polling until the
    // operation completes (with success or error) or 45 minutes, whichever
    // happens first. Initially pause for 10 seconds between polling requests,
    // increasing the pause by a factor of 4 until it becomes 2 minutes.
    auto polling_policy =
        spanner::GenericPollingPolicy<>(
            spanner::LimitedTimeRetryPolicy(
                /*maximum_duration=*/std::chrono::minutes(45)),
            spanner::ExponentialBackoffPolicy(
                /*initial_delay=*/std::chrono::seconds(10),
                /*maximum_delay=*/std::chrono::minutes(2),
                /*scaling=*/4.0))
            .clone();
    auto client =
        spanner::DatabaseAdminClient(spanner::MakeDatabaseAdminConnection(
            spanner::ConnectionOptions{}, std::move(retry_policy),
            std::move(backoff_policy), std::move(polling_policy)));

    // Use the client as usual.
    spanner::Instance instance(project_id, instance_id);
    std::cout << "Available databases for instance " << instance << "\n";
    for (auto const& db : client.ListDatabases(instance)) {
      if (!db) throw std::runtime_error(db.status().message());
      std::cout << db->name() << "\n";
    }
    std::cout << "End of available databases\n";
  }
  //! [custom-database-admin-policies]
  (argv[0], argv[1]);
}

//! [get-singular-row]
void GetSingularRow(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  auto query = client.ExecuteQuery(spanner::SqlStatement(
      "SELECT FirstName, LastName FROM Singers WHERE SingerId = @singer_id",
      {{"singer_id", spanner::Value(2)}}));
  // `SingerId` is the primary key for the `Singers` table, the `GetSingularRow`
  // helper returns a single row or an error:
  using RowType = std::tuple<std::string, std::string>;
  auto row = GetSingularRow(spanner::StreamOf<RowType>(query));
  if (!row) throw std::runtime_error(row.status().message());
  std::cout << "FirstName: " << std::get<0>(*row)
            << "\nLastName: " << std::get<1>(*row) << "\n";
}
//! [get-singular-row]

//! [stream-of]
void StreamOf(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  std::cout << "Querying the Singers table:\n";
  auto query = client.ExecuteQuery(spanner::SqlStatement(
      "SELECT SingerId, FirstName, LastName FROM Singers"));
  using RowType = std::tuple<std::int64_t, std::string, std::string>;

  for (auto& row : spanner::StreamOf<RowType>(query)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "  FirstName: " << std::get<0>(*row)
              << "\n  LastName: " << std::get<1>(*row) << "\n";
  }
  std::cout << "end of results\n";
}
//! [stream-of]

class RemoteConnectionFake {
 public:
  void SendBinaryStringData(std::string const& serialized_partition) {
    serialized_partition_in_transit_ = serialized_partition;
  }
  std::string Receive() { return serialized_partition_in_transit_; }
  void SendPartitionToRemoteMachine(
      google::cloud::spanner::ReadPartition const& partition) {
    //! [serialize-read-partition]
    namespace spanner = ::google::cloud::spanner;
    google::cloud::StatusOr<std::string> serialized_partition =
        spanner::SerializeReadPartition(partition);
    if (!serialized_partition) {
      throw std::runtime_error(serialized_partition.status().message());
    }
    std::string const& bytes = *serialized_partition;
    // `bytes` contains the serialized data, which may contain NULs and other
    // non-printable characters.
    SendBinaryStringData(bytes);
    //! [serialize-read-partition]
  }

  void SendPartitionToRemoteMachine(
      google::cloud::spanner::QueryPartition const& partition) {
    //! [serialize-query-partition]
    namespace spanner = ::google::cloud::spanner;
    google::cloud::StatusOr<std::string> serialized_partition =
        spanner::SerializeQueryPartition(partition);
    if (!serialized_partition) {
      throw std::runtime_error(serialized_partition.status().message());
    }
    std::string const& bytes = *serialized_partition;
    // `bytes` contains the serialized data, which may contain NULs and other
    // non-printable characters.
    SendBinaryStringData(bytes);
    //! [serialize-query-partition]
  }
  //! [deserialize-read-partition]
  google::cloud::StatusOr<google::cloud::spanner::ReadPartition>
  ReceiveReadPartitionFromRemoteMachine() {
    std::string serialized_partition = Receive();
    return google::cloud::spanner::DeserializeReadPartition(
        serialized_partition);
  }
  //! [deserialize-read-partition]

  //! [deserialize-query-partition]
  google::cloud::StatusOr<google::cloud::spanner::QueryPartition>
  ReceiveQueryPartitionFromRemoteMachine() {
    std::string serialized_partition = Receive();
    return google::cloud::spanner::DeserializeQueryPartition(
        serialized_partition);
  }
  //! [deserialize-query-partition]

 private:
  std::string serialized_partition_in_transit_;
};

void ProcessRow(google::cloud::spanner::Row const&) {}

void PartitionRead(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  RemoteConnectionFake remote_connection;
  //! [key-set-builder] [make-keybound-open]
  auto key_set = spanner::KeySet().AddRange(spanner::MakeKeyBoundOpen(0),
                                            spanner::MakeKeyBoundOpen(11));
  //! [key-set-builder] [make-keybound-open]

  //! [partition-read]
  spanner::Transaction ro_transaction = spanner::MakeReadOnlyTransaction();
  google::cloud::StatusOr<std::vector<spanner::ReadPartition>> partitions =
      client.PartitionRead(ro_transaction, "Singers", key_set,
                           {"SingerId", "FirstName", "LastName"});
  if (!partitions) throw std::runtime_error(partitions.status().message());
  for (auto& partition : *partitions) {
    remote_connection.SendPartitionToRemoteMachine(partition);
  }
  //! [partition-read]

  //! [read-read-partition]
  google::cloud::StatusOr<spanner::ReadPartition> partition =
      remote_connection.ReceiveReadPartitionFromRemoteMachine();
  if (!partition) throw std::runtime_error(partition.status().message());
  auto rows = client.Read(*partition);
  for (auto const& row : rows) {
    if (!row) throw std::runtime_error(row.status().message());
    ProcessRow(*row);
  }
  //! [read-read-partition]
}

void PartitionQuery(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  RemoteConnectionFake remote_connection;

  //! [analyze-query]
  // Only SQL queries with a Distributed Union as the first operator in the
  // `ExecutionPlan` can be partitioned.
  auto is_partitionable = [](spanner::ExecutionPlan const& plan) {
    return (!plan.plan_nodes().empty() &&
            plan.plan_nodes(0).kind() ==
                google::spanner::v1::PlanNode::RELATIONAL &&
            plan.plan_nodes(0).display_name() == "Distributed Union");
  };

  google::cloud::StatusOr<spanner::ExecutionPlan> plan = client.AnalyzeSql(
      spanner::MakeReadOnlyTransaction(),
      spanner::SqlStatement(
          "SELECT SingerId, FirstName, LastName FROM Singers"));
  if (!plan) throw std::runtime_error(plan.status().message());
  if (!is_partitionable(*plan)) {
    throw std::runtime_error("Query is not partitionable");
  }
  //! [analyze-query]

  //! [partition-query]
  google::cloud::StatusOr<std::vector<spanner::QueryPartition>> partitions =
      client.PartitionQuery(
          spanner::MakeReadOnlyTransaction(),
          spanner::SqlStatement(
              "SELECT SingerId, FirstName, LastName FROM Singers"));
  if (!partitions) throw std::runtime_error(partitions.status().message());
  for (auto& partition : *partitions) {
    remote_connection.SendPartitionToRemoteMachine(partition);
  }
  //! [partition-query]

  //! [execute-sql-query-partition]
  google::cloud::StatusOr<spanner::QueryPartition> partition =
      remote_connection.ReceiveQueryPartitionFromRemoteMachine();
  if (!partition) throw std::runtime_error(partition.status().message());
  auto rows = client.ExecuteQuery(*partition);
  for (auto const& row : rows) {
    if (!row) throw std::runtime_error(row.status().message());
    ProcessRow(*row);
  }
  //! [execute-sql-query-partition]
}

int RunOneCommand(std::vector<std::string> argv) {
  using CommandType = std::function<void(std::vector<std::string> const&)>;

  using SampleFunction = void (*)(google::cloud::spanner::Client);

  using CommandMap = std::map<std::string, CommandType>;
  auto make_command_entry = [](std::string const& sample_name,
                               SampleFunction sample) {
    auto make_command = [](std::string const& sample_name,
                           SampleFunction sample) {
      return [sample_name, sample](std::vector<std::string> const& argv) {
        if (argv.size() != 3) {
          throw std::runtime_error(sample_name +
                                   " <project-id> <instance-id> <database-id>");
        }
        sample(MakeSampleClient(argv[0], argv[1], argv[2]));
      };
    };
    return CommandMap::value_type(sample_name,
                                  make_command(sample_name, sample));
  };

  using DatabaseAdminSampleFunction =
      void (*)(google::cloud::spanner::DatabaseAdminClient, std::string const&,
               std::string const&, std::string const&);
  auto make_database_command_entry = [](std::string const& sample_name,
                                        DatabaseAdminSampleFunction sample) {
    auto make_command = [](std::string const& sample_name,
                           DatabaseAdminSampleFunction sample) {
      return [sample_name, sample](std::vector<std::string> const& argv) {
        if (argv.size() != 3) {
          throw std::runtime_error(sample_name +
                                   " <project-id> <instance-id> <database-id>");
        }
        google::cloud::spanner::DatabaseAdminClient client;
        sample(client, argv[0], argv[1], argv[2]);
      };
    };
    return CommandMap::value_type(sample_name,
                                  make_command(sample_name, sample));
  };

  CommandMap commands = {
      {"get-instance", GetInstanceCommand},
      {"create-instance", CreateInstanceCommand},
      {"update-instance", UpdateInstanceCommand},
      {"delete-instance", DeleteInstanceCommand},
      {"list-instance-configs", ListInstanceConfigsCommand},
      {"get-instance-config", GetInstanceConfigCommand},
      {"list-instances", ListInstancesCommand},
      {"instance-get-iam-policy", InstanceGetIamPolicyCommand},
      {"add-database-reader", AddDatabaseReaderCommand},
      {"remove-database-reader", RemoveDatabaseReaderCommand},
      {"instance-test-iam-permissions", InstanceTestIamPermissionsCommand},
      make_database_command_entry("create-database", CreateDatabase),
      make_database_command_entry("create-table-with-datatypes",
                                  CreateTableWithDatatypes),
      make_database_command_entry("create-table-with-timestamp",
                                  CreateTableWithTimestamp),
      make_database_command_entry("add-index", AddIndex),
      make_database_command_entry("add-storing-index", AddStoringIndex),
      make_database_command_entry("get-database", GetDatabase),
      make_database_command_entry("get-database-ddl", GetDatabaseDdl),
      make_database_command_entry("add-column", AddColumn),
      make_database_command_entry("add-timestamp-column", AddTimestampColumn),
      {"list-databases", ListDatabasesCommand},
      {"create-backup", CreateBackupCommand},
      {"restore-database", RestoreDatabaseCommand},
      {"get-backup", GetBackupCommand},
      {"update-backup", UpdateBackupCommand},
      {"delete-backup", DeleteBackupCommand},
      {"create-backup-and-cancel", CreateBackupAndCancelCommand},
      {"list-backups", ListBackupsCommand},
      make_database_command_entry("list-backup-operations",
                                  ListBackupOperations),
      {"list-database-operations", ListDatabaseOperationsCommand},
      make_database_command_entry("drop-database", DropDatabase),
      make_database_command_entry("database-get-iam-policy",
                                  DatabaseGetIamPolicy),
      {"add-database-reader-on-database", AddDatabaseReaderOnDatabaseCommand},
      {"database-test-iam-permissions", DatabaseTestIamPermissionsCommand},
      {"quickstart", QuickstartCommand},
      {"create-client-with-query-options", CreateClientWithQueryOptionsCommand},
      make_command_entry("insert-data", InsertData),
      make_command_entry("update-data", UpdateData),
      make_command_entry("delete-data", DeleteData),
      make_command_entry("insert-datatypes-data", InsertDatatypesData),
      make_command_entry("query-with-array-parameter", QueryWithArrayParameter),
      make_command_entry("query-with-bool-parameter", QueryWithBoolParameter),
      make_command_entry("query-with-bytes-parameter", QueryWithBytesParameter),
      make_command_entry("query-with-date-parameter", QueryWithDateParameter),
      make_command_entry("query-with-float-parameter", QueryWithFloatParameter),
      make_command_entry("query-with-int-parameter", QueryWithIntParameter),
      make_command_entry("query-with-string-parameter",
                         QueryWithStringParameter),
      make_command_entry("query-with-timestamp-parameter",
                         QueryWithTimestampParameter),
      make_command_entry("insert-data-with-timestamp", InsertDataWithTimestamp),
      make_command_entry("update-data-with-timestamp", UpdateDataWithTimestamp),
      make_command_entry("query-data-with-timestamp", QueryDataWithTimestamp),
      make_database_command_entry("add-numeric-column", AddNumericColumn),
      make_command_entry("update-data-with-numeric", UpdateDataWithNumeric),
      make_command_entry("query-with-numeric-parameter",
                         QueryWithNumericParameter),
      make_command_entry("read-only-transaction", ReadOnlyTransaction),
      make_command_entry("read-stale-data", ReadStaleData),
      make_command_entry("use-partition-query", UsePartitionQuery),
      make_command_entry("read-data-with-index", ReadDataWithIndex),
      make_command_entry("query-new-column", QueryNewColumn),
      make_command_entry("query-data-with-index", QueryUsingIndex),
      make_command_entry("query-with-query-options", QueryWithQueryOptions),
      make_command_entry("read-data-with-storing-index",
                         ReadDataWithStoringIndex),
      make_command_entry("read-write-transaction", ReadWriteTransaction),
      make_command_entry("dml-standard-insert", DmlStandardInsert),
      make_command_entry("dml-standard-update", DmlStandardUpdate),
      make_command_entry("dml-standard-update-with-timestamp",
                         DmlStandardUpdateWithTimestamp),
      make_command_entry("profile-dml-standard-update",
                         ProfileDmlStandardUpdate),
      make_command_entry("commit-with-policies", CommitWithPolicies),
      make_command_entry("dml-write-then-read", DmlWriteThenRead),
      make_command_entry("dml-standard-delete", DmlStandardDelete),
      make_command_entry("dml-partitioned-update", DmlPartitionedUpdate),
      make_command_entry("dml-batch-update", DmlBatchUpdate),
      make_command_entry("dml-partitioned-delete", DmlPartitionedDelete),
      make_command_entry("dml-structs", DmlStructs),
      make_command_entry("write-data-for-struct-queries",
                         WriteDataForStructQueries),
      make_command_entry("query-data", QueryData),
      make_command_entry("getting-started-insert", DmlGettingStartedInsert),
      make_command_entry("getting-started-update", DmlGettingStartedUpdate),
      make_command_entry("query-with-parameter", QueryWithParameter),
      make_command_entry("read-data", ReadData),
      make_command_entry("query-data-select-star", QueryDataSelectStar),
      make_command_entry("query-data-with-struct", QueryDataWithStruct),
      make_command_entry("query-data-with-array-of-struct",
                         QueryDataWithArrayOfStruct),
      make_command_entry("field-access-struct-parameters",
                         FieldAccessOnStructParameters),
      make_command_entry("field-access-on-nested-struct",
                         FieldAccessOnNestedStruct),
      make_command_entry("partition-read", PartitionRead),
      make_command_entry("partition-query", PartitionQuery),
      make_command_entry("example-status-or", ExampleStatusOr),
      make_command_entry("get-singular-row", GetSingularRow),
      make_command_entry("stream-of", StreamOf),
      make_command_entry("profile-query", ProfileQuery),
      {"custom-retry-policy", CustomRetryPolicy},
      {"custom-instance-admin-policies", CustomInstanceAdminPolicies},
      {"custom-database-admin-policies", CustomDatabaseAdminPolicies},
      make_command_entry("delete-all", DeleteAll),
      make_command_entry("insert-mutation-builder", InsertMutationBuilder),
      make_command_entry("make-insert-mutation", MakeInsertMutation),
      make_command_entry("update-mutation-builder", UpdateMutationBuilder),
      make_command_entry("make-update-mutation", MakeUpdateMutation),
      make_command_entry("insert-or-update-mutation-builder",
                         InsertOrUpdateMutationBuilder),
      make_command_entry("make-insert-or-update-mutation",
                         MakeInsertOrUpdateMutation),
      make_command_entry("replace-mutation-builder", ReplaceMutationBuilder),
      make_command_entry("make-replace-mutation", MakeReplaceMutation),
      make_command_entry("delete-mutation-builder", DeleteMutationBuilder),
      make_command_entry("make-delete-mutation", MakeDeleteMutation),
  };

  static std::string usage_msg = [&argv, &commands] {
    auto last_slash = std::string(argv[0]).find_last_of("/\\");
    auto program = argv[0].substr(last_slash + 1);
    std::string usage;
    usage += "Usage: ";
    usage += program;
    usage += " <command> [arguments]\n\n";
    usage += "Commands:\n";
    for (auto&& kv : commands) {
      try {
        kv.second({});
      } catch (std::exception const& ex) {
        usage += "    ";
        usage += ex.what();
        usage += "\n";
      }
    }
    return usage;
  }();

  if (argv.size() < 2) {
    std::cerr << "Missing command argument\n" << usage_msg << "\n";
    return 1;
  }
  std::string command_name = argv[1];
  argv.erase(argv.begin());  // remove the program name from the list.
  argv.erase(argv.begin());  // remove the command name from the list.

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    std::cerr << "Unknown command " << command_name << "\n"
              << usage_msg << "\n";
    return 1;
  }

  // Run the command.
  command->second(argv);
  return 0;
}

void RunAll(bool emulator) {
  auto const run_slow_integration_tests =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS")
          .value_or("");
  auto const run_slow_backup_tests =
      run_slow_integration_tests.find("backup") != std::string::npos;
  auto const run_slow_instance_tests =
      run_slow_integration_tests.find("instance") != std::string::npos;
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  if (project_id.empty()) {
    throw std::runtime_error("GOOGLE_CLOUD_PROJECT is not set or is empty");
  }

  auto test_iam_service_account =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT")
          .value_or("");
  if (!emulator && test_iam_service_account.empty()) {
    throw std::runtime_error(
        "GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT is not set or is empty");
  }

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto random_instance =
      google::cloud::spanner_testing::PickRandomInstance(generator, project_id);
  if (!random_instance) {
    throw std::runtime_error("Cannot find an instance to run the examples: " +
                             random_instance.status().message());
  }

  std::string instance_id = *std::move(random_instance);
  std::cout << "Running instance admin samples on " << instance_id << std::endl;

  google::cloud::spanner::InstanceAdminClient instance_admin_client(
      google::cloud::spanner::MakeInstanceAdminConnection(
          google::cloud::spanner::ConnectionOptions{},
          google::cloud::spanner_testing::TestRetryPolicy(),
          google::cloud::spanner_testing::TestBackoffPolicy(),
          google::cloud::spanner_testing::TestPollingPolicy()));

  std::cout << "\nRunning get-instance sample" << std::endl;
  GetInstance(instance_admin_client, project_id, instance_id);

  std::cout << "\nRunning get-instance-config sample" << std::endl;
  GetInstanceConfig(instance_admin_client, project_id,
                    emulator ? "emulator-config" : "regional-us-central1");

  std::cout << "\nRunning list-instance-configs sample" << std::endl;
  ListInstanceConfigs(instance_admin_client, project_id);

  std::cout << "\nRunning list-instances sample" << std::endl;
  ListInstances(instance_admin_client, project_id);

  if (!emulator) {
    std::cout << "\nRunning (instance) get-iam-policy sample" << std::endl;
    InstanceGetIamPolicy(instance_admin_client, project_id, instance_id);
  }

  std::string database_id =
      google::cloud::spanner_testing::RandomDatabaseName(generator);

  google::cloud::spanner::DatabaseAdminClient database_admin_client(
      MakeDatabaseAdminConnection(
          google::cloud::spanner::ConnectionOptions{},
          google::cloud::spanner_testing::TestRetryPolicy(),
          google::cloud::spanner_testing::TestBackoffPolicy(),
          google::cloud::spanner_testing::TestPollingPolicy()));

  if (run_slow_instance_tests) {
    std::string crud_instance_id =
        google::cloud::spanner_testing::RandomInstanceName(generator);
    std::cout << "\nRunning create-instance sample" << std::endl;
    PickLocationAndCreateInstance(instance_admin_client, project_id,
                                  crud_instance_id, "Test Instance");
    if (!emulator) {
      std::cout << "\nRunning update-instance sample" << std::endl;
      UpdateInstance(instance_admin_client, project_id, crud_instance_id,
                     "New name");
      std::cout << "\nRunning (instance) add-database-reader sample"
                << std::endl;
      AddDatabaseReader(instance_admin_client, project_id, crud_instance_id,
                        "serviceAccount:" + test_iam_service_account);
      std::cout << "\nRunning (instance) remove-database-reader sample"
                << std::endl;
      RemoveDatabaseReader(instance_admin_client, project_id, crud_instance_id,
                           "serviceAccount:" + test_iam_service_account);
      if (run_slow_backup_tests) {
        std::string backup_id =
            google::cloud::spanner_testing::RandomBackupName(generator);

        std::cout << "\nRunning spanner_create_database sample" << std::endl;
        CreateDatabase(database_admin_client, project_id, crud_instance_id,
                       database_id);

        std::cout << "\nRunning spanner_create_backup sample" << std::endl;
        CreateBackup(database_admin_client, project_id, crud_instance_id,
                     database_id, backup_id);

        std::cout << "\nRunning spanner_get_backup sample" << std::endl;
        GetBackup(database_admin_client, project_id, crud_instance_id,
                  backup_id);

        std::cout << "\nRunning spanner_update_backup sample" << std::endl;
        UpdateBackup(database_admin_client, project_id, crud_instance_id,
                     backup_id);

        std::string restore_database_id =
            google::cloud::spanner_testing::RandomDatabaseName(generator);

        std::cout << "\nRunning spanner_restore_database sample" << std::endl;
        RestoreDatabase(database_admin_client, project_id, crud_instance_id,
                        restore_database_id, backup_id);

        std::cout << "\nRunning spanner_drop_database sample" << std::endl;
        DropDatabase(database_admin_client, project_id, crud_instance_id,
                     restore_database_id);

        std::cout << "\nRunning spanner_delete_backup sample" << std::endl;
        DeleteBackup(database_admin_client, project_id, crud_instance_id,
                     backup_id);

        std::cout << "\nRunning spanner_cancel_backup_create sample"
                  << std::endl;
        CreateBackupAndCancel(database_admin_client, project_id,
                              crud_instance_id, database_id, backup_id);

        std::cout << "\nRunning spanner_list_backup_operations sample"
                  << std::endl;
        ListBackupOperations(database_admin_client, project_id,
                             crud_instance_id, database_id);

        std::cout << "\nRunning spanner_list_backups sample" << std::endl;
        ListBackups(database_admin_client, project_id, crud_instance_id);

        std::cout << "\nRunning spanner_list_database_operations sample"
                  << std::endl;
        ListDatabaseOperations(database_admin_client, project_id,
                               crud_instance_id);
      }
    }

    std::cout << "\nRunning delete-instance sample" << std::endl;
    DeleteInstance(instance_admin_client, project_id, crud_instance_id);
  }

  if (!emulator) {
    std::cout << "\nRunning (instance) test-iam-permissions sample"
              << std::endl;
    InstanceTestIamPermissions(instance_admin_client, project_id, instance_id);
  }

  std::cout << "Running samples in database " << database_id << "" << std::endl;

  std::cout << "\nRunning spanner_create_database sample" << std::endl;
  CreateDatabase(database_admin_client, project_id, instance_id, database_id);

  std::cout << "\nRunning spanner_create_table_with_datatypes sample"
            << std::endl;
  CreateTableWithDatatypes(database_admin_client, project_id, instance_id,
                           database_id);

  std::cout << "\nRunning spanner_create_table_with_timestamp_column sample"
            << std::endl;
  CreateTableWithTimestamp(database_admin_client, project_id, instance_id,
                           database_id);

  std::cout << "\nRunning spanner_create_index sample" << std::endl;
  AddIndex(database_admin_client, project_id, instance_id, database_id);

  std::cout << "\nRunning spanner get-database sample" << std::endl;
  GetDatabase(database_admin_client, project_id, instance_id, database_id);

  std::cout << "\nRunning spanner get-database-ddl sample" << std::endl;
  GetDatabaseDdl(database_admin_client, project_id, instance_id, database_id);

  std::cout << "\nList all databases" << std::endl;
  ListDatabases(database_admin_client, project_id, instance_id);

  std::cout << "\nRunning spanner_add_column sample" << std::endl;
  AddColumn(database_admin_client, project_id, instance_id, database_id);

  std::cout << "\nRunning spanner_add_timestamp_column sample" << std::endl;
  AddTimestampColumn(database_admin_client, project_id, instance_id,
                     database_id);

  std::cout << "\nRunning spanner_create_storing_index sample" << std::endl;
  AddStoringIndex(database_admin_client, project_id, instance_id, database_id);

  if (!emulator) {
    std::cout << "\nRunning (database) get-iam-policy sample" << std::endl;
    DatabaseGetIamPolicy(database_admin_client, project_id, instance_id,
                         database_id);

    std::cout << "\nRunning (database) add-database-reader sample" << std::endl;
    AddDatabaseReaderOnDatabase(database_admin_client, project_id, instance_id,
                                database_id,
                                "serviceAccount:" + test_iam_service_account);

    std::cout << "\nRunning (database) test-iam-permissions sample"
              << std::endl;
    DatabaseTestIamPermissions(database_admin_client, project_id, instance_id,
                               database_id, "spanner.databases.read");
  }

  // Call via RunOneCommand() for better code coverage.
  std::cout << "\nRunning spanner_quickstart sample" << std::endl;
  RunOneCommand({"", "quickstart", project_id, instance_id, database_id});

  std::cout << "\nRunning spanner_create_client_with_query_options sample"
            << std::endl;
  RunOneCommand({"", "create-client-with-query-options", project_id,
                 instance_id, database_id});

  auto client = MakeSampleClient(project_id, instance_id, database_id);

  std::cout << "\nRunning spanner_insert_data sample" << std::endl;
  InsertData(client);

  std::cout << "\nRunning spanner_update_data sample" << std::endl;
  UpdateData(client);

  std::cout << "\nRunning spanner_insert_datatypes_data sample" << std::endl;
  InsertDatatypesData(client);

  std::cout << "\nRunning spanner_query_with_array_parameter sample"
            << std::endl;
  QueryWithArrayParameter(client);

  std::cout << "\nRunning spanner_query_with_bool_parameter sample"
            << std::endl;
  QueryWithBoolParameter(client);

  std::cout << "\nRunning spanner_query_with_bytes_parameter sample"
            << std::endl;
  QueryWithBytesParameter(client);

  std::cout << "\nRunning spanner_query_with_date_parameter sample"
            << std::endl;
  QueryWithDateParameter(client);

  std::cout << "\nRunning spanner_query_with_float_parameter sample"
            << std::endl;
  QueryWithFloatParameter(client);

  std::cout << "\nRunning spanner_query_with_int_parameter sample" << std::endl;
  QueryWithIntParameter(client);

  std::cout << "\nRunning spanner_query_with_string_parameter sample"
            << std::endl;
  QueryWithStringParameter(client);

  std::cout << "\nRunning spanner_query_with_timestamp_parameter sample"
            << std::endl;
  QueryWithTimestampParameter(client);

  std::cout << "\nRunning spanner_insert_data_with_timestamp_column sample"
            << std::endl;
  InsertDataWithTimestamp(client);

  std::cout << "\nRunning spanner_update_data_with_timestamp_column sample"
            << std::endl;
  UpdateDataWithTimestamp(client);

  std::cout << "\nRunning spanner_query_data_with_timestamp_column sample"
            << std::endl;
  QueryDataWithTimestamp(client);

  // TODO(#5024): Remove this check when the emulator supports NUMERIC.
  if (!emulator) {
    std::cout << "\nRunning spanner_add_numeric_column sample" << std::endl;
    AddNumericColumn(database_admin_client, project_id, instance_id,
                     database_id);

    std::cout << "\nRunning spanner_update_data_with_numeric sample"
              << std::endl;
    UpdateDataWithNumeric(client);

    std::cout << "\nRunning spanner_query_with_numeric_parameter sample"
              << std::endl;
    QueryWithNumericParameter(client);
  }

  std::cout << "\nRunning spanner_read_only_transaction sample" << std::endl;
  ReadOnlyTransaction(client);

  std::cout << "\nRunning spanner_stale_data sample" << std::endl;
  ReadStaleData(client);

  if (!emulator) {
    std::cout << "\nRunning spanner_batch_client sample" << std::endl;
    UsePartitionQuery(client);
  }

  std::cout << "\nRunning spanner_read_data_with_index sample" << std::endl;
  ReadDataWithIndex(client);

  std::cout << "\nRunning spanner_query_data_with_new_column sample"
            << std::endl;
  QueryNewColumn(client);

  std::cout << "\nRunning spanner_profile_query sample" << std::endl;
  ProfileQuery(client);

  std::cout << "\nRunning spanner_query_data_with_index sample" << std::endl;
  QueryUsingIndex(client);

  std::cout << "\nRunning spanner_query_with_query_options sample" << std::endl;
  QueryWithQueryOptions(client);

  std::cout << "\nRunning spanner_read_data_with_storing_index sample"
            << std::endl;
  ReadDataWithStoringIndex(client);

  std::cout << "\nRunning spanner_read_write_transaction sample" << std::endl;
  ReadWriteTransaction(client);

  std::cout << "\nRunning spanner_dml_standard_insert sample" << std::endl;
  DmlStandardInsert(client);

  std::cout << "\nRunning spanner_dml_standard_update sample" << std::endl;
  DmlStandardUpdate(client);

  std::cout << "\nRunning commit-with-policies sample" << std::endl;
  CommitWithPolicies(client);

  std::cout << "\nRunning spanner_dml_standard_update_with_timestamp sample"
            << std::endl;
  DmlStandardUpdateWithTimestamp(client);

  std::cout << "\nRunning profile_spanner_dml_standard_update sample"
            << std::endl;
  ProfileDmlStandardUpdate(client);

  std::cout << "\nRunning spanner_dml_write_then_read sample" << std::endl;
  DmlWriteThenRead(client);

  if (!emulator) {
    std::cout << "\nRunning spanner_dml_batch_update sample" << std::endl;
    DmlBatchUpdate(client);
  }

  std::cout << "\nRunning spanner_write_data_for_struct_queries sample"
            << std::endl;
  WriteDataForStructQueries(client);

  std::cout << "\nRunning spanner_query_data sample" << std::endl;
  QueryData(client);

  std::cout << "\nRunning spanner_dml_getting_started_insert sample"
            << std::endl;
  DmlGettingStartedInsert(client);

  std::cout << "\nRunning spanner_dml_getting_started_update sample"
            << std::endl;
  DmlGettingStartedUpdate(client);

  std::cout << "\nRunning spanner_query_with_parameter sample" << std::endl;
  QueryWithParameter(client);

  std::cout << "\nRunning spanner_read_data sample" << std::endl;
  ReadData(client);

  std::cout << "\nRunning spanner_query_data_select_star sample" << std::endl;
  QueryDataSelectStar(client);

  std::cout << "\nRunning spanner_query_data_with_struct sample" << std::endl;
  QueryDataWithStruct(client);

  std::cout << "\nRunning spanner_query_data_with_array_of_struct sample"
            << std::endl;
  QueryDataWithArrayOfStruct(client);

  std::cout << "\nRunning spanner_field_access_on_struct_parameters sample"
            << std::endl;
  FieldAccessOnStructParameters(client);

  std::cout << "\nRunning spanner_field_access_on_nested_struct sample"
            << std::endl;
  FieldAccessOnNestedStruct(client);

  if (!emulator) {
    std::cout << "\nRunning spanner_partition_read sample" << std::endl;
    PartitionRead(client);

    std::cout << "\nRunning spanner_partition_query sample" << std::endl;
    PartitionQuery(client);
  }

  std::cout << "\nRunning example-status-or sample" << std::endl;
  ExampleStatusOr(client);

  std::cout << "\nRunning get-singular-row sample" << std::endl;
  GetSingularRow(client);

  std::cout << "\nRunning stream-of sample" << std::endl;
  StreamOf(client);

  std::cout << "\nRunning custom-retry-policy sample" << std::endl;
  RunOneCommand(
      {"", "custom-retry-policy", project_id, instance_id, database_id});

  std::cout << "\nRunning custom-instance-admin-policies sample" << std::endl;
  RunOneCommand({"", "custom-instance-admin-policies", project_id});

  std::cout << "\nRunning custom-database-admin-policies sample" << std::endl;
  RunOneCommand(
      {"", "custom-database-admin-policies", project_id, instance_id});

  if (!emulator) {
    std::cout << "\nRunning spanner_dml_partitioned_update sample" << std::endl;
    DmlPartitionedUpdate(client);

    std::cout << "\nRunning spanner_dml_partitioned_delete sample" << std::endl;
    DmlPartitionedDelete(client);
  }

  std::cout << "\nRunning spanner_dml_structs sample" << std::endl;
  DmlStructs(client);

  std::cout << "\nRunning spanner_dml_standard_delete sample" << std::endl;
  DmlStandardDelete(client);

  std::cout << "\nRunning spanner_delete_data sample" << std::endl;
  DeleteData(client);

  std::cout << "\nDeleting all data to run the mutation examples" << std::endl;
  DeleteAll(client);

  std::cout << "\nRunning the insert-mutation-builder example" << std::endl;
  InsertMutationBuilder(client);

  std::cout << "\nRunning the make-insert-mutation example" << std::endl;
  MakeInsertMutation(client);

  std::cout << "\nRunning the update-mutation-builder example" << std::endl;
  UpdateMutationBuilder(client);

  std::cout << "\nRunning the make-update-mutation example" << std::endl;
  MakeUpdateMutation(client);

  std::cout << "\nRunning the insert-or-update-mutation-builder example"
            << std::endl;
  InsertOrUpdateMutationBuilder(client);

  std::cout << "\nRunning the make-insert-or-update-mutation example"
            << std::endl;
  MakeInsertOrUpdateMutation(client);

  std::cout << "\nRunning the replace-mutation-builder example" << std::endl;
  ReplaceMutationBuilder(client);

  std::cout << "\nRunning the make-replace-mutation example" << std::endl;
  MakeReplaceMutation(client);

  std::cout << "\nRunning the delete-mutation-builder example" << std::endl;
  DeleteMutationBuilder(client);

  std::cout << "\nRunning the make-delete-mutation example" << std::endl;
  MakeDeleteMutation(client);

  std::cout << "\nRunning spanner_drop_database sample" << std::endl;
  DeleteAll(client);
  DropDatabase(database_admin_client, project_id, instance_id, database_id);
}

bool AutoRun() {
  return google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
             .value_or("") == "yes";
}

bool Emulator() {
  return google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST").has_value();
}

}  // namespace

int main(int ac, char* av[]) try {
  if (AutoRun()) {
    RunAll(Emulator());
    return 0;
  }

  return RunOneCommand({av, av + ac});
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  return 1;
}
