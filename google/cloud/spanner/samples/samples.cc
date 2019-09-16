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
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include <sstream>
#include <tuple>
#include <utility>

namespace {

std::string RandomDatabaseName(
    google::cloud::internal::DefaultPRNG& generator) {
  // A database ID must be between 2 and 30 characters, fitting the regular
  // expression `[a-z][a-z0-9_\-]*[a-z0-9]`
  int max_size = 30;
  std::string const prefix = "db-";
  auto size = static_cast<int>(max_size - 1 - prefix.size());
  return prefix +
         google::cloud::internal::Sample(
             generator, size, "abcdefghijlkmnopqrstuvwxyz012345689_-") +
         google::cloud::internal::Sample(generator, 1,
                                         "abcdefghijlkmnopqrstuvwxyz");
}

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

//! [list-instances]
void ListInstances(google::cloud::spanner::InstanceAdminClient client,
                   std::string const& project_id) {
  int count = 0;
  for (auto instance : client.ListInstances(project_id, "")) {
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
    throw std::runtime_error("get-instance <project-id>");
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

void CreateDatabase(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "create-database <project-id> <instance-id> <database-id>");
  }

  //! [create-database] [START spanner_create_database]
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& database_id) {
    google::cloud::spanner::DatabaseAdminClient client;
    google::cloud::spanner::Database database(project_id, instance_id,
                                              database_id);
    future<StatusOr<google::spanner::admin::database::v1::Database>> future =
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
    StatusOr<google::spanner::admin::database::v1::Database> db = future.get();
    if (!db) {
      throw std::runtime_error(db.status().message());
    }
    std::cout << "Created database [" << database << "]\n";
  }
  //! [create-database] [END spanner_create_database]
  (argv[0], argv[1], argv[2]);
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

void GetDatabaseCommandDdl(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "get-database-ddl <project-id> <instance-id> <database-id>");
  }
  google::cloud::spanner::DatabaseAdminClient client;
  GetDatabaseDdl(std::move(client), argv[0], argv[1], argv[2]);
}

void AddColumn(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "add-column <project-id> <instance-id> <database-id>");
  }

  //! [update-database] [START spanner_add_column]
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& database_id) {
    google::cloud::spanner::DatabaseAdminClient client;
    google::cloud::spanner::Database database(project_id, instance_id,
                                              database_id);
    future<StatusOr<
        google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>>
        future = client.UpdateDatabase(
            database, {"ALTER TABLE Albums ADD COLUMN MarketingBudget INT64"});
    StatusOr<google::spanner::admin::database::v1::UpdateDatabaseDdlMetadata>
        metadata = future.get();
    if (!metadata) {
      throw std::runtime_error(metadata.status().message());
    }
    std::cout << "Added MarketingBudget column\n";
  }
  //! [update-database] [END spanner_add_column]
  (argv[0], argv[1], argv[2]);
}

//! [list-databases]
void ListDatabases(google::cloud::spanner::DatabaseAdminClient client,
                   std::string const& project_id,
                   std::string const& instance_id) {
  google::cloud::spanner::Instance in(project_id, instance_id);
  int count = 0;
  for (auto database : client.ListDatabases(in)) {
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

void DropDatabase(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "drop-database <project-id> <instance-id> <database-id>");
  }

  //! [drop-database] [START spanner_drop_database]
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& database_id) {
    google::cloud::spanner::DatabaseAdminClient client;
    google::cloud::spanner::Database database(project_id, instance_id,
                                              database_id);
    google::cloud::Status status = client.DropDatabase(database);
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "Database " << database << " successfully dropped\n";
  }
  //! [drop-database] [END spanner_drop_database]
  (argv[0], argv[1], argv[2]);
}

//! [quickstart] [START spanner_quickstart]
void Quickstart(std::string const& project_id, std::string const& instance_id,
                std::string const& database_id) {
  namespace spanner = google::cloud::spanner;

  spanner::Client client(spanner::MakeConnection(
      spanner::Database(project_id, instance_id, database_id)));

  auto reader =
      client.ExecuteSql(spanner::SqlStatement("SELECT 'Hello World'"));
  if (!reader) throw std::runtime_error(reader.status().message());

  using RowType = spanner::Row<std::string>;
  for (auto row : reader->Rows<RowType>()) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << row->get<0>() << "\n";
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
      client.Commit(spanner::MakeReadWriteTransaction(),
                    {std::move(insert_singers), std::move(insert_albums)});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful [spanner_insert_data]\n";
}
//! [END spanner_insert_data]

