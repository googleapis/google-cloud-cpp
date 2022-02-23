// Copyright 2019 Google LLC
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

//! [START spanner_quickstart]
#include "google/cloud/spanner/client.h"
//! [END spanner_quickstart]
#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/admin/database_admin_options.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"
#include "google/cloud/spanner/admin/instance_admin_options.h"
#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/backup.h"
#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/spanner/create_instance_request_builder.h"
#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/testing/instance_location.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_backup_name.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/spanner/testing/random_instance_name.h"
#include "google/cloud/spanner/update_instance_request_builder.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/kms_key_name.h"
#include "google/cloud/log.h"
#include "google/cloud/project.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include <chrono>
#include <iomanip>
#include <iterator>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

//! [get-instance]
void GetInstance(google::cloud::spanner_admin::InstanceAdminClient client,
                 std::string const& project_id,
                 std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  auto instance = client.GetInstance(in.FullName());
  if (!instance) throw std::runtime_error(instance.status().message());

  std::cout << "The instance " << instance->name()
            << " exists and its metadata is:\n"
            << instance->DebugString();
}
//! [get-instance]

void GetInstanceCommand(std::vector<std::string> argv) {
  if (argv.size() != 2) {
    throw std::runtime_error("get-instance <project-id> <instance-id>");
  }
  google::cloud::spanner_admin::InstanceAdminClient client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection());
  GetInstance(std::move(client), argv[0], argv[1]);
}

//! [START spanner_create_instance] [create-instance]
void CreateInstance(google::cloud::spanner_admin::InstanceAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id,
                    std::string const& display_name,
                    std::string const& config) {
  namespace spanner = ::google::cloud::spanner;
  spanner::Instance in(project_id, instance_id);

  auto project = google::cloud::Project(project_id);
  std::string instance_config =
      project.FullName() + "/instanceConfigs/" + config;
  auto instance =
      client
          .CreateInstance(
              spanner::CreateInstanceRequestBuilder(in, instance_config)
                  .SetDisplayName(display_name)
                  .SetNodeCount(1)
                  .SetLabels({{"cloud_spanner_samples", "true"}})
                  .Build())
          .get();
  if (!instance) throw std::runtime_error(instance.status().message());
  std::cout << "Created instance [" << in << "]:\n" << instance->DebugString();
}
//! [END spanner_create_instance] [create-instance]

// [START spanner_create_instance_with_processing_units]
void CreateInstanceWithProcessingUnits(
    google::cloud::spanner_admin::InstanceAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& display_name, std::string const& config) {
  namespace spanner = ::google::cloud::spanner;
  spanner::Instance in(project_id, instance_id);

  auto project = google::cloud::Project(project_id);
  std::string instance_config =
      project.FullName() + "/instanceConfigs/" + config;
  auto instance =
      client
          .CreateInstance(
              spanner::CreateInstanceRequestBuilder(in, instance_config)
                  .SetDisplayName(display_name)
                  .SetProcessingUnits(500)
                  .SetLabels({{"cloud_spanner_samples", "true"}})
                  .Build())
          .get();
  if (!instance) throw std::runtime_error(instance.status().message());
  std::cout << "Created instance [" << in << "]:\n" << instance->DebugString();

  instance = client.GetInstance(in.FullName());
  if (!instance) throw std::runtime_error(instance.status().message());
  std::cout << "Instance " << in << " has " << instance->processing_units()
            << " processing units.\n";
}
// [END spanner_create_instance_with_processing_units]

void CreateInstanceCommand(std::vector<std::string> argv) {
  bool low_cost = !argv.empty() && argv.front() == "--low-cost";
  if (low_cost) argv.erase(argv.begin());
  if (argv.size() != 3 && argv.size() != 4) {
    throw std::runtime_error(
        "create-instance [--low-cost] <project-id> <instance-id>"
        " <display_name> [config]");
  }
  google::cloud::spanner_admin::InstanceAdminClient client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection());
  std::string config = argv.size() == 4 ? argv[3] : "regional-us-central1";
  if (low_cost) {
    CreateInstanceWithProcessingUnits(std::move(client), argv[0], argv[1],
                                      argv[2], config);
  } else {
    CreateInstance(std::move(client), argv[0], argv[1], argv[2], config);
  }
}

//! [update-instance]
void UpdateInstance(google::cloud::spanner_admin::InstanceAdminClient client,
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

void UpdateInstanceCommand(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "update-instance <project-id> <instance-id> <new_display_name>");
  }
  google::cloud::spanner_admin::InstanceAdminClient client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection());
  UpdateInstance(std::move(client), argv[0], argv[1], argv[2]);
}

//! [delete-instance]
void DeleteInstance(google::cloud::spanner_admin::InstanceAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);

  auto status = client.DeleteInstance(in.FullName());
  if (!status.ok()) throw std::runtime_error(status.message());
  std::cout << "Deleted instance [" << in << "]\n";
}
//! [delete-instance]

void DeleteInstanceCommand(std::vector<std::string> argv) {
  if (argv.size() != 2) {
    throw std::runtime_error("delete-instance <project-id> <instance-id>");
  }
  google::cloud::spanner_admin::InstanceAdminClient client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection());
  DeleteInstance(std::move(client), argv[0], argv[1]);
}

//! [START spanner_list_instance_configs] [list-instance-configs]
void ListInstanceConfigs(
    google::cloud::spanner_admin::InstanceAdminClient client,
    std::string const& project_id) {
  int count = 0;
  auto project = google::cloud::Project(project_id);
  for (auto const& instance_config :
       client.ListInstanceConfigs(project.FullName())) {
    if (!instance_config) {
      throw std::runtime_error(instance_config.status().message());
    }
    ++count;
    std::cout << "Instance config [" << count << "]:\n"
              << instance_config->DebugString();
  }
  if (count == 0) {
    std::cout << "No instance configs found in project " << project_id << "\n";
  }
}
//! [END spanner_list_instance_configs] [list-instance-configs]

void ListInstanceConfigsCommand(std::vector<std::string> argv) {
  if (argv.size() != 1) {
    throw std::runtime_error("list-instance-configs <project-id>");
  }
  google::cloud::spanner_admin::InstanceAdminClient client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection());
  ListInstanceConfigs(std::move(client), argv[0]);
}

//! [START spanner_get_instance_config] [get-instance-config]
void GetInstanceConfig(google::cloud::spanner_admin::InstanceAdminClient client,
                       std::string const& project_id,
                       std::string const& instance_config_name) {
  auto project = google::cloud::Project(project_id);
  auto instance_config = client.GetInstanceConfig(
      project.FullName() + "/instanceConfigs/" + instance_config_name);
  if (!instance_config) {
    throw std::runtime_error(instance_config.status().message());
  }

  std::cout << "The instanceConfig " << instance_config->name()
            << " exists and its metadata is:\n"
            << instance_config->DebugString();
}
//! [END spanner_get_instance_config] [get-instance-config]

void GetInstanceConfigCommand(std::vector<std::string> argv) {
  if (argv.size() != 2) {
    throw std::runtime_error(
        "get-instance-config <project-id> <instance-config-name>");
  }
  google::cloud::spanner_admin::InstanceAdminClient client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection());
  GetInstanceConfig(std::move(client), argv[0], argv[1]);
}

//! [list-instances]
void ListInstances(google::cloud::spanner_admin::InstanceAdminClient client,
                   std::string const& project_id) {
  int count = 0;
  auto project = google::cloud::Project(project_id);
  for (auto const& instance : client.ListInstances(project.FullName())) {
    if (!instance) throw std::runtime_error(instance.status().message());
    ++count;
    std::cout << "Instance [" << count << "]:\n" << instance->DebugString();
  }
  if (count == 0) {
    std::cout << "No instances found in project " << project_id << "\n";
  }
}
//! [list-instances]

void ListInstancesCommand(std::vector<std::string> argv) {
  if (argv.size() != 1) {
    throw std::runtime_error("list-instances <project-id>");
  }
  google::cloud::spanner_admin::InstanceAdminClient client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection());
  ListInstances(std::move(client), argv[0]);
}

//! [instance-get-iam-policy]
void InstanceGetIamPolicy(
    google::cloud::spanner_admin::InstanceAdminClient client,
    std::string const& project_id, std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  auto actual = client.GetIamPolicy(in.FullName());
  if (!actual) throw std::runtime_error(actual.status().message());

  std::cout << "The IAM policy for instance " << instance_id << " is:\n"
            << actual->DebugString();
}
//! [instance-get-iam-policy]

void InstanceGetIamPolicyCommand(std::vector<std::string> argv) {
  if (argv.size() != 2) {
    throw std::runtime_error(
        "instance-get-iam-policy <project-id> <instance-id>");
  }
  google::cloud::spanner_admin::InstanceAdminClient client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection());
  InstanceGetIamPolicy(std::move(client), argv[0], argv[1]);
}

//! [add-database-reader]
void AddDatabaseReader(google::cloud::spanner_admin::InstanceAdminClient client,
                       std::string const& project_id,
                       std::string const& instance_id,
                       std::string const& new_reader) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  auto result = client.SetIamPolicy(
      in.FullName(),
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
            << result->DebugString();
}
//! [add-database-reader]

void AddDatabaseReaderCommand(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "add-database-reader <project-id> <instance-id> <new-reader>");
  }
  google::cloud::spanner_admin::InstanceAdminClient client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection());
  AddDatabaseReader(std::move(client), argv[0], argv[1], argv[2]);
}

//! [remove-database-reader]
void RemoveDatabaseReader(
    google::cloud::spanner_admin::InstanceAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& reader) {
  google::cloud::spanner::Instance in(project_id, instance_id);

  auto result = client.SetIamPolicy(
      in.FullName(),
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
            << result->DebugString();
}
//! [remove-database-reader]

void RemoveDatabaseReaderCommand(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "remove-database-reader <project-id> <instance-id> <existing-reader>");
  }
  google::cloud::spanner_admin::InstanceAdminClient client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection());
  RemoveDatabaseReader(std::move(client), argv[0], argv[1], argv[2]);
}

