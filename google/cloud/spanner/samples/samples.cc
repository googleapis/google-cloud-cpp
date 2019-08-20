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

#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database_admin_client.h"
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

void QueryWithStructCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "query-with-struct <project-id> <instance-id> <database-id>");
  }
  // TODO(#188): Add querying part once data client is ready.
  // [START spanner_create_struct_with_data]
  auto singer_info = std::make_tuple(std::make_pair("FirstName", "Elena"),
                                     std::make_pair("LastName", "Campbell"));
  // [END spanner_create_struct_with_data]
  std::cout << "Struct created with the following data:\n"
            << std::get<0>(singer_info).first << ":"
            << std::get<0>(singer_info).second << "\n"
            << std::get<1>(singer_info).first << ":"
            << std::get<1>(singer_info).second << "\n";
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

void InsertDataCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "insert-data <project-id> <instance-id> <database-id>");
  }

  InsertData(MakeSampleClient(argv[0], argv[1], argv[2]));
}

//! [START spanner_update_data]
void UpdateData(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  auto update_albums = spanner::UpdateMutationBuilder(
      "Albums", {"SingerId", "AlbumId", "MarketingBudget"});

  auto txn = spanner::MakeReadWriteTransaction();

  auto read = client.ExecuteSql(
      txn, spanner::SqlStatement("SELECT SingerId, AlbumId FROM Albums"));
  if (!read) {
    throw std::runtime_error(read.status().message());
  }
  for (auto row : read->Rows<std::int64_t, std::int64_t>()) {
    if (!row) {
      throw std::runtime_error(row.status().message());
    }
    if (row->get<0>() == 1 && row->get<1>() == 1) {
      update_albums.EmplaceRow(1, 1, 100000);
    }
    if (row->get<0>() == 2 && row->get<1>() == 2) {
      update_albums.EmplaceRow(2, 2, 500000);
    }
  }
  auto commit_result = client.Commit(txn, {update_albums.Build()});
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Update was successful [spanner_update_data]\n";
}
//! [END spanner_update_data]

void UpdateDataCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "update-data <project-id> <instance-id> <database-id>");
  }

  UpdateData(MakeSampleClient(argv[0], argv[1], argv[2]));
}

//! [START spanner_read_only_transaction]
void ReadOnlyTransaction(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  auto read_only = spanner::MakeReadOnlyTransaction();

  spanner::SqlStatement select(
      "SELECT SingerId, AlbumId, AlbumTitle FROM Albums");

  // Read#1.
  auto read1 = client.ExecuteSql(read_only, select);
  std::cout << "Read 1 results\n";
  for (auto row : read1->Rows<std::int64_t, std::int64_t, std::string>()) {
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
  for (auto row : read2->Rows<std::int64_t, std::int64_t, std::string>()) {
    if (!row) {
      throw std::runtime_error(row.status().message());
    }
    std::cout << "SingerId: " << row->get<0>() << " AlbumId: " << row->get<1>()
              << " AlbumTitle: " << row->get<2>() << "\n";
  }
}
//! [END spanner_read_only_transaction]

void ReadOnlyTransactionCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "read-only-transaction <project-id> <instance-id> <database-id>");
  }

  ReadOnlyTransaction(MakeSampleClient(argv[0], argv[1], argv[2]));
}

//! [START spanner_read_write_transaction]
void ReadWriteTransaction(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  auto txn = spanner::MakeReadWriteTransaction();

  auto get_current_budget = [&txn, &client](std::int64_t singer_id,
                                            std::int64_t album_id) {
    auto key = spanner::KeySetBuilder<spanner::Row<std::int64_t, std::int64_t>>(
                   spanner::MakeRow(singer_id, album_id))
                   .Build();
    auto read = client.Read(txn, "Albums", std::move(key), {"MarketingBudget"});
    if (!read) {
      throw std::runtime_error(read.status().message());
    }
    for (auto row : read->Rows<std::int64_t>()) {
      if (!row) {
        throw std::runtime_error(read.status().message());
      }
      // We expect at most one result from the `Read()` request. Return the
      // first one.
      return row->get<0>();
    }
    throw std::runtime_error("Key not found (" + std::to_string(singer_id) +
                             "," + std::to_string(album_id) + ")");
  };

  auto b1 = get_current_budget(1, 1);
  auto b2 = get_current_budget(2, 2);
  std::int64_t transfer_amount = 200000;

  auto commit_result = client.Commit(
      txn, {spanner::UpdateMutationBuilder(
                "Albums", {"SingerId", "AlbumId", "MarketingBudget"})
                .EmplaceRow(1, 1, b1 + transfer_amount)
                .EmplaceRow(2, 2, b2 - transfer_amount)
                .Build()});

  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Transfer was successful [spanner_read_write_transaction]\n";
}
//! [END spanner_read_write_transaction]

void ReadWriteTransactionCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "read-write-transaction <project-id> <instance-id> <database-id>");
  }

  ReadWriteTransaction(MakeSampleClient(argv[0], argv[1], argv[2]));
}

//! [START spanner_dml_standard_insert]
namespace spanner = google::cloud::spanner;
void DmlStandardInsert(google::cloud::spanner::Client client) {
  auto commit_result = spanner::RunTransaction(
      std::move(client), spanner::Transaction::ReadWriteOptions{},
      [](spanner::Client client, spanner::Transaction txn) {
        client.ExecuteSql(
            std::move(txn),
            spanner::SqlStatement(
                "INSERT INTO Singers (SingerId, FirstName, LastName)"
                "  VALUES (10, 'Virginia', 'Watson')",
                {}));
        return spanner::TransactionAction{spanner::TransactionAction::kCommit,
                                          {}};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Insert was successful [spanner_dml_standard_insert]\n";
}
//! [END spanner_dml_standard_insert]

void DmlStandardInsertCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "dml-standard-insert <project-id> <instance-id> <database-id>");
  }

  DmlStandardInsert(MakeSampleClient(argv[0], argv[1], argv[2]));
}

//! [START spanner_dml_standard_update]
void DmlStandardUpdate(google::cloud::spanner::Client client) {
  namespace gcs = google::cloud::spanner;
  auto commit_result = gcs::RunTransaction(
      std::move(client), gcs::Transaction::ReadWriteOptions{},
      [](gcs::Client client, gcs::Transaction txn) {
        client.ExecuteSql(
            std::move(txn),
            gcs::SqlStatement(
                "UPDATE Albums SET MarketingBudget = MarketingBudget * 2"
                " WHERE SingerId = 1 AND AlbumId = 1",
                {}));
        return gcs::TransactionAction{gcs::TransactionAction::kCommit, {}};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Update was successful [spanner_dml_standard_update]\n";
}
//! [END spanner_dml_standard_update]

void DmlStandardUpdateCommand(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw std::runtime_error(
        "dml-standard-update <project-id> <instance-id> <database-id>");
  }

  DmlStandardUpdate(MakeSampleClient(argv[0], argv[1], argv[2]));
}

int RunOneCommand(std::vector<std::string> argv) {
  using CommandType = std::function<void(std::vector<std::string> const&)>;

  std::map<std::string, CommandType> commands = {
      {"create-database", &CreateDatabase},
      {"add-column", &AddColumn},
      {"query-with-struct", &QueryWithStructCommand},
      {"drop-database", &DropDatabase},
      {"insert-data", &InsertDataCommand},
      {"update-data", &UpdateDataCommand},
      {"read-only-transaction", &ReadOnlyTransactionCommand},
      {"read-write-transaction", &ReadWriteTransactionCommand},
      {"dml-standard-insert", &DmlStandardInsertCommand},
      {"dml-standard-update", &DmlStandardUpdateCommand},
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
  auto instance_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
          .value_or("");

  if (project_id.empty()) {
    throw std::runtime_error("GOOGLE_CLOUD_PROJECT is not set or is empty");
  }
  if (instance_id.empty()) {
    throw std::runtime_error(
        "GOOGLE_CLOUD_CPP_SPANNER_INSTANCE is not set or is empty");
  }
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::string database_id = RandomDatabaseName(generator);

  RunOneCommand({"", "create-database", project_id, instance_id, database_id});
  RunOneCommand({"", "add-column", project_id, instance_id, database_id});

  namespace spanner = google::cloud::spanner;
  spanner::Database db(project_id, instance_id, database_id);
  google::cloud::spanner::Client client(
      google::cloud::spanner::MakeConnection(db));

  InsertData(client);
  UpdateData(client);
  ReadOnlyTransaction(client);
  ReadWriteTransaction(client);
  // TODO(#188) - Implement QueryWithStruct()
  QueryWithStructCommand({project_id, instance_id, database_id});
  DmlStandardInsert(client);
  DmlStandardUpdate(client);

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
