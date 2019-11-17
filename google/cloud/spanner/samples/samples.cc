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
#include "google/cloud/spanner/create_instance_request_builder.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/spanner/testing/random_instance_name.h"
#include "google/cloud/spanner/update_instance_request_builder.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/optional.h"
#include <chrono>
#include <sstream>
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

//! [create-instance]
void CreateInstance(google::cloud::spanner::InstanceAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id,
                    std::string const& display_name) {
  using google::cloud::future;
  using google::cloud::StatusOr;
  google::cloud::spanner::Instance in(project_id, instance_id);

  std::vector<std::string> instance_config_names = [client,
                                                    project_id]() mutable {
    std::vector<std::string> names;
    for (auto const& instance_config : client.ListInstanceConfigs(project_id)) {
      if (!instance_config) break;
      names.push_back(instance_config->name());
    }
    return names;
  }();
  if (instance_config_names.empty()) {
    throw std::runtime_error("Can not retrieve the list of instance configs.");
  }
  // Use the name of the first element from the list of instance configs.
  auto instance_config = instance_config_names[0];
  future<StatusOr<google::spanner::admin::instance::v1::Instance>> f =
      client.CreateInstance(
          google::cloud::spanner::CreateInstanceRequestBuilder(in,
                                                               instance_config)
              .SetDisplayName(display_name)
              .SetNodeCount(1)
              .SetLabels({{"label-key", "label-value"}})
              .Build());
  StatusOr<google::spanner::admin::instance::v1::Instance> instance = f.get();
  if (!instance) {
    throw std::runtime_error(instance.status().message());
  }
  std::cout << "Created instance [" << in << "]\n";
}
//! [create-instance]

void CreateInstanceCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "create-instance <project-id> <instance-id> <display_name>");
  }
  google::cloud::spanner::InstanceAdminClient client(
      google::cloud::spanner::MakeInstanceAdminConnection());
  CreateInstance(std::move(client), argv[0], argv[1], argv[2]);
}

//! [update-instance]
void UpdateInstance(google::cloud::spanner::InstanceAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id,
                    std::string const& new_display_name) {
  using google::cloud::future;
  using google::cloud::StatusOr;
  google::cloud::spanner::Instance in(project_id, instance_id);

  auto f = client.UpdateInstance(
      google::cloud::spanner::UpdateInstanceRequestBuilder(in)
          .SetDisplayName(new_display_name)
          .Build());
  auto instance = f.get();
  if (!instance) {
    throw std::runtime_error(instance.status().message());
  }
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
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
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

  auto current = client.GetIamPolicy(in);
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
  if (member_pos == binding.members().end()) {
    binding.add_members(new_reader);
  }

  auto result = client.SetIamPolicy(in, *std::move(current));
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

  auto current = client.GetIamPolicy(in);
  if (!current) throw std::runtime_error(current.status().message());

  // Find the binding for "roles/spanner.databaseReader".
  auto role_pos = std::find_if(
      current->mutable_bindings()->begin(), current->mutable_bindings()->end(),
      [](google::iam::v1::Binding const& b) {
        return b.role() == "roles/spanner.databaseReader" && !b.has_condition();
      });
  if (role_pos == current->mutable_bindings()->end()) {
    std::cout
        << "Nothing to do as the roles/spanner.databaseReader role is empty\n";
    return;
  }
  role_pos->mutable_members()->erase(
      std::remove(role_pos->mutable_members()->begin(),
                  role_pos->mutable_members()->end(), reader),
      role_pos->mutable_members()->end());

  auto result = client.SetIamPolicy(in, *std::move(current));
  if (!result) throw std::runtime_error(result.status().message());

  std::cout << "Successfully remove " << reader
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
  using google::cloud::future;
  using google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  future<StatusOr<google::spanner::admin::database::v1::Database>> f =
      client.CreateDatabase(database, {R"""(
          CREATE TABLE Singers (
              SingerId   INT64 NOT NULL,
              FirstName  STRING(1024),
              LastName   STRING(1024),
              SingerInfo BYTES(MAX)
          ) PRIMARY KEY (SingerId))""",
                                       R"""(
          CREATE TABLE Albums (
              SingerId     INT64 NOT NULL,
              AlbumId      INT64 NOT NULL,
              AlbumTitle   STRING(MAX)
          ) PRIMARY KEY (SingerId, AlbumId),
              INTERLEAVE IN PARENT Singers ON DELETE CASCADE)"""});
  StatusOr<google::spanner::admin::database::v1::Database> db = f.get();
  if (!db) {
    throw std::runtime_error(db.status().message());
  }
  std::cout << "Created database [" << database << "]\n";
}
//! [create-database] [END spanner_create_database]

void CreateDatabaseCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "create-database <project-id> <instance-id> <database-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  CreateDatabase(std::move(client), argv[0], argv[1], argv[2]);
}