//! [instance-test-iam-permissions]
void InstanceTestIamPermissions(
    google::cloud::spanner_admin::InstanceAdminClient client,
    std::string const& project_id, std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  auto actual =
      client.TestIamPermissions(in.FullName(), {"spanner.databases.list"});
  if (!actual) throw std::runtime_error(actual.status().message());

  char const* msg = actual->permissions().empty() ? "does not" : "does";

  std::cout
      << "The caller " << msg
      << " have permission to list databases on the Cloud Spanner instance "
      << in.instance_id() << "\n";
}
//! [instance-test-iam-permissions]

void InstanceTestIamPermissionsCommand(std::vector<std::string> argv) {
  if (argv.size() != 2) {
    throw std::runtime_error(
        "instance-test-iam-permissions <project-id> <instance-id>");
  }
  google::cloud::spanner_admin::InstanceAdminClient client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection());
  InstanceTestIamPermissions(std::move(client), argv[0], argv[1]);
}

google::cloud::spanner::Timestamp DatabaseNow(
    google::cloud::spanner::Client client) {
  auto rows = client.ExecuteQuery(
      google::cloud::spanner::SqlStatement("SELECT CURRENT_TIMESTAMP()"));
  using RowType = std::tuple<google::cloud::spanner::Timestamp>;
  auto row = google::cloud::spanner::GetSingularRow(
      google::cloud::spanner::StreamOf<RowType>(rows));
  if (!row) throw std::runtime_error(row.status().message());
  return std::get<0>(*row);
}

google::cloud::spanner::Timestamp TimestampAdd(
    google::cloud::spanner::Timestamp ts, absl::Duration d) {
  auto const t = ts.get<absl::Time>().value();
  return google::cloud::spanner::MakeTimestamp(t + d).value();
}

google::cloud::spanner::Client MakeSampleClient(
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  return google::cloud::spanner::Client(google::cloud::spanner::MakeConnection(
      google::cloud::spanner::Database(project_id, instance_id, database_id)));
}

//! [create-database] [START spanner_create_database]
void CreateDatabase(google::cloud::spanner_admin::DatabaseAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id,
                    std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  google::spanner::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent(database.instance().FullName());
  request.set_create_statement("CREATE DATABASE `" + database.database_id() +
                               "`");
  request.add_extra_statements(R"""(
      CREATE TABLE Singers (
          SingerId   INT64 NOT NULL,
          FirstName  STRING(1024),
          LastName   STRING(1024),
          SingerInfo BYTES(MAX)
      ) PRIMARY KEY (SingerId))""");
  request.add_extra_statements(R"""(
      CREATE TABLE Albums (
          SingerId     INT64 NOT NULL,
          AlbumId      INT64 NOT NULL,
          AlbumTitle   STRING(MAX)
      ) PRIMARY KEY (SingerId, AlbumId),
          INTERLEAVE IN PARENT Singers ON DELETE CASCADE)""");
  auto db = client.CreateDatabase(request).get();
  if (!db) throw std::runtime_error(db.status().message());
  std::cout << "Database " << db->name() << " created.\n";
}
//! [create-database] [END spanner_create_database]

// [START spanner_create_database_with_version_retention_period]
void CreateDatabaseWithVersionRetentionPeriod(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  google::spanner::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent(database.instance().FullName());
  request.set_create_statement("CREATE DATABASE `" + database.database_id() +
                               "`");
  request.add_extra_statements("ALTER DATABASE `" + database.database_id() +
                               "` " +
                               "SET OPTIONS (version_retention_period='2h')");
  request.add_extra_statements(R"""(
      CREATE TABLE Singers (
          SingerId   INT64 NOT NULL,
          FirstName  STRING(1024),
          LastName   STRING(1024),
          SingerInfo BYTES(MAX)
      ) PRIMARY KEY (SingerId))""");
  request.add_extra_statements(R"""(
      CREATE TABLE Albums (
          SingerId     INT64 NOT NULL,
          AlbumId      INT64 NOT NULL,
          AlbumTitle   STRING(MAX)
      ) PRIMARY KEY (SingerId, AlbumId),
          INTERLEAVE IN PARENT Singers ON DELETE CASCADE)""");
  auto db = client.CreateDatabase(request).get();
  if (!db) throw std::runtime_error(db.status().message());
  std::cout << "Database " << db->name() << " created.\n";

  auto ddl = client.GetDatabaseDdl(db->name());
  if (!ddl) throw std::runtime_error(ddl.status().message());
  std::cout << "Database DDL is:\n" << ddl->DebugString();
}
// [END spanner_create_database_with_version_retention_period]

// [START spanner_create_database_with_default_leader]
void CreateDatabaseWithDefaultLeader(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id, std::string const& default_leader) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  google::spanner::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent(database.instance().FullName());
  request.set_create_statement("CREATE DATABASE `" + database.database_id() +
                               "`");
  request.add_extra_statements("ALTER DATABASE `" + database_id + "` " +
                               "SET OPTIONS (default_leader='" +
                               default_leader + "')");
  auto db = client.CreateDatabase(request).get();
  if (!db) throw std::runtime_error(db.status().message());
  std::cout << "Database " << db->name() << " created.\n" << db->DebugString();
}
// [END spanner_create_database_with_default_leader]

// [START spanner_update_database_with_default_leader]
void UpdateDatabaseWithDefaultLeader(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id, std::string const& default_leader) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  std::vector<std::string> statements;
  statements.push_back("ALTER DATABASE `" + database_id + "` " +
                       "SET OPTIONS (default_leader='" + default_leader + "')");
  auto metadata =
      client.UpdateDatabaseDdl(database.FullName(), std::move(statements))
          .get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`default_leader` altered, new DDL metadata:\n"
            << metadata->DebugString();
}
// [END spanner_update_database_with_default_leader]

// [START spanner_create_table_with_datatypes]
void CreateTableWithDatatypes(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto metadata = client
                      .UpdateDatabaseDdl(database.FullName(), {R"""(
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
                        ) PRIMARY KEY (VenueId))"""})
                      .get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`Venues` table created, new DDL:\n" << metadata->DebugString();
}
// [END spanner_create_table_with_datatypes]

// [START spanner_create_table_with_timestamp_column]
void CreateTableWithTimestamp(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto metadata = client
                      .UpdateDatabaseDdl(database.FullName(), {R"""(
                        CREATE TABLE Performances (
                            SingerId        INT64 NOT NULL,
                            VenueId         INT64 NOT NULL,
                            EventDate       Date,
                            Revenue         INT64,
                            LastUpdateTime  TIMESTAMP NOT NULL OPTIONS
                                (allow_commit_timestamp=true)
                        ) PRIMARY KEY (SingerId, VenueId, EventDate),
                            INTERLEAVE IN PARENT Singers ON DELETE CASCADE)"""})
                      .get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`Performances` table created, new DDL:\n"
            << metadata->DebugString();
}
// [END spanner_create_table_with_timestamp_column]

// [START spanner_create_index]
void AddIndex(google::cloud::spanner_admin::DatabaseAdminClient client,
              std::string const& project_id, std::string const& instance_id,
              std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto metadata =
      client
          .UpdateDatabaseDdl(
              database.FullName(),
              {"CREATE INDEX AlbumsByAlbumTitle ON Albums(AlbumTitle)"})
          .get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`AlbumsByAlbumTitle` Index successfully added, new DDL:\n"
            << metadata->DebugString();
}
// [END spanner_create_index]

//! [get-database]
void GetDatabase(google::cloud::spanner_admin::DatabaseAdminClient client,
                 std::string const& project_id, std::string const& instance_id,
                 std::string const& database_id) {
  namespace spanner = ::google::cloud::spanner;
  auto database = client.GetDatabase(
      spanner::Database(project_id, instance_id, database_id).FullName());
  if (!database) throw std::runtime_error(database.status().message());
  std::cout << "Database metadata is:\n" << database->DebugString();
}
//! [get-database]

//! [START spanner_get_database_ddl] [get-database-ddl]
void GetDatabaseDdl(google::cloud::spanner_admin::DatabaseAdminClient client,
                    std::string const& project_id,
                    std::string const& instance_id,
                    std::string const& database_id) {
  namespace spanner = ::google::cloud::spanner;
  auto database = client.GetDatabaseDdl(
      spanner::Database(project_id, instance_id, database_id).FullName());
  if (!database) throw std::runtime_error(database.status().message());
  std::cout << "Database metadata is:\n" << database->DebugString();
}
//! [END spanner_get_database_ddl] [get-database-ddl]

//! [update-database] [START spanner_add_column]
void AddColumn(google::cloud::spanner_admin::DatabaseAdminClient client,
               std::string const& project_id, std::string const& instance_id,
               std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto metadata =
      client
          .UpdateDatabaseDdl(
              database.FullName(),
              {"ALTER TABLE Albums ADD COLUMN MarketingBudget INT64"})
          .get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "Added MarketingBudget column\n";
}
//! [update-database] [END spanner_add_column]

// [START spanner_add_timestamp_column]
void AddTimestampColumn(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto metadata =
      client
          .UpdateDatabaseDdl(
              database.FullName(),
              {"ALTER TABLE Albums ADD COLUMN LastUpdateTime TIMESTAMP "
               "OPTIONS (allow_commit_timestamp=true)"})
          .get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "Added LastUpdateTime column\n";
}
// [END spanner_add_timestamp_column]

// [START spanner_create_storing_index]
void AddStoringIndex(google::cloud::spanner_admin::DatabaseAdminClient client,
                     std::string const& project_id,
                     std::string const& instance_id,
                     std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto metadata = client
                      .UpdateDatabaseDdl(database.FullName(), {R"""(
                        CREATE INDEX AlbumsByAlbumTitle2 ON Albums(AlbumTitle)
                            STORING (MarketingBudget))"""})
                      .get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`AlbumsByAlbumTitle2` Index successfully added, new DDL:\n"
            << metadata->DebugString();
}
// [END spanner_create_storing_index]

//! [START spanner_list_databases] [list-databases]
void ListDatabases(google::cloud::spanner_admin::DatabaseAdminClient client,
                   std::string const& project_id,
                   std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  int count = 0;
  for (auto const& database : client.ListDatabases(in.FullName())) {
    if (!database) throw std::runtime_error(database.status().message());
    std::cout << "Database " << database->name() << " full metadata:\n"
              << database->DebugString();
    ++count;
  }
  if (count == 0) {
    std::cout << "No databases found in instance " << instance_id
              << " for project << " << project_id << "\n";
  }
}
//! [END spanner_list_databases] [list-databases]

