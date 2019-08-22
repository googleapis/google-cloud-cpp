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

  for (auto row : reader->Rows<std::string>()) {
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

//! [START spanner_dml_standard_insert]
void DmlStandardInsert(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
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

//! [START spanner_dml_standard_update]
void DmlStandardUpdate(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  auto commit_result = spanner::RunTransaction(
      std::move(client), spanner::Transaction::ReadWriteOptions{},
      [](spanner::Client client, spanner::Transaction txn) {
        client.ExecuteSql(
            std::move(txn),
            spanner::SqlStatement(
                "UPDATE Albums SET MarketingBudget = MarketingBudget * 2"
                " WHERE SingerId = 1 AND AlbumId = 1",
                {}));
        return spanner::TransactionAction{spanner::TransactionAction::kCommit,
                                          {}};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Update was successful [spanner_dml_standard_update]\n";
}
//! [END spanner_dml_standard_update]

//! [START spanner_dml_standard_delete]
void DmlStandardDelete(google::cloud::spanner::Client client) {
  namespace spanner = google::cloud::spanner;
  auto commit_result = spanner::RunTransaction(
      std::move(client), spanner::Transaction::ReadWriteOptions{},
      [](spanner::Client client, spanner::Transaction txn) {
        client.ExecuteSql(std::move(txn),
                          spanner::SqlStatement(
                              "DELETE FROM Singers WHERE FirstName = 'Alice'"));
        return spanner::TransactionAction{spanner::TransactionAction::kCommit,
                                          {}};
      });
  if (!commit_result) {
    throw std::runtime_error(commit_result.status().message());
  }
  std::cout << "Delete was successful [spanner_dml_standard_delete]\n";
}
//! [END spanner_dml_standard_delete]

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
  namespace spanner = google::cloud::spanner;
  // [START spanner_create_struct_with_data]
  auto singer_info = std::make_tuple("Elena", "Campbell");
  // [END spanner_create_struct_with_data]

  auto reader = client.ExecuteSql(spanner::SqlStatement(
      "SELECT SingerId FROM Singers WHERE (FirstName, LastName) = @name",
      {{"name", spanner::Value(singer_info)}}));
  if (!reader) throw std::runtime_error(reader.status().message());

  for (auto row : reader->Rows<std::int64_t>()) {
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

  for (auto row : reader->Rows<std::int64_t>()) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << row->get<0>() << "\n";
  }
  std::cout << "Query completed for"
            << " [spanner_query_data_with_array_of_struct]\n";
}
//! [END spanner_query_data_with_array_of_struct]

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
      {"create-database", &CreateDatabase},
      {"add-column", &AddColumn},
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
      make_command_entry("write-data-for-struct-queries",
                         &WriteDataForStructQueries),
      make_command_entry("query-data-with-struct", &QueryDataWithStruct),
      make_command_entry("query-data-with-array-of-struct",
                         &QueryDataWithArrayOfStruct),
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

  std::cout << "Running samples in database " << database_id << "\n";

  std::cout << "\nRunning spanner_create_database sample\n";
  RunOneCommand({"", "create-database", project_id, instance_id, database_id});

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