// [START spanner_create_table_with_timestamp_column]
void CreateTableWithTimestamp(
    google::cloud::spanner::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  using google::cloud::future;
  using google::cloud::StatusOr;
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
  if (!metadata) {
    throw std::runtime_error(metadata.status().message());
  }
  std::cout << "`Performances` table created, new DDL:\n"
            << metadata->DebugString() << "\n";
}
// [END spanner_create_table_with_timestamp_column]

void CreateTableWithTimestampCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "create-table-with-timestamp <project-id> <instance-id> <database-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  CreateTableWithTimestamp(std::move(client), argv[0], argv[1], argv[2]);
}

// [START spanner_create_index]
void AddIndex(google::cloud::spanner::DatabaseAdminClient client,
              std::string const& project_id, std::string const& instance_id,
              std::string const& database_id) {
  using google::cloud::future;
  using google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
      f = client.UpdateDatabase(
          database, {"CREATE INDEX AlbumsByAlbumTitle ON Albums(AlbumTitle)"});
  StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
      metadata = f.get();
  if (!metadata) {
    throw std::runtime_error(metadata.status().message());
  }
  std::cout << "`AlbumsByAlbumTitle` Index successfully added, new DDL:\n"
            << metadata->DebugString() << "\n";
}
// [END spanner_create_index]

void AddIndexCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "add-index <project-id> <instance-id> <database-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  AddIndex(std::move(client), argv[0], argv[1], argv[2]);
}

//! [get-database]
void GetDatabase(google::cloud::spanner::DatabaseAdminClient client,
                 std::string const& project_id, std::string const& instance_id,
                 std::string const& database_id) {
  namespace spanner = google::cloud::spanner;
  auto database = client.GetDatabase(
      spanner::Database(project_id, instance_id, database_id));
  if (!database) throw std::runtime_error(database.status().message());
  std::cout << "Database metadata is:\n" << database->DebugString() << "\n";
}
//! [get-database]

void GetDatabaseCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "get-database <project-id> <instance-id> <database-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  GetDatabase(std::move(client), argv[0], argv[1], argv[2]);
}

//! [get-database-ddl]
void GetDatabaseDdl(google::cloud::spanner::DatabaseAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id,
                    std::string const& database_id) {
  namespace spanner = google::cloud::spanner;
  auto database = client.GetDatabaseDdl(
      spanner::Database(project_id, instance_id, database_id));
  if (!database) throw std::runtime_error(database.status().message());
  std::cout << "Database metadata is:\n" << database->DebugString() << "\n";
}
//! [get-database-ddl]

void GetDatabaseDdlCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "get-database-ddl <project-id> <instance-id> <database-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  GetDatabaseDdl(std::move(client), argv[0], argv[1], argv[2]);
}

//! [update-database] [START spanner_add_column]
void AddColumn(google::cloud::spanner::DatabaseAdminClient client,
               std::string const& project_id, std::string const& instance_id,
               std::string const& database_id) {
  using google::cloud::future;
  using google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
      f = client.UpdateDatabase(
          database, {"ALTER TABLE Albums ADD COLUMN MarketingBudget INT64"});
  StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
      metadata = f.get();
  if (!metadata) {
    throw std::runtime_error(metadata.status().message());
  }
  std::cout << "Added MarketingBudget column\n";
}
//! [update-database] [END spanner_add_column]

void AddColumnCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "add-column <project-id> <instance-id> <database-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  AddColumn(std::move(client), argv[0], argv[1], argv[2]);
}

// [START spanner_create_storing_index]
void AddStoringIndex(google::cloud::spanner::DatabaseAdminClient client,
                     std::string const& project_id,
                     std::string const& instance_id,
                     std::string const& database_id) {
  using google::cloud::future;
  using google::cloud::StatusOr;
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  future<
      StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
      f = client.UpdateDatabase(database, {R"""(
            CREATE INDEX AlbumsByAlbumTitle2 ON Albums(AlbumTitle)
                STORING (MarketingBudget))"""});
  StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
      metadata = f.get();
  if (!metadata) {
    throw std::runtime_error(metadata.status().message());
  }
  std::cout << "`AlbumsByAlbumTitle2` Index successfully added, new DDL:\n"
            << metadata->DebugString() << "\n";
}
// [END spanner_create_storing_index]

void AddStoringIndexCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "add-storing-index <project-id> <instance-id> <database-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  AddStoringIndex(std::move(client), argv[0], argv[1], argv[2]);
}

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

//! [drop-database] [START spanner_drop_database]
void DropDatabase(google::cloud::spanner::DatabaseAdminClient client,
                  std::string const& project_id, std::string const& instance_id,
                  std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  google::cloud::Status status = client.DropDatabase(database);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
  std::cout << "Database " << database << " successfully dropped\n";
}
//! [drop-database] [END spanner_drop_database]

void DropDatabaseCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "drop-database <project-id> <instance-id> <database-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  DropDatabase(std::move(client), argv[0], argv[1], argv[2]);
}

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

void DatabaseGetIamPolicyCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "database-get-iam-policy <project-id> <instance-id> <database-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  DatabaseGetIamPolicy(std::move(client), argv[0], argv[1], argv[2]);
}

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
  if (member_pos == binding.members().end()) {
    binding.add_members(new_reader);
  }

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
  namespace spanner = google::cloud::spanner;

  spanner::Client client(spanner::MakeConnection(
      spanner::Database(project_id, instance_id, database_id)));

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
  namespace spanner = google::cloud::spanner;
  return spanner::Client(spanner::MakeConnection(
      spanner::Database(project_id, instance_id, database_id)));
}

//! [START spanner_insert_data]
void InsertData(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  using google::cloud::StatusOr;
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

  auto commit_result = client.Commit(
      [&insert_singers, &insert_albums](spanner::Transaction const&) {
        return spanner::Mutations{insert_singers, insert_albums};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful [spanner_insert_data]\n";
}
//! [END spanner_insert_data]

//! [START spanner_update_data]
void UpdateData(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  using google::cloud::StatusOr;
  auto commit_result = client.Commit([](spanner::Transaction const&) {
    return spanner::Mutations{
        spanner::UpdateMutationBuilder(
            "Albums", {"SingerId", "AlbumId", "MarketingBudget"})
            .EmplaceRow(1, 1, 100000)
            .EmplaceRow(2, 2, 500000)
            .Build()};
  });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Update was successful [spanner_update_data]\n";
}
//! [END spanner_update_data]

//! [START spanner_delete_data]
void DeleteData(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  using google::cloud::StatusOr;

  // Delete each of the albums by individual key, then delete all the singers
  // using a key range.
  auto delete_albums = spanner::DeleteMutationBuilder(
                           "Albums", spanner::KeySet()
                                         .AddKey(spanner::MakeKey(1, 1))
                                         .AddKey(spanner::MakeKey(1, 2))
                                         .AddKey(spanner::MakeKey(2, 1))
                                         .AddKey(spanner::MakeKey(2, 2))
                                         .AddKey(spanner::MakeKey(2, 3)))
                           .Build();
  auto delete_singers =
      spanner::DeleteMutationBuilder(
          "Singers", spanner::KeySet().AddRange(spanner::MakeKeyBoundClosed(1),
                                                spanner::MakeKeyBoundClosed(5)))
          .Build();

  auto commit_result = client.Commit(
      [&delete_albums, &delete_singers](spanner::Transaction const&) {
        return spanner::Mutations{delete_albums, delete_singers};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Delete was successful [spanner_delete_data]\n";
}
//! [END spanner_update_data]

//! [START spanner_read_only_transaction]
void ReadOnlyTransaction(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  auto read_only = spanner::MakeReadOnlyTransaction();

  spanner::SqlStatement select(
      "SELECT SingerId, AlbumId, AlbumTitle FROM Albums");
  using RowType = std::tuple<std::int64_t, std::int64_t, std::string>;

  // Read#1.
  auto rows1 = client.ExecuteQuery(read_only, select);
  std::cout << "Read 1 results\n";
  for (auto const& row : spanner::StreamOf<RowType>(rows1)) {
    if (!row) {
      throw std::runtime_error(row.status().message());
    }
    std::cout << "SingerId: " << std::get<0>(*row)
              << " AlbumId: " << std::get<1>(*row)
              << " AlbumTitle: " << std::get<2>(*row) << "\n";
  }
  // Read#2. Even if changes occur in-between the reads the transaction ensures
  // that Read #1 and Read #2 return the same data.
  auto rows2 = client.ExecuteQuery(read_only, select);
  std::cout << "Read 2 results\n";
  for (auto const& row : spanner::StreamOf<RowType>(rows2)) {
    if (!row) {
      throw std::runtime_error(row.status().message());
    }
    std::cout << "SingerId: " << std::get<0>(*row)
              << " AlbumId: " << std::get<1>(*row)
              << " AlbumTitle: " << std::get<2>(*row) << "\n";
  }
}
//! [END spanner_read_only_transaction]

//! [START spanner_read_stale_data]
void ReadStaleData(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  auto opts = spanner::Transaction::ReadOnlyOptions(std::chrono::seconds(15));
  auto read_only = spanner::MakeReadOnlyTransaction(opts);

  spanner::SqlStatement select(
      "SELECT SingerId, AlbumId, AlbumTitle FROM Albums");
  using RowType = std::tuple<std::int64_t, std::int64_t, std::string>;

  auto rows = client.ExecuteQuery(read_only, select);
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) {
      throw std::runtime_error(row.status().message());
    }
    std::cout << "SingerId: " << std::get<0>(*row)
              << " AlbumId: " << std::get<1>(*row)
              << " AlbumTitle: " << std::get<2>(*row) << "\n";
  }
}
//! [END spanner_read_stale_data]

//! [START spanner_read_data_with_index]
void ReadDataWithIndex(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;

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
  namespace spanner = google::cloud::spanner;

  spanner::SqlStatement select(
      "SELECT SingerId, AlbumId, MarketingBudget FROM Albums");

  using RowType = std::tuple<std::int64_t, std::int64_t,
                             google::cloud::optional<std::int64_t>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\t";
    std::cout << "AlbumId: " << std::get<1>(*row) << "\t";
    auto marketing_budget = std::get<2>(*row);
    if (marketing_budget) {
      std::cout << "MarketingBudget: " << marketing_budget.value() << "\n";
    } else {
      std::cout << "MarketingBudget: NULL"
                << "\n";
    }
  }
  std::cout << "Read completed for [spanner_read_data_with_new_column]\n";
}
//! [END spanner_query_data_with_new_column]

//! [START spanner_query_data_with_index]
void QueryUsingIndex(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;

  spanner::SqlStatement select(
      "SELECT AlbumId, AlbumTitle, MarketingBudget"
      " FROM Albums@{FORCE_INDEX=AlbumsByAlbumTitle}"
      " WHERE AlbumTitle >= @start_title AND AlbumTitle < @end_title",
      {{"start_title", spanner::Value("Aardvark")},
       {"end_title", spanner::Value("Goo")}});
  using RowType = std::tuple<std::int64_t, std::string,
                             google::cloud::optional<std::int64_t>>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "AlbumId: " << std::get<0>(*row) << "\t";
    std::cout << "AlbumTitle: " << std::get<1>(*row) << "\t";
    auto marketing_budget = std::get<2>(*row);
    if (marketing_budget) {
      std::cout << "MarketingBudget: " << marketing_budget.value() << "\n";
    } else {
      std::cout << "MarketingBudget: NULL"
                << "\n";
    }
  }
  std::cout << "Read completed for [spanner_query_data_with_index]\n";
}
//! [END spanner_query_data_with_index]