//! [START spanner_update_data]
void UpdateData(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  auto commit_result =
      client.Commit(spanner::MakeReadWriteTransaction(),
                    {spanner::UpdateMutationBuilder(
                         "Albums", {"SingerId", "AlbumId", "MarketingBudget"})
                         .EmplaceRow(1, 1, 100000)
                         .EmplaceRow(2, 2, 500000)
                         .Build()});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Update was successful [spanner_update_data]\n";
}
//! [END spanner_update_data]

//! [START spanner_delete_data]
void DeleteData(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;

  // Delete each of the albums by individual key, then delete all the singers
  // using a key range.
  auto delete_albums =
      spanner::DeleteMutationBuilder(
          "Albums",
          spanner::KeySetBuilder<spanner::Row<std::int64_t, std::int64_t>>()
              .Add(spanner::MakeRow(1, 1))
              .Add(spanner::MakeRow(1, 2))
              .Add(spanner::MakeRow(2, 1))
              .Add(spanner::MakeRow(2, 2))
              .Add(spanner::MakeRow(2, 3))
              .Build())
          .Build();
  using SingersKey = spanner::Row<std::int64_t>;
  auto delete_singers =
      spanner::DeleteMutationBuilder(
          "Singers", spanner::KeySetBuilder<SingersKey>(
                         MakeKeyRangeClosed(SingersKey(1), SingersKey(5)))
                         .Build())
          .Build();

  auto commit_result = client.Commit(spanner::MakeReadWriteTransaction(),
                                     {delete_albums, delete_singers});
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
  using RowType = spanner::Row<std::int64_t, std::int64_t, std::string>;

  // Read#1.
  auto read1 = client.ExecuteSql(read_only, select);
  std::cout << "Read 1 results\n";
  for (auto row : read1->Rows<RowType>()) {
    if (!row) {
      throw std::runtime_error(row.status().message());
    }
    std::cout << "SingerId: " << row->get<0>() << " AlbumId: " << row->get<1>()
              << " AlbumTitle: " << row->get<2>() << "\n";
  }
  // Read#2. Even if changes occur in-between the reads the transaction ensures
  // that Read #1 and Read #2 return the same data.
  auto read2 = client.ExecuteSql(read_only, select);
  std::cout << "Read 2 results\n";
  for (auto row : read2->Rows<RowType>()) {
    if (!row) {
      throw std::runtime_error(row.status().message());
    }
    std::cout << "SingerId: " << row->get<0>() << " AlbumId: " << row->get<1>()
              << " AlbumTitle: " << row->get<2>() << "\n";
  }
}
//! [END spanner_read_only_transaction]