void ListDatabasesCommand(std::vector<std::string> argv) {
  if (argv.size() != 2) {
    throw std::runtime_error("list-databases <project-id> <instance-id>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  ListDatabases(std::move(client), argv[0], argv[1]);
}

//! [create-backup] [START spanner_create_backup]
void CreateBackup(google::cloud::spanner_admin::DatabaseAdminClient client,
                  std::string const& project_id, std::string const& instance_id,
                  std::string const& database_id, std::string const& backup_id,
                  google::cloud::spanner::Timestamp expire_time,
                  google::cloud::spanner::Timestamp version_time) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  google::spanner::admin::database::v1::CreateBackupRequest request;
  request.set_parent(database.instance().FullName());
  request.set_backup_id(backup_id);
  request.mutable_backup()->set_database(database.FullName());
  *request.mutable_backup()->mutable_expire_time() =
      expire_time.get<google::protobuf::Timestamp>().value();
  *request.mutable_backup()->mutable_version_time() =
      version_time.get<google::protobuf::Timestamp>().value();
  auto backup = client.CreateBackup(request).get();
  if (!backup) throw std::runtime_error(backup.status().message());
  std::cout
      << "Backup " << backup->name() << " of " << backup->database()
      << " of size " << backup->size_bytes() << " bytes"
      << " was created at "
      << google::cloud::spanner::MakeTimestamp(backup->create_time()).value()
      << ".\n";
}
//! [create-backup] [END spanner_create_backup]

void CreateBackupCommand(std::vector<std::string> argv) {
  if (argv.size() != 4) {
    throw std::runtime_error(
        "create-backup <project-id> <instance-id> <database-id> <backup-id>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  auto now = DatabaseNow(MakeSampleClient(argv[0], argv[1], argv[2]));
  CreateBackup(std::move(client), argv[0], argv[1], argv[2], argv[3],
               TimestampAdd(now, absl::Hours(7)), now);
}

//! [restore-database] [START spanner_restore_backup]
void RestoreDatabase(google::cloud::spanner_admin::DatabaseAdminClient client,
                     std::string const& project_id,
                     std::string const& instance_id,
                     std::string const& database_id,
                     std::string const& backup_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  google::cloud::spanner::Backup backup(database.instance(), backup_id);
  auto restored_db =
      client
          .RestoreDatabase(database.instance().FullName(),
                           database.database_id(), backup.FullName())
          .get();
  if (!restored_db) {
    throw std::runtime_error(restored_db.status().message());
  }
  std::cout << "Database";
  if (restored_db->restore_info().source_type() ==
      google::spanner::admin::database::v1::BACKUP) {
    auto const& backup_info = restored_db->restore_info().backup_info();
    std::cout << " " << backup_info.source_database() << " as of "
              << google::cloud::spanner::MakeTimestamp(
                     backup_info.version_time())
                     .value();
  }
  std::cout << " restored to " << restored_db->name();
  std::cout << " from backup " << backup.FullName();
  std::cout << ".\n";
}
//! [restore-database] [END spanner_restore_backup]

void RestoreDatabaseCommand(std::vector<std::string> argv) {
  if (argv.size() != 4) {
    throw std::runtime_error(
        "restore-backup <project-id> <instance-id> <database-id> <backup-id>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  RestoreDatabase(std::move(client), argv[0], argv[1], argv[2], argv[3]);
}

//! [get-backup] [START spanner_get_backup]
void GetBackup(google::cloud::spanner_admin::DatabaseAdminClient client,
               std::string const& project_id, std::string const& instance_id,
               std::string const& backup_id) {
  google::cloud::spanner::Backup backup_name(
      google::cloud::spanner::Instance(project_id, instance_id), backup_id);
  auto backup = client.GetBackup(backup_name.FullName());
  if (!backup) throw std::runtime_error(backup.status().message());
  std::cout
      << "Backup " << backup->name() << " of size " << backup->size_bytes()
      << " bytes was created at "
      << google::cloud::spanner::MakeTimestamp(backup->create_time()).value()
      << ".\n";
}
//! [get-backup] [END spanner_get_backup]

void GetBackupCommand(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "get-backup <project-id> <instance-id> <backup-id>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  GetBackup(std::move(client), argv[0], argv[1], argv[2]);
}

//! [update-backup] [START spanner_update_backup]
void UpdateBackup(google::cloud::spanner_admin::DatabaseAdminClient client,
                  std::string const& project_id, std::string const& instance_id,
                  std::string const& backup_id,
                  google::cloud::spanner::Timestamp expire_time) {
  google::cloud::spanner::Backup backup_name(
      google::cloud::spanner::Instance(project_id, instance_id), backup_id);
  google::spanner::admin::database::v1::UpdateBackupRequest request;
  request.mutable_backup()->set_name(backup_name.FullName());
  *request.mutable_backup()->mutable_expire_time() =
      expire_time.get<google::protobuf::Timestamp>().value();
  request.mutable_update_mask()->add_paths("expire_time");
  auto backup = client.UpdateBackup(request);
  if (!backup) throw std::runtime_error(backup.status().message());
  std::cout
      << "Backup " << backup->name() << " updated to expire at "
      << google::cloud::spanner::MakeTimestamp(backup->expire_time()).value()
      << ".\n";
}
//! [update-backup] [END spanner_update_backup]

void UpdateBackupCommand(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "update-backup <project-id> <instance-id> <backup-id>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  auto now = DatabaseNow(MakeSampleClient(argv[0], argv[1], argv[2]));
  UpdateBackup(std::move(client), argv[0], argv[1], argv[2],
               TimestampAdd(now, absl::Hours(7)));
}

//! [delete-backup] [START spanner_delete_backup]
void DeleteBackup(google::cloud::spanner_admin::DatabaseAdminClient client,
                  std::string const& project_id, std::string const& instance_id,
                  std::string const& backup_id) {
  google::cloud::spanner::Backup backup(
      google::cloud::spanner::Instance(project_id, instance_id), backup_id);
  auto status = client.DeleteBackup(backup.FullName());
  if (!status.ok()) throw std::runtime_error(status.message());
  std::cout << "Backup " << backup.FullName() << " was deleted.\n";
}
//! [delete-backup] [END spanner_delete_backup]

void DeleteBackupCommand(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "delete-backup <project-id> <instance-id> <backup-id>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  DeleteBackup(std::move(client), argv[0], argv[1], argv[2]);
}

// [START spanner_cancel_backup_create]
void CreateBackupAndCancel(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id, std::string const& backup_id,
    google::cloud::spanner::Timestamp expire_time) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  google::spanner::admin::database::v1::CreateBackupRequest request;
  request.set_parent(database.instance().FullName());
  request.set_backup_id(backup_id);
  request.mutable_backup()->set_database(database.FullName());
  *request.mutable_backup()->mutable_expire_time() =
      expire_time.get<google::protobuf::Timestamp>().value();
  auto f = client.CreateBackup(request);
  f.cancel();
  auto backup = f.get();
  if (backup) {
    auto status = client.DeleteBackup(backup->name());
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Backup " << backup->name() << " was deleted.\n";
  } else {
    std::cout << "CreateBackup operation was cancelled with the message '"
              << backup.status().message() << "'.\n";
  }
}
// [END spanner_cancel_backup_create]

void CreateBackupAndCancelCommand(std::vector<std::string> argv) {
  if (argv.size() != 4) {
    throw std::runtime_error(
        "create-backup-and-cancel <project-id> <instance-id>"
        " <database-id> <backup-id>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  auto now = DatabaseNow(MakeSampleClient(argv[0], argv[1], argv[2]));
  CreateBackupAndCancel(std::move(client), argv[0], argv[1], argv[2], argv[3],
                        TimestampAdd(now, absl::Hours(7)));
}

//! [create-database-with-encryption-key]
// [START spanner_create_database_with_encryption_key]
void CreateDatabaseWithEncryptionKey(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id,
    google::cloud::KmsKeyName const& encryption_key) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  google::spanner::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent(database.instance().FullName());
  request.set_create_statement("CREATE DATABASE `" + database.database_id() +
                               "`");
  request.add_extra_statements(R"""(
      CREATE TABLE Singers (
          SingerId   INT64 NOT NULL,
          FirstName  STRING(1024),
          LastName   STRING(1024),
          SingerInfo BYTES(MAX)
      ) PRIMARY KEY (SingerId))""");
  request.add_extra_statements(R"""(
      CREATE TABLE Albums (
          SingerId     INT64 NOT NULL,
          AlbumId      INT64 NOT NULL,
          AlbumTitle   STRING(MAX)
      ) PRIMARY KEY (SingerId, AlbumId),
          INTERLEAVE IN PARENT Singers ON DELETE CASCADE)""");
  request.mutable_encryption_config()->set_kms_key_name(
      encryption_key.FullName());
  auto db = client.CreateDatabase(request).get();
  if (!db) throw std::runtime_error(db.status().message());
  std::cout << "Database " << db->name() << " created";
  std::cout << " using encryption key " << encryption_key.FullName();
  std::cout << ".\n";
}
// [END spanner_create_database_with_encryption_key]
//! [create-database-with-encryption-key]

void CreateDatabaseWithEncryptionKeyCommand(std::vector<std::string> argv) {
  if (argv.size() != 6) {
    throw std::runtime_error(
        "create-database-with-encryption-key <project-id> <instance-id>"
        " <database-id> <location> <key-ring> <key-name>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  google::cloud::KmsKeyName encryption_key(/*project_id=*/argv[0],
                                           /*location=*/argv[3],
                                           /*key_ring=*/argv[4],
                                           /*kms_key_name=*/argv[5]);
  CreateDatabaseWithEncryptionKey(std::move(client), argv[0], argv[1], argv[2],
                                  encryption_key);
}

// [create-backup-with-encryption-key]
// [START spanner_create_backup_with_encryption_key]
void CreateBackupWithEncryptionKey(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id, std::string const& backup_id,
    google::cloud::spanner::Timestamp expire_time,
    google::cloud::spanner::Timestamp version_time,
    google::cloud::KmsKeyName const& encryption_key) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  google::spanner::admin::database::v1::CreateBackupRequest request;
  request.set_parent(database.instance().FullName());
  request.set_backup_id(backup_id);
  request.mutable_backup()->set_database(database.FullName());
  *request.mutable_backup()->mutable_expire_time() =
      expire_time.get<google::protobuf::Timestamp>().value();
  *request.mutable_backup()->mutable_version_time() =
      version_time.get<google::protobuf::Timestamp>().value();
  request.mutable_encryption_config()->set_encryption_type(
      google::spanner::admin::database::v1::CreateBackupEncryptionConfig::
          CUSTOMER_MANAGED_ENCRYPTION);
  request.mutable_encryption_config()->set_kms_key_name(
      encryption_key.FullName());
  auto backup = client.CreateBackup(request).get();
  if (!backup) throw std::runtime_error(backup.status().message());
  std::cout
      << "Backup " << backup->name() << " of " << backup->database()
      << " of size " << backup->size_bytes() << " bytes"
      << " was created at "
      << google::cloud::spanner::MakeTimestamp(backup->create_time()).value()
      << " using encryption key " << encryption_key.FullName() << ".\n";
}
// [END spanner_create_backup_with_encryption_key]
//! [create-backup-with-encryption-key]

void CreateBackupWithEncryptionKeyCommand(std::vector<std::string> argv) {
  if (argv.size() != 7) {
    throw std::runtime_error(
        "create-backup-with-encryption-key <project-id> <instance-id>"
        " <database-id> <backup-id> <location> <key-ring> <key-name>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  auto now = DatabaseNow(MakeSampleClient(argv[0], argv[1], argv[2]));
  google::cloud::KmsKeyName encryption_key(/*project_id=*/argv[0],
                                           /*location=*/argv[4],
                                           /*key_ring=*/argv[5],
                                           /*kms_key_name=*/argv[6]);
  CreateBackupWithEncryptionKey(std::move(client), argv[0], argv[1], argv[2],
                                argv[3], TimestampAdd(now, absl::Hours(7)), now,
                                encryption_key);
}

// [restore-database-with-encryption-key]
// [START spanner_restore_backup_with_encryption_key]
void RestoreDatabaseWithEncryptionKey(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id, std::string const& backup_id,
    google::cloud::KmsKeyName const& encryption_key) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  google::cloud::spanner::Backup backup(database.instance(), backup_id);
  google::spanner::admin::database::v1::RestoreDatabaseRequest request;
  request.set_parent(database.instance().FullName());
  request.set_database_id(database.database_id());
  request.set_backup(backup.FullName());
  request.mutable_encryption_config()->set_encryption_type(
      google::spanner::admin::database::v1::RestoreDatabaseEncryptionConfig::
          CUSTOMER_MANAGED_ENCRYPTION);
  request.mutable_encryption_config()->set_kms_key_name(
      encryption_key.FullName());
  auto restored_db = client.RestoreDatabase(request).get();
  if (!restored_db) {
    throw std::runtime_error(restored_db.status().message());
  }
  std::cout << "Database";
  if (restored_db->restore_info().source_type() ==
      google::spanner::admin::database::v1::BACKUP) {
    auto const& backup_info = restored_db->restore_info().backup_info();
    std::cout << " " << backup_info.source_database() << " as of "
              << google::cloud::spanner::MakeTimestamp(
                     backup_info.version_time())
                     .value();
  }
  std::cout << " restored to " << restored_db->name();
  std::cout << " from backup " << backup.FullName();
  std::cout << " using encryption key " << encryption_key.FullName();
  std::cout << ".\n";
}
// [END spanner_restore_backup_with_encryption_key]
//! [restore-database-with-encryption-key]

void RestoreDatabaseWithEncryptionKeyCommand(std::vector<std::string> argv) {
  if (argv.size() != 7) {
    throw std::runtime_error(
        "restore-database-with-encryption-key <project-id> <instance-id>"
        " <database-id> <backup-id> <location> <key-ring> <key-name>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  google::cloud::KmsKeyName encryption_key(/*project_id=*/argv[0],
                                           /*location=*/argv[4],
                                           /*key_ring=*/argv[5],
                                           /*kms_key_name=*/argv[6]);
  RestoreDatabaseWithEncryptionKey(std::move(client), argv[0], argv[1], argv[2],
                                   argv[3], encryption_key);
}

//! [list-backups] [START spanner_list_backups]
void ListBackups(google::cloud::spanner_admin::DatabaseAdminClient client,
                 std::string const& project_id,
                 std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  std::cout << "All backups:\n";
  for (auto const& backup : client.ListBackups(in.FullName())) {
    if (!backup) throw std::runtime_error(backup.status().message());
    std::cout << "Backup " << backup->name() << " on database "
              << backup->database() << " with size : " << backup->size_bytes()
              << " bytes.\n";
  }
}
//! [list-backups] [END spanner_list_backups]

void ListBackupsCommand(std::vector<std::string> argv) {
  if (argv.size() != 2) {
    throw std::runtime_error("list-backups <project-id> <instance-id>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  ListBackups(std::move(client), argv[0], argv[1]);
}

//! [list-backup-operations] [START spanner_list_backup_operations]
void ListBackupOperations(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  google::cloud::spanner::Database database(in, database_id);
  google::spanner::admin::database::v1::ListBackupOperationsRequest request;
  request.set_parent(in.FullName());
  request.set_filter(std::string("(metadata.@type=type.googleapis.com/") +
                     "google.spanner.admin.database.v1.CreateBackupMetadata)" +
                     " AND (metadata.database=" + database.FullName() + ")");
  for (auto const& operation : client.ListBackupOperations(request)) {
    if (!operation) throw std::runtime_error(operation.status().message());
    google::spanner::admin::database::v1::CreateBackupMetadata metadata;
    operation->metadata().UnpackTo(&metadata);
    std::cout << "Backup " << metadata.name() << " on database "
              << metadata.database()
              << " progress: " << metadata.progress().progress_percent()
              << "% complete.\n";
  }
}
//! [list-backup-operations] [END spanner_list_backup_operations]

//! [list-database-operations] [START spanner_list_database_operations]
void ListDatabaseOperations(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  google::spanner::admin::database::v1::ListDatabaseOperationsRequest request;
  request.set_parent(in.FullName());
  request.set_filter(
      "(metadata.@type:type.googleapis.com/"
      "google.spanner.admin.database.v1.OptimizeRestoredDatabaseMetadata)");
  for (auto const& operation : client.ListDatabaseOperations(request)) {
    if (!operation) throw std::runtime_error(operation.status().message());
    google::spanner::admin::database::v1::OptimizeRestoredDatabaseMetadata
        metadata;
    operation->metadata().UnpackTo(&metadata);
    std::cout << "Database " << metadata.name() << " restored from backup is "
              << metadata.progress().progress_percent() << "% optimized.\n";
  }
}
//! [list-database-operations] [END spanner_list_database_operations]

void ListDatabaseOperationsCommand(std::vector<std::string> argv) {
  if (argv.size() != 2) {
    throw std::runtime_error(
        "list-database-operations <project-id> <instance-id>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  ListDatabaseOperations(std::move(client), argv[0], argv[1]);
}

//! [drop-database] [START spanner_drop_database]
void DropDatabase(google::cloud::spanner_admin::DatabaseAdminClient client,
                  std::string const& project_id, std::string const& instance_id,
                  std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto status = client.DropDatabase(database.FullName());
  if (!status.ok()) throw std::runtime_error(status.message());
  std::cout << "Database " << database << " successfully dropped\n";
}
//! [drop-database] [END spanner_drop_database]

//! [database-get-iam-policy]
void DatabaseGetIamPolicy(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto actual = client.GetIamPolicy(database.FullName());
  if (!actual) throw std::runtime_error(actual.status().message());

  std::cout << "The IAM policy for database " << database_id << " is:\n"
            << actual->DebugString() << "\n";
}
//! [database-get-iam-policy]

//! [add-database-reader-on-database]
void AddDatabaseReaderOnDatabase(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id, std::string const& new_reader) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto current = client.GetIamPolicy(database.FullName());
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
  auto result = client.SetIamPolicy(database.FullName(), *std::move(current));
  if (!result) throw std::runtime_error(result.status().message());

  std::cout << "Successfully added " << new_reader
            << " to the database reader role:\n"
            << result->DebugString();
}
//! [add-database-reader-on-database]

void AddDatabaseReaderOnDatabaseCommand(std::vector<std::string> argv) {
  if (argv.size() != 4) {
    throw std::runtime_error(
        "add-database-reader-on-database <project-id> <instance-id>"
        " <database-id> <new-reader>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  AddDatabaseReaderOnDatabase(std::move(client), argv[0], argv[1], argv[2],
                              argv[3]);
}

//! [database-test-iam-permissions]
void DatabaseTestIamPermissions(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id, std::string const& permission) {
  google::cloud::spanner::Database db(project_id, instance_id, database_id);
  auto actual = client.TestIamPermissions(db.FullName(), {permission});
  if (!actual) throw std::runtime_error(actual.status().message());

  char const* msg = actual->permissions().empty() ? "does not" : "does";

  std::cout << "The caller " << msg << " have permission '" << permission
            << "' on the Cloud Spanner database " << db.database_id() << "\n";
}
//! [database-test-iam-permissions]

void DatabaseTestIamPermissionsCommand(std::vector<std::string> argv) {
  if (argv.size() != 4) {
    throw std::runtime_error(
        "database-test-iam-permissions <project-id> <instance-id>"
        " <database-id> <permission>");
  }
  google::cloud::spanner_admin::DatabaseAdminClient client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
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

void QuickstartCommand(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "quickstart <project-id> <instance-id> <database-id>");
  }
  Quickstart(argv[0], argv[1], argv[2]);
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
void QueryWithTimestampParameter(
    google::cloud::spanner::Client client,
    google::cloud::spanner::Timestamp example_timestamp) {
  namespace spanner = ::google::cloud::spanner;
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

void QueryWithTimestampParameterCommand(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "query-with-timestamp-parameter <project-id> <instance-id>"
        " <database-id>");
  }
  auto client = MakeSampleClient(argv[0], argv[1], argv[2]);
  auto now = DatabaseNow(client);
  QueryWithTimestampParameter(client, now);
}

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
  std::cout << "make-update-mutation was successful\n";
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

  auto rows = client.ExecuteQuery(std::move(select));
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

// [START spanner_add_json_column]
void AddJsonColumn(google::cloud::spanner_admin::DatabaseAdminClient client,
                   std::string const& project_id,
                   std::string const& instance_id,
                   std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto metadata = client
                      .UpdateDatabaseDdl(database.FullName(), {R"""(
                        ALTER TABLE Venues ADD COLUMN VenueDetails JSON)"""})
                      .get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`Venues` table altered, new DDL:\n" << metadata->DebugString();
}
// [END spanner_add_json_column]

//! [START spanner_update_data_with_json_column]
void UpdateDataWithJson(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto venue19_details = spanner::Json(R"""(
        {"rating": 9, "open": true}
      )""");  // object
  auto venue4_details = spanner::Json(R"""(
        [
          {"name": "room 1", "open": true},
          {"name": "room 2", "open": false}
        ]
      )""");  // array
  auto venue42_details = spanner::Json(R"""(
        {
          "name": null,
          "open": {"Monday": true, "Tuesday": false},
          "tags": ["large", "airy"]
        }
      )""");  // nested
  auto update_venues =
      spanner::UpdateMutationBuilder(
          "Venues", {"VenueId", "VenueName", "VenueDetails", "LastUpdateTime"})
          .EmplaceRow(19, "Venue 19", venue19_details,
                      spanner::CommitTimestamp())
          .EmplaceRow(4, "Venue 4", venue4_details, spanner::CommitTimestamp())
          .EmplaceRow(42, "Venue 42", venue42_details,
                      spanner::CommitTimestamp())
          .Build();

  auto commit_result = client.Commit(spanner::Mutations{update_venues});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful [spanner_update_data_with_json_column]\n";
}
//! [END spanner_update_data_with_json_column]

// [START spanner_query_with_json_parameter]
void QueryWithJsonParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto rating9_details = spanner::Json(R"""(
        {"rating": 9}
      )""");  // object
  spanner::SqlStatement select(
      "SELECT VenueId, VenueDetails"
      "  FROM Venues"
      " WHERE JSON_VALUE(VenueDetails, '$.rating') ="
      "       JSON_VALUE(@details, '$.rating')",
      {{"details", spanner::Value(std::move(rating9_details))}});
  using RowType = std::tuple<std::int64_t, absl::optional<spanner::Json>>;

  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "VenueId: " << std::get<0>(*row) << ", ";
    auto venue_details = std::get<1>(*row).value();
    std::cout << "VenueDetails: " << venue_details << "\n";
  }
}
// [END spanner_query_with_json_parameter]

// [START spanner_add_numeric_column]
void AddNumericColumn(google::cloud::spanner_admin::DatabaseAdminClient client,
                      std::string const& project_id,
                      std::string const& instance_id,
                      std::string const& database_id) {
  google::cloud::spanner::Database database(project_id, instance_id,
                                            database_id);
  auto metadata = client
                      .UpdateDatabaseDdl(database.FullName(), {R"""(
                        ALTER TABLE Venues ADD COLUMN Revenue NUMERIC)"""})
                      .get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "`Venues` table altered, new DDL:\n" << metadata->DebugString();
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
  std::cout
      << "Insert was successful [spanner_update_data_with_numeric_column]\n";
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

  auto rows = client.ExecuteQuery(std::move(select));
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

//! [START spanner_set_transaction_tag]
void SetTransactionTag(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  using ::google::cloud::StatusOr;

  // Sets the transaction tag to "app=concert,env=dev". This will be
  // applied to all the individual operations inside this transaction.
  auto commit_options =
      google::cloud::Options{}.set<spanner::TransactionTagOption>(
          "app=concert,env=dev");
  auto commit = client.Commit(
      [&client](
          spanner::Transaction const& txn) -> StatusOr<spanner::Mutations> {
        spanner::SqlStatement update_statement(
            "UPDATE Venues SET Capacity = CAST(Capacity/4 AS INT64)"
            " WHERE OutdoorVenue = false");
        // Sets the request tag to "app=concert,env=dev,action=update".
        // This will only be set on this request.
        auto update = client.ExecuteDml(
            txn, std::move(update_statement),
            google::cloud::Options{}.set<spanner::RequestTagOption>(
                "app=concert,env=dev,action=update"));
        if (!update) return std::move(update).status();

        spanner::SqlStatement insert_statement(
            "INSERT INTO Venues (VenueId, VenueName, Capacity, OutdoorVenue, "
            "                    LastUpdateTime)"
            " VALUES (@venueId, @venueName, @capacity, @outdoorVenue, "
            "         PENDING_COMMIT_TIMESTAMP())",
            {
                {"venueId", spanner::Value(81)},
                {"venueName", spanner::Value("Venue 81")},
                {"capacity", spanner::Value(1440)},
                {"outdoorVenue", spanner::Value(true)},
            });
        // Sets the request tag to "app=concert,env=dev,action=insert".
        // This will only be set on this request.
        auto insert = client.ExecuteDml(
            txn, std::move(insert_statement),
            google::cloud::Options{}.set<spanner::RequestTagOption>(
                "app=concert,env=dev,action=select"));
        if (!insert) return std::move(insert).status();
        return spanner::Mutations{};
      },
      commit_options);
  if (!commit) throw std::runtime_error(commit.status().message());
}
//! [END spanner_set_transaction_tag]

//! [START spanner_read_stale_data]
void ReadStaleData(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto opts = spanner::Transaction::ReadOnlyOptions(std::chrono::seconds(15));
  auto read_only = spanner::MakeReadOnlyTransaction(std::move(opts));

  spanner::SqlStatement select(
      "SELECT SingerId, AlbumId, AlbumTitle FROM Albums");
  using RowType = std::tuple<std::int64_t, std::int64_t, std::string>;

  auto rows = client.ExecuteQuery(std::move(read_only), std::move(select));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row)
              << " AlbumId: " << std::get<1>(*row)
              << " AlbumTitle: " << std::get<2>(*row) << "\n";
  }
}
//! [END spanner_read_stale_data]

//! [START spanner_set_request_tag]
void SetRequestTag(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  spanner::SqlStatement select(
      "SELECT SingerId, AlbumId, AlbumTitle FROM Albums");
  using RowType = std::tuple<std::int64_t, std::int64_t, std::string>;

  auto opts = google::cloud::Options{}.set<spanner::RequestTagOption>(
      "app=concert,env=dev,action=select");
  auto rows = client.ExecuteQuery(std::move(select), std::move(opts));
  for (auto const& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row)
              << " AlbumId: " << std::get<1>(*row)
              << " AlbumTitle: " << std::get<2>(*row) << "\n";
  }
}
//! [END spanner_set_request_tag]

//! [START spanner_batch_client]
void UsePartitionQuery(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto txn = spanner::MakeReadOnlyTransaction();

  spanner::SqlStatement select(
      "SELECT SingerId, FirstName, LastName FROM Singers");
  using RowType = std::tuple<std::int64_t, std::string, std::string>;

  auto partitions =
      client.PartitionQuery(std::move(txn), std::move(select), {});
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
      std::cout << "MarketingBudget: " << *marketing_budget << "\n";
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
      std::cout << "MarketingBudget: " << *marketing_budget << "\n";
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
      google::cloud::Options{}
          .set<spanner::QueryOptimizerVersionOption>("1")
          .set<spanner::QueryOptimizerStatisticsPackageOption>(
              "auto_20191128_14_47_22UTC"));
  //! [END spanner_create_client_with_query_options]
}

void CreateClientWithQueryOptionsCommand(std::vector<std::string> argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "create-client-with-query-options <project-id> <instance-id>"
        " <database-id>");
  }
  CreateClientWithQueryOptions(argv[0], argv[1], argv[2]);
}

//! [START spanner_query_with_query_options]
void QueryWithQueryOptions(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  auto sql = spanner::SqlStatement("SELECT SingerId, FirstName FROM Singers");
  auto opts =
      google::cloud::Options{}
          .set<spanner::QueryOptimizerVersionOption>("1")
          .set<spanner::QueryOptimizerStatisticsPackageOption>("latest");
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
      std::cout << "MarketingBudget: " << *marketing_budget << "\n";
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

//! [START spanner_get_commit_stats]
void GetCommitStatistics(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  //! [commit-options]
  auto commit = client.Commit(
      spanner::Mutations{
          spanner::UpdateMutationBuilder(
              "Albums", {"SingerId", "AlbumId", "MarketingBudget"})
              .EmplaceRow(1, 1, 200000)
              .EmplaceRow(2, 2, 400000)
              .Build()},
      google::cloud::Options{}.set<spanner::CommitReturnStatsOption>(true));
  //! [commit-options]

  if (!commit) throw std::runtime_error(commit.status().message());
  if (commit->commit_stats) {
    std::cout << "Updated data with " << commit->commit_stats->mutation_count
              << " mutations.\n";
  }
  std::cout << "Update was successful [spanner_get_commit_stats]\n";
}
//! [END spanner_get_commit_stats]

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
        if (!insert) return std::move(insert).status();
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
        if (!update) return std::move(update).status();
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
        if (!update) return std::move(update).status();
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
        if (!update) return std::move(update).status();
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
        if (!update) return std::move(update).status();
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
        if (!insert) return std::move(insert).status();
        // Read newly inserted record.
        spanner::SqlStatement select(
            "SELECT FirstName, LastName FROM Singers where SingerId = 11");
        using RowType = std::tuple<std::string, std::string>;
        auto rows = client.ExecuteQuery(std::move(txn), std::move(select));
        for (auto const& row : spanner::StreamOf<RowType>(rows)) {
          if (!row) return std::move(row).status();
          std::cout << "FirstName: " << std::get<0>(*row) << "\t";
          std::cout << "LastName: " << std::get<1>(*row) << "\n";
        }
        return spanner::Mutations{};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Write then read succeeded [spanner_dml_write_then_read]\n";
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
    if (!dele) return std::move(dele).status();
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
        if (!result) return std::move(result).status();
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
        if (!dml_result) return std::move(dml_result).status();
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
        if (!insert) return std::move(insert).status();
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
    if (!row) return std::move(row).status();
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
        if (!budget1) return std::move(budget1).status();
        if (*budget1 < transfer_amount) {
          return google::cloud::Status(
              google::cloud::StatusCode::kUnknown,
              "cannot transfer " + std::to_string(transfer_amount) +
                  " from budget of " + std::to_string(*budget1));
        }
        auto budget2 = get_budget(txn, 2, 2);
        if (!budget2) return std::move(budget2).status();
        auto update = update_budget(txn, 1, 1, *budget1 - transfer_amount);
        if (!update) return std::move(update).status();
        update = update_budget(txn, 2, 2, *budget2 + transfer_amount);
        if (!update) return std::move(update).status();
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
    auto retry_policy =
        google::cloud::spanner_admin::InstanceAdminLimitedTimeRetryPolicy(
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
    auto client = google::cloud::spanner_admin::InstanceAdminClient(
        google::cloud::spanner_admin::MakeInstanceAdminConnection(),
        google::cloud::Options{}
            .set<google::cloud::spanner_admin::InstanceAdminRetryPolicyOption>(
                std::move(retry_policy))
            .set<
                google::cloud::spanner_admin::InstanceAdminBackoffPolicyOption>(
                std::move(backoff_policy))
            .set<
                google::cloud::spanner_admin::InstanceAdminPollingPolicyOption>(
                std::move(polling_policy)));

    // Use the client as usual.
    std::cout << "Available configs for project " << project_id << "\n";
    auto project = google::cloud::Project(project_id);
    for (auto const& cfg : client.ListInstanceConfigs(project.FullName())) {
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
    auto retry_policy =
        google::cloud::spanner_admin::DatabaseAdminLimitedTimeRetryPolicy(
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
    auto client = google::cloud::spanner_admin::DatabaseAdminClient(
        google::cloud::spanner_admin::MakeDatabaseAdminConnection(),
        google::cloud::Options{}
            .set<google::cloud::spanner_admin::DatabaseAdminRetryPolicyOption>(
                std::move(retry_policy))
            .set<
                google::cloud::spanner_admin::DatabaseAdminBackoffPolicyOption>(
                std::move(backoff_policy))
            .set<
                google::cloud::spanner_admin::DatabaseAdminPollingPolicyOption>(
                std::move(polling_policy)));

    // Use the client as usual.
    spanner::Instance instance(project_id, instance_id);
    std::cout << "Available databases for instance " << instance << "\n";
    for (auto const& db : client.ListDatabases(instance.FullName())) {
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

// [START spanner_query_information_schema_database_options]
void QueryInformationSchemaDatabaseOptions(
    google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;
  // clang-format misinterprets the namespace alias followed by a block as
  // introducing a sub-namespace and so adds a misleading namespace-closing
  // comment at the end. This separating comment defeats that.
  {
    auto rows = client.ExecuteQuery(spanner::SqlStatement(R"""(
        SELECT s.OPTION_NAME, s.OPTION_VALUE
        FROM INFORMATION_SCHEMA.DATABASE_OPTIONS s
        WHERE s.OPTION_NAME = 'default_leader'
      )"""));
    using RowType = std::tuple<std::string, std::string>;
    for (auto& row : spanner::StreamOf<RowType>(rows)) {
      if (!row) throw std::runtime_error(row.status().message());
      std::cout << std::get<0>(*row) << "=" << std::get<1>(*row) << "\n";
    }
  }
  {
    auto rows = client.ExecuteQuery(spanner::SqlStatement(R"""(
        SELECT s.OPTION_NAME, s.OPTION_VALUE
        FROM INFORMATION_SCHEMA.DATABASE_OPTIONS s
        WHERE s.OPTION_NAME = 'version_retention_period'
      )"""));
    using RowType = std::tuple<std::string, std::string>;
    for (auto& row : spanner::StreamOf<RowType>(rows)) {
      if (!row) throw std::runtime_error(row.status().message());
      std::cout << std::get<0>(*row) << "=" << std::get<1>(*row) << "\n";
    }
  }
}
// [END spanner_query_information_schema_database_options]

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
      void (*)(google::cloud::spanner_admin::DatabaseAdminClient,
               std::string const&, std::string const&, std::string const&);
  auto make_database_command_entry = [](std::string const& sample_name,
                                        DatabaseAdminSampleFunction sample) {
    auto make_command = [](std::string const& sample_name,
                           DatabaseAdminSampleFunction sample) {
      return [sample_name, sample](std::vector<std::string> const& argv) {
        if (argv.size() != 3) {
          throw std::runtime_error(sample_name +
                                   " <project-id> <instance-id> <database-id>");
        }
        google::cloud::spanner_admin::DatabaseAdminClient client(
            google::cloud::spanner_admin::MakeDatabaseAdminConnection());
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
      make_database_command_entry("create-database-with-retention-period",
                                  CreateDatabaseWithVersionRetentionPeriod),
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
      {"create-database-with-encryption-key",
       CreateDatabaseWithEncryptionKeyCommand},
      {"create-backup-with-encryption-key",
       CreateBackupWithEncryptionKeyCommand},
      {"restore-database-with-encryption-key",
       RestoreDatabaseWithEncryptionKeyCommand},
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
      {"query-with-timestamp-parameter", QueryWithTimestampParameterCommand},
      make_command_entry("insert-data-with-timestamp", InsertDataWithTimestamp),
      make_command_entry("update-data-with-timestamp", UpdateDataWithTimestamp),
      make_command_entry("query-data-with-timestamp", QueryDataWithTimestamp),
      make_database_command_entry("add-json-column", AddJsonColumn),
      make_command_entry("update-data-with-json", UpdateDataWithJson),
      make_command_entry("query-for-json", QueryWithJsonParameter),
      make_database_command_entry("add-numeric-column", AddNumericColumn),
      make_command_entry("update-data-with-numeric", UpdateDataWithNumeric),
      make_command_entry("query-with-numeric-parameter",
                         QueryWithNumericParameter),
      make_command_entry("read-only-transaction", ReadOnlyTransaction),
      make_command_entry("set-transaction-tag", SetTransactionTag),
      make_command_entry("read-stale-data", ReadStaleData),
      make_command_entry("set-request-tag", SetRequestTag),
      make_command_entry("use-partition-query", UsePartitionQuery),
      make_command_entry("read-data-with-index", ReadDataWithIndex),
      make_command_entry("query-new-column", QueryNewColumn),
      make_command_entry("query-data-with-index", QueryUsingIndex),
      make_command_entry("query-with-query-options", QueryWithQueryOptions),
      make_command_entry("read-data-with-storing-index",
                         ReadDataWithStoringIndex),
      make_command_entry("read-write-transaction", ReadWriteTransaction),
      make_command_entry("get-commit-stats", GetCommitStatistics),
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
      make_command_entry("query-information-schema-database-options",
                         QueryInformationSchemaDatabaseOptions),
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
  command->second(std::move(argv));
  return 0;
}

void SampleBanner(std::string const& name) {
  std::cout << "\nRunning " << name << " sample at "
            << absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", absl::Now(),
                                absl::UTCTimeZone())
            << std::endl;
  GCP_LOG(DEBUG) << "Running " << name << " sample";
}

std::string PickConfig(google::cloud::spanner_admin::InstanceAdminClient client,
                       std::string const& project_id,
                       google::cloud::internal::DefaultPRNG& generator) {
  // Skip non-US configs. They are too slow.
  std::string const included_prefix = "regional-us-";
  // Exclude the US configs where we keep most of our test instances.
  std::vector<std::string> const excluded{"regional-us-central1",
                                          "regional-us-east1"};

  auto project = google::cloud::Project(project_id);
  std::string const config_prefix = project.FullName() + "/instanceConfigs/";

  // Log the config names to aid in troubleshooting. This is only used during
  // an AutoRun() invocation, so it will not pollute the output for users.
  std::cout << "\n" << __func__ << ":\n";

  std::string ret;
  std::string last;
  int i = 0;
  for (auto const& instance_config :
       client.ListInstanceConfigs(project.FullName())) {
    if (!instance_config) break;
    if (instance_config->name().rfind(config_prefix, 0) != 0) continue;
    auto const config = instance_config->name().substr(config_prefix.size());
    last = config;
    if (config.rfind(included_prefix, 0) != 0) continue;
    if (std::find(excluded.begin(), excluded.end(), config) != excluded.end()) {
      continue;
    }
    // Use a reservoir sampler of size k==1 to pick a region at random.
    auto const selected =
        std::uniform_int_distribution<int>(1, ++i)(generator) == 1;
    if (selected) ret = config;
    std::cout << "    " << i << " " << config << " " << selected << " " << ret
              << "\n";
  }
  return ret.empty() ? last : ret;
}

std::vector<std::string> LeaderOptions(
    google::cloud::spanner_admin::InstanceAdminClient client,
    std::string const& project_id, std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  auto instance = client.GetInstance(in.FullName());
  if (!instance) throw std::runtime_error(instance.status().message());

  auto config = client.GetInstanceConfig(instance->config());
  if (!config) throw std::runtime_error(config.status().message());

  auto* leader_options = config->mutable_leader_options();
  return std::vector<std::string>(
      std::make_move_iterator(leader_options->begin()),
      std::make_move_iterator(leader_options->end()));
}

void RunAllSlowInstanceTests(
    google::cloud::spanner_admin::InstanceAdminClient& instance_admin_client,
    google::cloud::spanner_admin::DatabaseAdminClient& database_admin_client,
    google::cloud::internal::DefaultPRNG& generator,
    std::string const& project_id, std::string const& test_iam_service_account,
    bool const emulator) {
  auto const run_slow_integration_tests =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS")
          .value_or("");
  auto const run_slow_backup_tests =
      run_slow_integration_tests.find("backup") != std::string::npos;
  auto const run_slow_instance_tests =
      run_slow_integration_tests.find("instance") != std::string::npos;

  if (!run_slow_instance_tests) return;

  auto const crud_instance_id =
      google::cloud::spanner_testing::RandomInstanceName(generator);
  auto const database_id =
      google::cloud::spanner_testing::RandomDatabaseName(generator);

  auto const config = PickConfig(instance_admin_client, project_id, generator);
  if (config.empty()) throw std::runtime_error("Failed to pick a config");

  SampleBanner("create-instance");
  CreateInstance(instance_admin_client, project_id, crud_instance_id,
                 "Test Instance", config);

  if (!emulator) {
    SampleBanner("update-instance");
    UpdateInstance(instance_admin_client, project_id, crud_instance_id,
                   "New name");

    SampleBanner("add-database-reader");
    AddDatabaseReader(instance_admin_client, project_id, crud_instance_id,
                      "serviceAccount:" + test_iam_service_account);

    SampleBanner("remove-database-reader");
    RemoveDatabaseReader(instance_admin_client, project_id, crud_instance_id,
                         "serviceAccount:" + test_iam_service_account);

    auto const backup_id =
        google::cloud::spanner_testing::RandomBackupName(generator);
    auto const restore_database_id =
        google::cloud::spanner_testing::RandomDatabaseName(generator);

    if (run_slow_backup_tests) {  // with default encryption
      SampleBanner("spanner_create_database");
      CreateDatabase(database_admin_client, project_id, crud_instance_id,
                     database_id);

      SampleBanner("spanner_create_backup");
      auto now = DatabaseNow(
          MakeSampleClient(project_id, crud_instance_id, database_id));
      auto expire_time = TimestampAdd(now, absl::Hours(7));
      auto version_time = now;
      CreateBackup(database_admin_client, project_id, crud_instance_id,
                   database_id, backup_id, expire_time, version_time);

      SampleBanner("spanner_get_backup");
      GetBackup(database_admin_client, project_id, crud_instance_id, backup_id);

      SampleBanner("spanner_update_backup");
      expire_time = TimestampAdd(expire_time, absl::Hours(1));
      UpdateBackup(database_admin_client, project_id, crud_instance_id,
                   backup_id, expire_time);

      SampleBanner("spanner_restore_backup");
      RestoreDatabase(database_admin_client, project_id, crud_instance_id,
                      restore_database_id, backup_id);

      SampleBanner("spanner_drop_database");
      DropDatabase(database_admin_client, project_id, crud_instance_id,
                   restore_database_id);

      SampleBanner("spanner_delete_backup");
      DeleteBackup(database_admin_client, project_id, crud_instance_id,
                   backup_id);

      SampleBanner("spanner_cancel_backup_create");
      CreateBackupAndCancel(database_admin_client, project_id, crud_instance_id,
                            database_id, backup_id, expire_time);

      SampleBanner("spanner_list_backup_operations");
      ListBackupOperations(database_admin_client, project_id, crud_instance_id,
                           database_id);

      SampleBanner("spanner_list_backups");
      ListBackups(database_admin_client, project_id, crud_instance_id);

      SampleBanner("spanner_list_database_operations");
      ListDatabaseOperations(database_admin_client, project_id,
                             crud_instance_id);
    }

    if (run_slow_backup_tests) {  // with customer-managed encryption
      auto location = google::cloud::spanner_testing::InstanceLocation(
          google::cloud::spanner::Instance(project_id, crud_instance_id));
      if (!location) {
        throw std::runtime_error("Cannot find instance location: " +
                                 location.status().message());
      }
      google::cloud::KmsKeyName encryption_key(
          project_id, *location, "spanner-cmek", "spanner-cmek-test-key");

      SampleBanner("spanner_drop_database");
      DropDatabase(database_admin_client, project_id, crud_instance_id,
                   database_id);

      SampleBanner("spanner_create_database_with_encryption_key");
      CreateDatabaseWithEncryptionKey(database_admin_client, project_id,
                                      crud_instance_id, database_id,
                                      encryption_key);

      SampleBanner("spanner_create_backup_with_encryption_key");
      auto now = DatabaseNow(
          MakeSampleClient(project_id, crud_instance_id, database_id));
      auto expire_time = TimestampAdd(now, absl::Hours(7));
      auto version_time = now;
      CreateBackupWithEncryptionKey(database_admin_client, project_id,
                                    crud_instance_id, database_id, backup_id,
                                    expire_time, version_time, encryption_key);

      SampleBanner("spanner_restore_backup_with_encryption_key");
      RestoreDatabaseWithEncryptionKey(database_admin_client, project_id,
                                       crud_instance_id, restore_database_id,
                                       backup_id, encryption_key);

      SampleBanner("spanner_drop_database");
      DropDatabase(database_admin_client, project_id, crud_instance_id,
                   restore_database_id);

      SampleBanner("spanner_delete_backup");
      DeleteBackup(database_admin_client, project_id, crud_instance_id,
                   backup_id);
    }
  }

  SampleBanner("delete-instance");
  DeleteInstance(instance_admin_client, project_id, crud_instance_id);

  if (!emulator) {
    SampleBanner("spanner_create_instance_with_processing_units");
    CreateInstanceWithProcessingUnits(instance_admin_client, project_id,
                                      crud_instance_id,
                                      "Test Low-Cost Instance", config);

    SampleBanner("delete-instance");
    DeleteInstance(instance_admin_client, project_id, crud_instance_id);
  }
}

void RunAll(bool emulator) {
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  if (project_id.empty()) {
    throw std::runtime_error("GOOGLE_CLOUD_PROJECT is not set or is empty");
  }

  auto const test_iam_service_account =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT")
          .value_or("");
  if (!emulator && test_iam_service_account.empty()) {
    throw std::runtime_error(
        "GOOGLE_CLOUD_CPP_SPANNER_TEST_SERVICE_ACCOUNT is not set or is empty");
  }

  auto generator = google::cloud::internal::MakeDefaultPRNG();

  auto random_instance = google::cloud::spanner_testing::PickRandomInstance(
      generator, project_id,
      "labels.samples:yes AND NOT name:/instances/test-instance-mr-");
  if (!random_instance) {
    throw std::runtime_error("Cannot find an instance to run the samples: " +
                             random_instance.status().message());
  }
  std::string instance_id = *std::move(random_instance);
  std::cout << "Running samples on " << instance_id << std::endl;

  google::cloud::spanner_admin::InstanceAdminClient instance_admin_client(
      google::cloud::spanner_admin::MakeInstanceAdminConnection(),
      google::cloud::Options{}
          .set<google::cloud::spanner_admin::InstanceAdminRetryPolicyOption>(
              google::cloud::spanner_admin::InstanceAdminLimitedTimeRetryPolicy(
                  std::chrono::minutes(60))
                  .clone())
          .set<google::cloud::spanner_admin::InstanceAdminBackoffPolicyOption>(
              google::cloud::spanner::ExponentialBackoffPolicy(
                  std::chrono::seconds(1), std::chrono::minutes(1), 2.0)
                  .clone()));

  SampleBanner("get-instance");
  GetInstance(instance_admin_client, project_id, instance_id);

  SampleBanner("get-instance-config");
  GetInstanceConfig(instance_admin_client, project_id,
                    emulator ? "emulator-config" : "regional-us-central1");

  SampleBanner("list-instance-configs");
  ListInstanceConfigs(instance_admin_client, project_id);

  SampleBanner("list-instances");
  ListInstances(instance_admin_client, project_id);

  if (!emulator) {
    SampleBanner("instance-get-iam-policy");
    InstanceGetIamPolicy(instance_admin_client, project_id, instance_id);
  }

  std::string database_id =
      google::cloud::spanner_testing::RandomDatabaseName(generator);

  google::cloud::spanner_admin::DatabaseAdminClient database_admin_client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection(),
      google::cloud::Options{}
          .set<google::cloud::spanner_admin::DatabaseAdminRetryPolicyOption>(
              google::cloud::spanner_admin::DatabaseAdminLimitedTimeRetryPolicy(
                  std::chrono::minutes(60))
                  .clone())
          .set<google::cloud::spanner_admin::DatabaseAdminBackoffPolicyOption>(
              google::cloud::spanner::ExponentialBackoffPolicy(
                  std::chrono::seconds(1), std::chrono::minutes(1), 2.0)
                  .clone())
          .set<google::cloud::spanner_admin::DatabaseAdminPollingPolicyOption>(
              google::cloud::spanner::GenericPollingPolicy<>(
                  google::cloud::spanner::LimitedTimeRetryPolicy(
                      std::chrono::hours(2)),
                  google::cloud::spanner::ExponentialBackoffPolicy(
                      std::chrono::seconds(1), std::chrono::minutes(1), 2.0))
                  .clone()));

  RunAllSlowInstanceTests(instance_admin_client, database_admin_client,
                          generator, project_id, test_iam_service_account,
                          emulator);

  if (!emulator) {
    SampleBanner("instance-test-iam-permissions");
    InstanceTestIamPermissions(instance_admin_client, project_id, instance_id);
  }

  std::cout << "Running samples in database " << database_id << std::endl;

  SampleBanner("spanner_create_database");
  CreateDatabase(database_admin_client, project_id, instance_id, database_id);

  // TODO(#5479): Awaiting emulator support for version_retention_period.
  if (!emulator) {
    SampleBanner("spanner_drop_database");
    DropDatabase(database_admin_client, project_id, instance_id, database_id);

    SampleBanner("spanner_create_database_with_version_retention_period");
    CreateDatabaseWithVersionRetentionPeriod(database_admin_client, project_id,
                                             instance_id, database_id);
  }

  // TODO(#5048): Remove this check when the emulator supports CMEK.
  if (!emulator) {
    auto location = google::cloud::spanner_testing::InstanceLocation(
        google::cloud::spanner::Instance(project_id, instance_id));
    if (!location) {
      throw std::runtime_error("Cannot find instance location: " +
                               location.status().message());
    }
    google::cloud::KmsKeyName encryption_key(
        project_id, *location, "spanner-cmek", "spanner-cmek-test-key");

    SampleBanner("spanner_drop_database");
    DropDatabase(database_admin_client, project_id, instance_id, database_id);

    SampleBanner("spanner_create_database_with_encryption_key");
    CreateDatabaseWithEncryptionKey(database_admin_client, project_id,
                                    instance_id, database_id, encryption_key);
  }

  SampleBanner("spanner_create_table_with_datatypes");
  CreateTableWithDatatypes(database_admin_client, project_id, instance_id,
                           database_id);

  SampleBanner("spanner_create_table_with_timestamp_column");
  CreateTableWithTimestamp(database_admin_client, project_id, instance_id,
                           database_id);

  SampleBanner("spanner_create_index");
  AddIndex(database_admin_client, project_id, instance_id, database_id);

  SampleBanner("get-database");
  GetDatabase(database_admin_client, project_id, instance_id, database_id);

  SampleBanner("spanner_get_database_ddl");
  GetDatabaseDdl(database_admin_client, project_id, instance_id, database_id);

  SampleBanner("list-databases");
  ListDatabases(database_admin_client, project_id, instance_id);

  SampleBanner("spanner_add_column");
  AddColumn(database_admin_client, project_id, instance_id, database_id);

  SampleBanner("spanner_add_timestamp_column");
  AddTimestampColumn(database_admin_client, project_id, instance_id,
                     database_id);

  SampleBanner("spanner_create_storing_index");
  AddStoringIndex(database_admin_client, project_id, instance_id, database_id);

  if (!emulator) {
    SampleBanner("database-get-iam-policy");
    DatabaseGetIamPolicy(database_admin_client, project_id, instance_id,
                         database_id);

    SampleBanner("add-database-reader-on-database");
    AddDatabaseReaderOnDatabase(database_admin_client, project_id, instance_id,
                                database_id,
                                "serviceAccount:" + test_iam_service_account);

    SampleBanner("database-test-iam-permissions");
    DatabaseTestIamPermissions(database_admin_client, project_id, instance_id,
                               database_id, "spanner.databases.read");
  }

  // Call via RunOneCommand() for better code coverage.
  SampleBanner("spanner_quickstart");
  RunOneCommand({"", "quickstart", project_id, instance_id, database_id});

  SampleBanner("spanner_create_client_with_query_options");
  RunOneCommand({"", "create-client-with-query-options", project_id,
                 instance_id, database_id});

  auto client = MakeSampleClient(project_id, instance_id, database_id);

  SampleBanner("spanner_insert_data");
  InsertData(client);

  SampleBanner("spanner_update_data");
  UpdateData(client);

  SampleBanner("spanner_insert_datatypes_data");
  InsertDatatypesData(client);

  SampleBanner("spanner_query_with_array_parameter");
  QueryWithArrayParameter(client);

  SampleBanner("spanner_query_with_bool_parameter");
  QueryWithBoolParameter(client);

  SampleBanner("spanner_query_with_bytes_parameter");
  QueryWithBytesParameter(client);

  SampleBanner("spanner_query_with_date_parameter");
  QueryWithDateParameter(client);

  SampleBanner("spanner_query_with_float_parameter");
  QueryWithFloatParameter(client);

  SampleBanner("spanner_query_with_int_parameter");
  QueryWithIntParameter(client);

  SampleBanner("spanner_query_with_string_parameter");
  QueryWithStringParameter(client);

  SampleBanner("spanner_query_with_timestamp_parameter");
  QueryWithTimestampParameter(client, DatabaseNow(client));

  SampleBanner("spanner_insert_data_with_timestamp_column");
  InsertDataWithTimestamp(client);

  SampleBanner("spanner_update_data_with_timestamp_column");
  UpdateDataWithTimestamp(client);

  SampleBanner("spanner_query_data_with_timestamp_column");
  QueryDataWithTimestamp(client);

  // TODO(#6873): Remove this check when the emulator supports JSON.
  if (!emulator) {
    SampleBanner("spanner_add_json_column");
    AddJsonColumn(database_admin_client, project_id, instance_id, database_id);

    SampleBanner("spanner_update_data_with_json_column");
    UpdateDataWithJson(client);

    SampleBanner("spanner_query_with_json_parameter");
    QueryWithJsonParameter(client);
  }

  // TODO(#5024): Remove this check when the emulator supports NUMERIC.
  if (!emulator) {
    SampleBanner("spanner_add_numeric_column");
    AddNumericColumn(database_admin_client, project_id, instance_id,
                     database_id);

    SampleBanner("spanner_update_data_with_numeric_column");
    UpdateDataWithNumeric(client);

    SampleBanner("spanner_query_with_numeric_parameter");
    QueryWithNumericParameter(client);
  }

  SampleBanner("spanner_read_only_transaction");
  ReadOnlyTransaction(client);

  SampleBanner("spanner_set_transaction_tag");
  SetTransactionTag(client);

  SampleBanner("spanner_stale_data");
  ReadStaleData(client);

  SampleBanner("spanner_set_request_tag");
  SetRequestTag(client);

  if (!emulator) {
    SampleBanner("spanner_batch_client");
    UsePartitionQuery(client);
  }

  SampleBanner("spanner_read_data_with_index");
  ReadDataWithIndex(client);

  SampleBanner("spanner_query_data_with_new_column");
  QueryNewColumn(client);

  SampleBanner("spanner_profile_query");
  ProfileQuery(client);

  SampleBanner("spanner_query_data_with_index");
  QueryUsingIndex(client);

  SampleBanner("spanner_query_with_query_options");
  QueryWithQueryOptions(client);

  SampleBanner("spanner_read_data_with_storing_index");
  ReadDataWithStoringIndex(client);

  SampleBanner("spanner_read_write_transaction");
  ReadWriteTransaction(client);

  SampleBanner("spanner_get_commit_stats");
  GetCommitStatistics(client);

  SampleBanner("spanner_dml_standard_insert");
  DmlStandardInsert(client);

  SampleBanner("spanner_dml_standard_update");
  DmlStandardUpdate(client);

  SampleBanner("commit-with-policies");
  CommitWithPolicies(client);

  SampleBanner("spanner_dml_standard_update_with_timestamp");
  DmlStandardUpdateWithTimestamp(client);

  SampleBanner("profile_spanner_dml_standard_update");
  ProfileDmlStandardUpdate(client);

  SampleBanner("spanner_dml_write_then_read");
  DmlWriteThenRead(client);

  if (!emulator) {
    SampleBanner("spanner_dml_batch_update");
    DmlBatchUpdate(client);
  }

  SampleBanner("spanner_write_data_for_struct_queries");
  WriteDataForStructQueries(client);

  SampleBanner("spanner_query_data");
  QueryData(client);

  SampleBanner("spanner_dml_getting_started_insert");
  DmlGettingStartedInsert(client);

  SampleBanner("spanner_dml_getting_started_update");
  DmlGettingStartedUpdate(client);

  SampleBanner("spanner_query_with_parameter");
  QueryWithParameter(client);

  SampleBanner("spanner_read_data");
  ReadData(client);

  SampleBanner("spanner_query_data_select_star");
  QueryDataSelectStar(client);

  SampleBanner("spanner_query_data_with_struct");
  QueryDataWithStruct(client);

  SampleBanner("spanner_query_data_with_array_of_struct");
  QueryDataWithArrayOfStruct(client);

  SampleBanner("spanner_field_access_on_struct_parameters");
  FieldAccessOnStructParameters(client);

  SampleBanner("spanner_field_access_on_nested_struct");
  FieldAccessOnNestedStruct(client);

  if (!emulator) {
    SampleBanner("spanner_partition_read");
    PartitionRead(client);

    SampleBanner("spanner_partition_query");
    PartitionQuery(client);
  }

  SampleBanner("example-status-or");
  ExampleStatusOr(client);

  SampleBanner("get-singular-row");
  GetSingularRow(client);

  SampleBanner("stream-of");
  StreamOf(client);

  SampleBanner("custom-retry-policy");
  RunOneCommand(
      {"", "custom-retry-policy", project_id, instance_id, database_id});

  SampleBanner("custom-instance-admin-policies");
  RunOneCommand({"", "custom-instance-admin-policies", project_id});

  SampleBanner("custom-database-admin-policies");
  RunOneCommand(
      {"", "custom-database-admin-policies", project_id, instance_id});

  if (!emulator) {
    SampleBanner("spanner_dml_partitioned_update");
    DmlPartitionedUpdate(client);

    SampleBanner("spanner_dml_partitioned_delete");
    DmlPartitionedDelete(client);
  }

  SampleBanner("spanner_dml_structs");
  DmlStructs(client);

  SampleBanner("spanner_dml_standard_delete");
  DmlStandardDelete(client);

  SampleBanner("spanner_delete_data");
  DeleteData(client);

  std::cout << "\nDeleting all data to run the mutation examples" << std::endl;
  DeleteAll(client);

  SampleBanner("insert-mutation-builder");
  InsertMutationBuilder(client);

  SampleBanner("make-insert-mutation");
  MakeInsertMutation(client);

  SampleBanner("update-mutation-builder");
  UpdateMutationBuilder(client);

  SampleBanner("make-update-mutation");
  MakeUpdateMutation(client);

  SampleBanner("insert-or-update-mutation-builder");
  InsertOrUpdateMutationBuilder(client);

  SampleBanner("make-insert-or-update-mutation");
  MakeInsertOrUpdateMutation(client);

  SampleBanner("replace-mutation-builder");
  ReplaceMutationBuilder(client);

  SampleBanner("make-replace-mutation");
  MakeReplaceMutation(client);

  SampleBanner("delete-mutation-builder");
  DeleteMutationBuilder(client);

  SampleBanner("make-delete-mutation");
  MakeDeleteMutation(client);

  if (!emulator) {
    SampleBanner("spanner_query_information_schema_database_options");
    QueryInformationSchemaDatabaseOptions(client);
  }

  SampleBanner("spanner_drop_database");
  DeleteAll(client);
  DropDatabase(database_admin_client, project_id, instance_id, database_id);

  // TODO(#7144): Awaiting emulator support for default_leader.
  if (!emulator) {
    auto random_instance = google::cloud::spanner_testing::PickRandomInstance(
        generator, project_id,
        "labels.samples:yes AND name:/instances/test-instance-mr-");
    if (!random_instance) {
      throw std::runtime_error(
          "Cannot find an instance to run the multi-region samples: " +
          random_instance.status().message());
    }
    std::string instance_id = *std::move(random_instance);
    std::cout << "\nRunning multi-region samples on " << instance_id
              << std::endl;

    auto leader_options =
        LeaderOptions(instance_admin_client, project_id, instance_id);
    if (leader_options.size() < 2) {
      throw std::runtime_error("Did not find at least two leader options in " +
                               instance_id);
    }

    SampleBanner("spanner_create_database_with_default_leader");
    CreateDatabaseWithDefaultLeader(database_admin_client, project_id,
                                    instance_id, database_id,
                                    leader_options[1]);

    SampleBanner("spanner_update_database_with_default_leader");
    UpdateDatabaseWithDefaultLeader(database_admin_client, project_id,
                                    instance_id, database_id,
                                    leader_options[0]);

    SampleBanner("spanner_drop_database");
    DropDatabase(database_admin_client, project_id, instance_id, database_id);
  }
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
  google::cloud::LogSink::Instance().Flush();
  return 1;
}