//! [START spanner_read_data_with_storing_index]
void ReadDataWithStoringIndex(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;

  spanner::ReadOptions read_options;
  read_options.index_name = "AlbumsByAlbumTitle2";
  auto rows =
      client.Read("Albums", google::cloud::spanner::KeySet::All(),
                  {"AlbumId", "AlbumTitle", "MarketingBudget"}, read_options);
  using RowType = std::tuple<std::int64_t, std::string,
                             google::cloud::optional<std::int64_t>>;
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "AlbumId: " << std::get<0>(*row) << "\t";
    std::cout << "AlbumTitle: " << std::get<1>(*row) << "\t";
    auto marketing_budget = std::get<2>(*row);
    if (marketing_budget) {
      std::cout << "MarketingBudget: " << marketing_budget.value() << "\n";
    } else {
      std::cout << "MarketingBudget: NULL"
                << "\n";
    }
  }
  std::cout << "Read completed for [spanner_read_data_with_storing_index]\n";
}
//! [END spanner_read_data_with_storing_index]

//! [START spanner_read_write_transaction]
void ReadWriteTransaction(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  using google::cloud::StatusOr;

  // A helper to read a single album MarketingBudget.
  auto get_current_budget =
      [](spanner::Client client, spanner::Transaction txn,
         std::int64_t singer_id,
         std::int64_t album_id) -> StatusOr<std::int64_t> {
    auto key = spanner::KeySet().AddKey(spanner::MakeKey(singer_id, album_id));
    auto rows = client.Read(std::move(txn), "Albums", std::move(key),
                            {"MarketingBudget"});
    for (auto& row : spanner::StreamOf<std::tuple<std::int64_t>>(rows)) {
      // Return the error (as opposed to throwing an exception) because
      // Commit() only retries on StatusCode::kAborted.
      if (!row) return std::move(row).status();
      // We expect at most one result from the `Read()` request. Return
      // the first one.
      return std::get<0>(*std::move(row));
    }
    // Throw an exception because this should terminate the transaction.
    throw std::runtime_error("Key not found (" + std::to_string(singer_id) +
                             "," + std::to_string(album_id) + ")");
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
  using google::cloud::StatusOr;
  namespace spanner = google::cloud::spanner;
  auto commit_result = client.Commit(
      [&client](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto insert = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "INSERT INTO Singers (SingerId, FirstName, LastName)"
                "  VALUES (10, 'Virginia', 'Watson')",
                {}));
        if (!insert) return insert.status();
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful [spanner_dml_standard_insert]\n";
}
//! [END spanner_dml_standard_insert]

//! [START spanner_dml_standard_update]
void DmlStandardUpdate(google::cloud::spanner::Client client) {
  using google::cloud::StatusOr;
  namespace spanner = google::cloud::spanner;
  auto commit_result = client.Commit(
      [&client](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto update = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "UPDATE Albums SET MarketingBudget = MarketingBudget * 2"
                " WHERE SingerId = 1 AND AlbumId = 1",
                {}));
        if (!update) return update.status();
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Update was successful [spanner_dml_standard_update]\n";
}
//! [END spanner_dml_standard_update]

//! [START spanner_dml_standard_delete]
void DmlStandardDelete(google::cloud::spanner::Client client) {
  using google::cloud::StatusOr;
  namespace spanner = google::cloud::spanner;
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
  namespace spanner = google::cloud::spanner;
  auto result = client.ExecutePartitionedDml(
      spanner::SqlStatement("DELETE FROM Singers WHERE SingerId > 10"));
  if (!result) throw std::runtime_error(result.status().message());
  std::cout << "Delete was successful [spanner_dml_partitioned_delete]\n";
}
//! [execute-sql-partitioned] [END spanner_dml_partitioned_delete]

//! [START spanner_dml_partitioned_update]
void DmlPartitionedUpdate(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  auto result = client.ExecutePartitionedDml(
      spanner::SqlStatement("UPDATE Albums SET MarketingBudget = 100000"
                            " WHERE SingerId > 1"));
  if (!result) throw std::runtime_error(result.status().message());
  std::cout << "Update was successful [spanner_dml_partitioned_update]\n";
}
//! [END spanner_dml_partitioned_update]

//! [START spanner_write_data_for_struct_queries]
void WriteDataForStructQueries(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  using google::cloud::StatusOr;
  auto commit_result = client.Commit([](spanner::Transaction const&) {
    return spanner::Mutations{
        spanner::InsertMutationBuilder("Singers",
                                       {"SingerId", "FirstName", "LastName"})
            .EmplaceRow(6, "Elena", "Campbell")
            .EmplaceRow(7, "Gabriel", "Wright")
            .EmplaceRow(8, "Benjamin", "Martinez")
            .EmplaceRow(9, "Hannah", "Harris")
            .Build()};
  });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful "
            << "[spanner_write_data_for_struct_queries]\n";
}
//! [END spanner_write_data_for_struct_queries]

//! [START spanner_query_data]
void QueryData(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;

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
//! [END spanner_query_data]

//! [START spanner_dml_getting_started_insert]
void DmlGettingStartedInsert(google::cloud::spanner::Client client) {
  using google::cloud::StatusOr;
  namespace spanner = google::cloud::spanner;

  auto commit_result = client.Commit(
      [&client](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto insert = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "INSERT INTO Singers (SingerId, FirstName, LastName) VALUES"
                " (12, 'Melissa', 'Garcia'),"
                " (13, 'Russell', 'Morales'),"
                " (14, 'Jacqueline', 'Long'),"
                " (15, 'Dylan', 'Shaw')",
                {}));
        if (!insert) return insert.status();
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful [spanner_dml_getting_started_insert]\n";
}
//! [END spanner_dml_getting_started_insert]

//! [START spanner_query_with_parameter]
void QueryWithParameter(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;

  spanner::SqlStatement select(
      "SELECT SingerId, FirstName, LastName FROM Singers"
      " WHERE LastName = @name",
      {{"name", spanner::Value("Garcia")}});
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

//! [START spanner_read_data]
void ReadData(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;

  auto rows = client.Read("Albums", google::cloud::spanner::KeySet::All(),
                          {"SingerId", "AlbumId", "AlbumTitle"});
  using RowType = std::tuple<std::int64_t, std::int64_t, std::string>;
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\t";
    std::cout << "AlbumId: " << std::get<1>(*row) << "\n";
    std::cout << "AlbumTitle: " << std::get<2>(*row) << "\n";
  }

  std::cout << "Read completed for [spanner_read_data]\n";
}
//! [END spanner_read_data]

//! [spanner-query-data-select-star]
void QueryDataSelectStar(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;

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

  // However, if we can avoid "SELECT *" and instead enumerate the exact
  // columns we care about in the query, it's more efficient because unwanted
  // values aren't returned, and we can use `StreamOf<std::tuple<...>>` to
  // automatically parse each row into the specified `std::tuple`, thus
  // removing error paths and simplifying the code.
  spanner::SqlStatement select("SELECT SingerId, LastName FROM Singers");
  using RowType = std::tuple<std::int64_t, std::string>;
  rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\t";
    std::cout << "LastName: " << std::get<1>(*row) << "\n";
  }

  std::cout << "Query completed for [spanner_query_data_select_star]\n";
}
//! [spanner-query-data-select-star]

//! [START spanner_query_data_with_struct]
void QueryDataWithStruct(google::cloud::spanner::Client client) {
  //! [spanner-sql-statement-params] [START spanner_create_struct_with_data]
  namespace spanner = google::cloud::spanner;
  auto singer_info = std::make_tuple("Elena", "Campbell");
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
  namespace spanner = google::cloud::spanner;
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
  namespace spanner = google::cloud::spanner;

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

//! [START spanner_field_access_on_nested_struct]
void FieldAccessOnNestedStruct(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;

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
//! [END spanner_field_access_on_nested_struct]

void ExampleStatusOr(google::cloud::spanner::Client client) {
  //! [example-status-or]
  namespace spanner = google::cloud::spanner;
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

class RemoteConnectionFake {
 public:
  void Send(std::string const& serialized_partition) {
    serialized_partition_in_transit_ = serialized_partition;
  }
  std::string Receive() { return serialized_partition_in_transit_; }
  void SendPartitionToRemoteMachine(
      google::cloud::spanner::ReadPartition const& partition) {
    //! [serialize-read-partition]
    namespace spanner = google::cloud::spanner;
    google::cloud::StatusOr<std::string> serialized_partition =
        spanner::SerializeReadPartition(partition);
    if (!serialized_partition) {
      throw std::runtime_error(serialized_partition.status().message());
    }
    Send(*serialized_partition);
    //! [serialize-read-partition]
  }

  void SendPartitionToRemoteMachine(
      google::cloud::spanner::QueryPartition const& partition) {
    //! [serialize-query-partition]
    namespace spanner = google::cloud::spanner;
    google::cloud::StatusOr<std::string> serialized_partition =
        spanner::SerializeQueryPartition(partition);
    if (!serialized_partition) {
      throw std::runtime_error(serialized_partition.status().message());
    }
    Send(*serialized_partition);
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
  namespace spanner = google::cloud::spanner;
  RemoteConnectionFake remote_connection;
  //! [key-set-builder]
  auto key_set = spanner::KeySet().AddRange(spanner::MakeKeyBoundClosed(1),
                                            spanner::MakeKeyBoundClosed(10));
  //! [key-set-builder]

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
  namespace spanner = google::cloud::spanner;
  RemoteConnectionFake remote_connection;
  //! [partition-query]
  spanner::Transaction ro_transaction = spanner::MakeReadOnlyTransaction();
  google::cloud::StatusOr<std::vector<spanner::QueryPartition>> partitions =
      client.PartitionQuery(
          ro_transaction,
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
  auto make_command_entry = [](char const* sample_name, SampleFunction sample) {
    auto make_command = [](char const* sample_name, SampleFunction sample) {
      return [sample_name, sample](std::vector<std::string> const& argv) {
        if (argv.size() != 3) {
          throw std::runtime_error(std::string(sample_name) +
                                   " <project-id> <instance-id> <database-id>");
        }
        sample(MakeSampleClient(argv[0], argv[1], argv[2]));
      };
    };
    return CommandMap::value_type(sample_name,
                                  make_command(sample_name, sample));
  };

  CommandMap commands = {
      {"get-instance", &GetInstanceCommand},
      {"create-instance", &CreateInstanceCommand},
      {"update-instance", &UpdateInstanceCommand},
      {"delete-instance", &DeleteInstanceCommand},
      {"list-instance-configs", &ListInstanceConfigsCommand},
      {"get-instance-config", &GetInstanceConfigCommand},
      {"list-instances", &ListInstancesCommand},
      {"instance-get-iam-policy", &InstanceGetIamPolicyCommand},
      {"add-database-reader", &AddDatabaseReaderCommand},
      {"remove-database-reader", &RemoveDatabaseReaderCommand},
      {"instance-test-iam-permissions", &InstanceTestIamPermissionsCommand},
      {"create-database", &CreateDatabaseCommand},
      {"create-table-with-timestamp", &CreateTableWithTimestampCommand},
      {"add-index", &AddIndexCommand},
      {"add-storing-index", &AddStoringIndexCommand},
      {"get-database", &GetDatabaseCommand},
      {"get-database-ddl", &GetDatabaseDdlCommand},
      {"add-column", &AddColumnCommand},
      {"list-databases", &ListDatabasesCommand},
      {"drop-database", &DropDatabaseCommand},
      {"database-get-iam-policy", &DatabaseGetIamPolicyCommand},
      {"add-database-reader-on-database", &AddDatabaseReaderOnDatabaseCommand},
      {"database-test-iam-permissions", &DatabaseTestIamPermissionsCommand},
      {"quickstart", &QuickstartCommand},
      make_command_entry("insert-data", &InsertData),
      make_command_entry("update-data", &UpdateData),
      make_command_entry("delete-data", &DeleteData),
      make_command_entry("read-only-transaction", &ReadOnlyTransaction),
      make_command_entry("read-stale-data", &ReadStaleData),
      make_command_entry("read-data-with-index", &ReadDataWithIndex),
      make_command_entry("query-new-column", &QueryNewColumn),
      make_command_entry("query-data-with-index", &QueryUsingIndex),
      make_command_entry("read-data-with-storing-index",
                         &ReadDataWithStoringIndex),
      make_command_entry("read-write-transaction", &ReadWriteTransaction),
      make_command_entry("dml-standard-insert", &DmlStandardInsert),
      make_command_entry("dml-standard-update", &DmlStandardUpdate),
      make_command_entry("dml-standard-delete", &DmlStandardDelete),
      make_command_entry("dml-partitioned-update", &DmlPartitionedUpdate),
      make_command_entry("dml-partitioned-delete", &DmlPartitionedDelete),
      make_command_entry("write-data-for-struct-queries",
                         &WriteDataForStructQueries),
      make_command_entry("query-data", &QueryData),
      make_command_entry("getting-started-insert", &DmlGettingStartedInsert),
      make_command_entry("query-with-parameter", &QueryWithParameter),
      make_command_entry("read-data", &ReadData),
      make_command_entry("query-data-select-star", &QueryDataSelectStar),
      make_command_entry("query-data-with-struct", &QueryDataWithStruct),
      make_command_entry("query-data-with-array-of-struct",
                         &QueryDataWithArrayOfStruct),
      make_command_entry("field-access-struct-parameters",
                         &FieldAccessOnStructParameters),
      make_command_entry("field-access-on-nested-struct",
                         &FieldAccessOnNestedStruct),
      make_command_entry("partition-read", &PartitionRead),
      make_command_entry("partition-query", &PartitionQuery),
      make_command_entry("example-status-or", &ExampleStatusOr),
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

void RunAll() {
  auto run_slow_integration_tests =
      google::cloud::internal::GetEnv("RUN_SLOW_INTEGRATION_TESTS")
          .value_or("");
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  if (project_id.empty()) {
    throw std::runtime_error("GOOGLE_CLOUD_PROJECT is not set or is empty");
  }

  auto test_iam_service_account =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_IAM_TEST_SA")
          .value_or("");
  if (test_iam_service_account.empty()) {
    throw std::runtime_error(
        "GOOGLE_CLOUD_CPP_SPANNER_IAM_TEST_SA is not set or is empty");
  }

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto random_instance =
      google::cloud::spanner_testing::PickRandomInstance(generator, project_id);
  if (!random_instance) {
    throw std::runtime_error("Cannot find an instance to run the examples: " +
                             random_instance.status().message());
  }

  std::string instance_id = *std::move(random_instance);
  std::cout << "Running instance admin samples on " << instance_id << "\n";

  google::cloud::spanner::InstanceAdminClient instance_admin_client(
      google::cloud::spanner::MakeInstanceAdminConnection());

  std::cout << "\nRunning get-instance sample\n";
  GetInstance(instance_admin_client, project_id, instance_id);

  std::cout << "\nRunning get-instance-config sample\n";
  GetInstanceConfig(instance_admin_client, project_id, "regional-us-central1");

  std::cout << "\nRunning list-instance-configs sample\n";
  ListInstanceConfigs(instance_admin_client, project_id);

  std::cout << "\nRunning list-instances sample\n";
  ListInstances(instance_admin_client, project_id);

  std::cout << "\nRunning (instance) get-iam-policy sample\n";
  InstanceGetIamPolicy(instance_admin_client, project_id, instance_id);

  if (run_slow_integration_tests == "yes") {
    std::string crud_instance_id =
        google::cloud::spanner_testing::RandomInstanceName(generator);
    std::cout << "\nRunning create-instance sample\n";
    CreateInstance(instance_admin_client, project_id, crud_instance_id,
                   "Test Instance");
    std::cout << "\nRunning update-instance sample\n";
    UpdateInstance(instance_admin_client, project_id, crud_instance_id,
                   "New name");
    std::cout << "\nRunning (instance) add-database-reader sample\n";
    AddDatabaseReader(instance_admin_client, project_id, crud_instance_id,
                      "serviceAccount:" + test_iam_service_account);
    std::cout << "\nRunning (instance) remove-database-reader sample\n";
    RemoveDatabaseReader(instance_admin_client, project_id, crud_instance_id,
                         "serviceAccount:" + test_iam_service_account);
    std::cout << "\nRunning delete-instance sample\n";
    DeleteInstance(instance_admin_client, project_id, crud_instance_id);
  }

  std::cout << "\nRunning (instance) test-iam-permissions sample\n";
  InstanceTestIamPermissions(instance_admin_client, project_id, instance_id);

  std::string database_id =
      google::cloud::spanner_testing::RandomDatabaseName(generator);

  std::cout << "Running samples in database " << database_id << "\n";

  google::cloud::spanner::DatabaseAdminClient database_admin_client;
  std::cout << "\nRunning spanner_create_database sample\n";
  CreateDatabase(database_admin_client, project_id, instance_id, database_id);

  std::cout << "\nRunning spanner_create_table_with_timestamp_column sample\n";
  CreateTableWithTimestamp(database_admin_client, project_id, instance_id,
                           database_id);

  std::cout << "\nRunning spanner_create_index sample\n";
  AddIndex(database_admin_client, project_id, instance_id, database_id);

  std::cout << "\nRunning spanner get-database sample\n";
  GetDatabase(database_admin_client, project_id, instance_id, database_id);

  std::cout << "\nRunning spanner get-database-ddl sample\n";
  GetDatabaseDdl(database_admin_client, project_id, instance_id, database_id);

  std::cout << "\nList all databases\n";
  ListDatabases(database_admin_client, project_id, instance_id);

  std::cout << "\nRunning spanner_add_column sample\n";
  AddColumn(database_admin_client, project_id, instance_id, database_id);

  std::cout << "\nRunning spanner_create_storing_index sample\n";
  AddStoringIndex(database_admin_client, project_id, instance_id, database_id);

  std::cout << "\nRunning (database) get-iam-policy sample\n";
  DatabaseGetIamPolicy(database_admin_client, project_id, instance_id,
                       database_id);

  std::cout << "\nRunning (database) add-database-reader sample\n";
  AddDatabaseReaderOnDatabase(database_admin_client, project_id, instance_id,
                              database_id,
                              "serviceAccount:" + test_iam_service_account);

  std::cout << "\nRunning (database) test-iam-permissions sample\n";
  DatabaseTestIamPermissions(database_admin_client, project_id, instance_id,
                             database_id, "spanner.databases.read");

  std::cout << "\nRunning spanner_quickstart sample\n";
  Quickstart(project_id, instance_id, database_id);

  auto client = MakeSampleClient(project_id, instance_id, database_id);

  std::cout << "\nRunning spanner_insert_data sample\n";
  InsertData(client);

  std::cout << "\nRunning spanner_update_data sample\n";
  UpdateData(client);

  std::cout << "\nRunning spanner_read_only_transaction sample\n";
  ReadOnlyTransaction(client);

  std::cout << "\nRunning spanner_stale_data sample\n";
  ReadStaleData(client);

  std::cout << "\nRunning spanner_read_data_with_index sample\n";
  ReadDataWithIndex(client);

  std::cout << "\nRunning spanner_query_data_with_new_column sample\n";
  QueryNewColumn(client);

  std::cout << "\nRunning spanner_query_data_with_index sample\n";
  QueryUsingIndex(client);

  std::cout << "\nRunning spanner_read_data_with_storing_index sample\n";
  ReadDataWithStoringIndex(client);

  std::cout << "\nRunning spanner_read_write_transaction sample\n";
  ReadWriteTransaction(client);

  std::cout << "\nRunning spanner_dml_standard_insert sample\n";
  DmlStandardInsert(client);

  std::cout << "\nRunning spanner_dml_standard_update sample\n";
  DmlStandardUpdate(client);

  std::cout << "\nRunning spanner_dml_standard_delete sample\n";
  DmlStandardDelete(client);

  std::cout << "\nRunning spanner_delete_data sample\n";
  DeleteData(client);

  std::cout << "\nRunning spanner_write_data_for_struct_queries sample\n";
  WriteDataForStructQueries(client);

  std::cout << "\nRunning spanner_query_data sample\n";
  QueryData(client);

  std::cout << "\nRunning spanner_dml_getting_started_insert sample\n";
  DmlGettingStartedInsert(client);

  std::cout << "\nRunning spanner_query_with_parameter sample\n";
  QueryWithParameter(client);

  std::cout << "\nRunning spanner_read_data sample\n";
  ReadData(client);

  std::cout << "\nRunning spanner_query_data_select_star sample\n";
  QueryDataSelectStar(client);

  std::cout << "\nRunning spanner_query_data_with_struct sample\n";
  QueryDataWithStruct(client);

  std::cout << "\nRunning spanner_query_data_with_array_of_struct sample\n";
  QueryDataWithArrayOfStruct(client);

  std::cout << "\nRunning spanner_field_access_on_struct_parameters sample\n";
  FieldAccessOnStructParameters(client);

  std::cout << "\nRunning spanner_field_access_on_nested_struct sample\n";
  FieldAccessOnNestedStruct(client);

  std::cout << "\nRunning spanner_partition_read sample\n";
  PartitionRead(client);

  std::cout << "\nRunning spanner_partition_query sample\n";
  PartitionQuery(client);

  std::cout << "\nRunning example-status-or sample\n";
  ExampleStatusOr(client);

  std::cout << "\nRunning spanner_dml_partitioned_update sample\n";
  DmlPartitionedUpdate(client);

  std::cout << "\nRunning spanner_dml_partitioned_delete sample\n";
  DmlPartitionedDelete(client);

  std::cout << "\nRunning spanner_drop_database sample\n";
  DropDatabase(database_admin_client, project_id, instance_id, database_id);
}

bool AutoRun() {
  return google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
             .value_or("") == "yes";
}

}  // namespace

int main(int ac, char* av[]) try {
  if (AutoRun()) {
    RunAll();
    return 0;
  }

  return RunOneCommand({av, av + ac});
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  return 1;
}