//! [START spanner_read_write_transaction]
void ReadWriteTransaction(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;

  // A helper to read a single album MarketingBudget.
  auto get_current_budget = [](spanner::Client client, spanner::Transaction txn,
                               std::int64_t singer_id, std::int64_t album_id) {
    auto key = spanner::KeySetBuilder<spanner::Row<std::int64_t, std::int64_t>>(
                   spanner::MakeRow(singer_id, album_id))
                   .Build();
    auto read = client.Read(std::move(txn), "Albums", std::move(key),
                            {"MarketingBudget"});
    if (!read) throw std::runtime_error(read.status().message());
    for (auto row : read->Rows<spanner::Row<std::int64_t>>()) {
      if (!row) throw std::runtime_error(read.status().message());
      // We expect at most one result from the `Read()` request. Return
      // the first one.
      return row->get<0>();
    }
    throw std::runtime_error("Key not found (" + std::to_string(singer_id) +
                             "," + std::to_string(album_id) + ")");
  };

  auto commit = spanner::RunTransaction(
      std::move(client), {},
      [&get_current_budget](spanner::Client const& client,
                            spanner::Transaction const& txn)
          -> google::cloud::StatusOr<spanner::Mutations> {
        auto b1 = get_current_budget(client, txn, 1, 1);
        auto b2 = get_current_budget(client, txn, 2, 2);
        std::int64_t transfer_amount = 200000;

        return spanner::Mutations{
            spanner::UpdateMutationBuilder(
                "Albums", {"SingerId", "AlbumId", "MarketingBudget"})
                .EmplaceRow(1, 1, b1 + transfer_amount)
                .EmplaceRow(2, 2, b2 - transfer_amount)
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
  auto commit_result = spanner::RunTransaction(
      std::move(client), spanner::Transaction::ReadWriteOptions{},
      [](spanner::Client client,
         spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto insert = client.ExecuteSql(
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
  auto commit_result = spanner::RunTransaction(
      std::move(client), spanner::Transaction::ReadWriteOptions{},
      [](spanner::Client client,
         spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto update = client.ExecuteSql(
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
  auto commit_result = spanner::RunTransaction(
      std::move(client), spanner::Transaction::ReadWriteOptions{},
      [](spanner::Client client,
         spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto dele = client.ExecuteSql(
            std::move(txn),
            spanner::SqlStatement(
                "DELETE FROM Singers WHERE FirstName = 'Alice'"));
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
  auto commit_result =
      client.Commit(spanner::MakeReadWriteTransaction(),
                    {spanner::InsertMutationBuilder(
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

//! [START spanner_query_data_with_struct]
void QueryDataWithStruct(google::cloud::spanner::Client client) {
  //! [spanner-sql-statement-params] [START spanner_create_struct_with_data]
  namespace spanner = google::cloud::spanner;
  auto singer_info = std::make_tuple("Elena", "Campbell");
  //! [END spanner_create_struct_with_data]
  auto reader = client.ExecuteSql(spanner::SqlStatement(
      "SELECT SingerId FROM Singers WHERE (FirstName, LastName) = @name",
      {{"name", spanner::Value(singer_info)}}));
  //! [spanner-sql-statement-params]
  if (!reader) throw std::runtime_error(reader.status().message());

  for (auto row : reader->Rows<spanner::Row<std::int64_t>>()) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << row->get<0>() << "\n";
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

  auto reader = client.ExecuteSql(spanner::SqlStatement(
      "SELECT SingerId FROM Singers"
      " WHERE STRUCT<FirstName STRING, LastName STRING>(FirstName, LastName)"
      "    IN UNNEST(@names)",
      {{"names", spanner::Value(singer_info)}}));
  if (!reader) throw std::runtime_error(reader.status().message());

  for (auto row : reader->Rows<spanner::Row<std::int64_t>>()) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << row->get<0>() << "\n";
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

  auto reader = client.ExecuteSql(spanner::SqlStatement(
      "SELECT SingerId FROM Singers WHERE FirstName = @name.FirstName",
      {{"name", spanner::Value(name)}}));
  if (!reader) throw std::runtime_error(reader.status().message());

  for (auto row : reader->Rows<spanner::Row<std::int64_t>>()) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << row->get<0>() << "\n";
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

  auto reader = client.ExecuteSql(spanner::SqlStatement(
      "SELECT SingerId, @songinfo.SongName FROM Singers"
      " WHERE STRUCT<FirstName STRING, LastName STRING>(FirstName, LastName)"
      "    IN UNNEST(@songinfo.ArtistNames)",
      {{"songinfo", spanner::Value(songinfo)}}));
  if (!reader) throw std::runtime_error(reader.status().message());

  using RowType = spanner::Row<std::int64_t, std::string>;
  for (auto row : reader->Rows<RowType>()) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << row->get<0>() << " SongName: " << row->get<1>()
              << "\n";
  }
  std::cout << "Query completed for [spanner_field_access_on_nested_struct]\n";
}
//! [END spanner_field_access_on_nested_struct]

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

void ProcessRow(google::cloud::spanner::Row<std::int64_t, std::string,
                                            std::string> const&) {}

void PartitionRead(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  RemoteConnectionFake remote_connection;
  //! [key-set-builder]
  auto key_set = spanner::KeySetBuilder<spanner::Row<int64_t>>()
                     .Add(spanner::MakeKeyRangeClosed(spanner::MakeRow(1),
                                                      spanner::MakeRow(10)))
                     .Build();
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
  auto result_set = client.Read(*partition);
  if (!result_set) throw std::runtime_error(result_set.status().message());
  using RowType = spanner::Row<std::int64_t, std::string, std::string>;
  for (auto& row : result_set->Rows<RowType>()) {
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
  auto result_set = client.ExecuteSql(*partition);
  if (!result_set) throw std::runtime_error(result_set.status().message());
  using RowType = spanner::Row<std::int64_t, std::string, std::string>;
  for (auto& row : result_set->Rows<RowType>()) {
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
      {"list-instances", &ListInstancesCommand},
      {"instance-get-iam-policy", &InstanceGetIamPolicyCommand},
      {"add-database-reader", &AddDatabaseReaderCommand},
      {"remove-database-reader", &RemoveDatabaseReaderCommand},
      {"instance-test-iam-permissions", &InstanceTestIamPermissionsCommand},
      {"create-database", &CreateDatabase},
      {"get-database", &GetDatabaseCommand},
      {"get-database-ddl", &GetDatabaseCommandDdl},
      {"add-column", &AddColumn},
      {"list-databases", &ListDatabasesCommand},
      {"drop-database", &DropDatabase},
      {"quickstart", &QuickstartCommand},
      make_command_entry("insert-data", &InsertData),
      make_command_entry("update-data", &UpdateData),
      make_command_entry("delete-data", &DeleteData),
      make_command_entry("read-only-transaction", &ReadOnlyTransaction),
      make_command_entry("read-write-transaction", &ReadWriteTransaction),
      make_command_entry("dml-standard-insert", &DmlStandardInsert),
      make_command_entry("dml-standard-update", &DmlStandardUpdate),
      make_command_entry("dml-standard-delete", &DmlStandardDelete),
      make_command_entry("dml-partitioned-update", &DmlPartitionedUpdate),
      make_command_entry("dml-partitioned-delete", &DmlPartitionedDelete),
      make_command_entry("write-data-for-struct-queries",
                         &WriteDataForStructQueries),
      make_command_entry("query-data-with-struct", &QueryDataWithStruct),
      make_command_entry("query-data-with-array-of-struct",
                         &QueryDataWithArrayOfStruct),
      make_command_entry("field-access-struct-parameters",
                         &FieldAccessOnStructParameters),
      make_command_entry("field-access-on-nested-struct",
                         &FieldAccessOnNestedStruct),
      make_command_entry("partition-read", &PartitionRead),
      make_command_entry("partition-query", &PartitionQuery)

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

  std::cout << "\nRunning get-instance sample\n";
  RunOneCommand({"", "get-instance", project_id, instance_id});

  std::cout << "\nRunning list-instances sample\n";
  RunOneCommand({"", "list-instances", project_id});

  std::cout << "\nRunning (instance) get-iam-policy sample\n";
  RunOneCommand({"", "instance-get-iam-policy", project_id, instance_id});

  std::cout << "\nRunning (instance) add-database-reader sample\n";
  RunOneCommand({"", "add-database-reader", project_id, instance_id,
                 "serviceAccount:" + test_iam_service_account});

  std::cout << "\nRunning (instance) remove-database-reader sample\n";
  RunOneCommand({"", "remove-database-reader", project_id, instance_id,
                 "serviceAccount:" + test_iam_service_account});

  std::cout << "\nRunning (instance) test-iam-permissions sample\n";
  RunOneCommand({"", "instance-test-iam-permissions", project_id, instance_id});

  std::string database_id = RandomDatabaseName(generator);

  std::cout << "Running samples in database " << database_id << "\n";

  std::cout << "\nRunning spanner_create_database sample\n";
  RunOneCommand({"", "create-database", project_id, instance_id, database_id});

  std::cout << "\nRunning spanner get-database sample\n";
  RunOneCommand({"", "get-database", project_id, instance_id, database_id});

  std::cout << "\nRunning spanner get-database-ddl sample\n";
  RunOneCommand({"", "get-database-ddl", project_id, instance_id, database_id});

  std::cout << "\nList all databases\n";
  RunOneCommand({"", "list-databases", project_id, instance_id});

  std::cout << "\nRunning spanner_add_column sample\n";
  RunOneCommand({"", "add-column", project_id, instance_id, database_id});

  std::cout << "\nRunning spanner_quickstart sample\n";
  RunOneCommand({"", "quickstart", project_id, instance_id, database_id});

  auto client = MakeSampleClient(project_id, instance_id, database_id);

  std::cout << "\nRunning spanner_insert_data sample\n";
  InsertData(client);

  std::cout << "\nRunning spanner_update_data sample\n";
  UpdateData(client);

  std::cout << "\nRunning spanner_read_only_transaction sample\n";
  ReadOnlyTransaction(client);

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

  std::cout << "\nRunning spanner_dml_partitioned_update sample\n";
  DmlPartitionedUpdate(client);

  std::cout << "\nRunning spanner_dml_partitioned_delete sample\n";
  DmlPartitionedDelete(client);

  std::cout << "\nRunning spanner_drop_database sample\n";
  RunOneCommand({"", "drop-database", project_id, instance_id, database_id});
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
