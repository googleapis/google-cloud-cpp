// Copyright 2022 Google LLC
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

#include "google/cloud/spanner/admin/database_admin_client.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/testing/pick_random_instance.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace samples {

// [START spanner_postgresql_create_database]
void CreateDatabase(google::cloud::spanner_admin::DatabaseAdminClient client,
                    google::cloud::spanner::Database const& database) {
  google::spanner::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent(database.instance().FullName());
  request.set_create_statement("CREATE DATABASE " + database.database_id());
  request.set_database_dialect(
      google::spanner::admin::database::v1::DatabaseDialect::POSTGRESQL);
  auto db = client.CreateDatabase(request).get();
  if (!db) throw std::runtime_error(db.status().message());
  std::cout << "Database " << db->name() << " created.\n";
}
// [END spanner_postgresql_create_database]

void CreateTables(google::cloud::spanner_admin::DatabaseAdminClient client,
                  google::cloud::spanner::Database const& database) {
  std::vector<std::string> statements = {
      R"""(
        CREATE TABLE Singers (
            SingerId   BIGINT NOT NULL,
            FirstName  CHARACTER VARYING(1024),
            LastName   CHARACTER VARYING(1024),
            singerInfo BYTEA,
            PRIMARY KEY(singerid)
        )
      )""",
      R"""(
        CREATE TABLE Albums (
            AlbumId    BIGINT NOT NULL,
            SingerId   BIGINT NOT NULL,
            AlbumTitle CHARACTER VARYING,
            PRIMARY KEY(SingerId, AlbumId),
            FOREIGN KEY(SingerId) REFERENCES Singers(SingerId)
        )
      )""",
  };
  auto metadata =
      client.UpdateDatabaseDdl(database.FullName(), statements).get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "Tables created.\nNew DDL:\n" << metadata->DebugString();
}

// [START spanner_postgresql_add_column]
void AddColumn(google::cloud::spanner_admin::DatabaseAdminClient client,
               google::cloud::spanner::Database const& database) {
  std::vector<std::string> statements = {
      R"""(
        ALTER TABLE Albums
            ADD COLUMN MarketingBudget BIGINT
      )""",
  };
  auto metadata =
      client.UpdateDatabaseDdl(database.FullName(), statements).get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "Column added.\nNew DDL:\n" << metadata->DebugString();
}
// [END spanner_postgresql_add_column]

// [START spanner_postgresql_create_storing_index]
void CreateStoringIndex(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    google::cloud::spanner::Database const& database) {
  std::vector<std::string> statements = {
      R"""(
        CREATE INDEX AlbumsByAlbumTitle2
            ON Albums (AlbumTitle NULLS FIRST)
            INCLUDE (MarketingBudget)
      )""",
  };
  auto metadata =
      client.UpdateDatabaseDdl(database.FullName(), statements).get();
  if (!metadata) throw std::runtime_error(metadata.status().message());
  std::cout << "Index added.\nNew DDL:\n" << metadata->DebugString();
}
// [END spanner_postgresql_create_storing_index]

void InsertData(google::cloud::spanner::Client client) {
  auto insert_singers =  //
      google::cloud::spanner::InsertOrUpdateMutationBuilder(
          "Singers", {"SingerId", "FirstName", "LastName"})
          .EmplaceRow(1, "Bruce", "Allison")
          .EmplaceRow(2, "Alice", "Bruxelles")
          .Build();
  auto insert_albums =
      google::cloud::spanner::InsertOrUpdateMutationBuilder(
          "Albums", {"SingerId", "AlbumId", "AlbumTitle", "MarketingBudget"})
          .EmplaceRow(1, 1, "Total Junk", 100000)
          .EmplaceRow(1, 2, "Go, Go, Go", 200000)
          .EmplaceRow(2, 1, "Green", 300000)
          .EmplaceRow(2, 2, "Forever Hold Your Peace", 400000)
          .EmplaceRow(2, 3, "Terrified", 500000)
          .Build();
  auto commit = client.Commit(
      google::cloud::spanner::Mutations{insert_singers, insert_albums});
  if (!commit) throw std::runtime_error(commit.status().message());
  std::cout << "Insert was successful.\n";
}

// [START spanner_postgresql_query_with_parameter]
void QueryWithParameter(google::cloud::spanner::Client client) {
  std::cout << "Listing all singers with a last name that starts with 'A'\n";
  google::cloud::spanner::SqlStatement select(
      "SELECT SingerId, FirstName, LastName FROM Singers"
      " WHERE LastName LIKE $1",
      {{"p1", google::cloud::spanner::Value("A%")}});
  using RowType = std::tuple<std::int64_t, std::string, std::string>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto const& row : google::cloud::spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "SingerId: " << std::get<0>(*row) << "\t";
    std::cout << "FirstName: " << std::get<1>(*row) << "\t";
    std::cout << "LastName: " << std::get<2>(*row) << "\n";
  }
  std::cout << "Query completed.\n";
}
// [END spanner_postgresql_query_with_parameter]

// [START spanner_postgresql_dml_getting_started_update]
void DmlGettingStartedUpdate(google::cloud::spanner::Client client) {
  // A helper to read the budget for the given album and singer.
  auto get_budget =
      [&](google::cloud::spanner::Transaction txn, std::int64_t album_id,
          std::int64_t singer_id) -> google::cloud::StatusOr<std::int64_t> {
    auto key = google::cloud::spanner::KeySet().AddKey(
        google::cloud::spanner::MakeKey(album_id, singer_id));
    auto rows = client.Read(std::move(txn), "Albums", key, {"MarketingBudget"});
    using RowType = std::tuple<absl::optional<std::int64_t>>;
    auto row = google::cloud::spanner::GetSingularRow(
        google::cloud::spanner::StreamOf<RowType>(rows));
    if (!row) return std::move(row).status();
    auto const budget = std::get<0>(*row);
    return budget ? *budget : 0;
  };

  // A helper to update the budget for the given album and singer.
  auto update_budget = [&](google::cloud::spanner::Transaction txn,
                           std::int64_t album_id, std::int64_t singer_id,
                           std::int64_t budget) {
    auto sql = google::cloud::spanner::SqlStatement(
        "UPDATE Albums SET MarketingBudget = $1"
        "  WHERE SingerId = $3 AND AlbumId = $2",
        {{"p1", google::cloud::spanner::Value(budget)},
         {"p2", google::cloud::spanner::Value(album_id)},
         {"p3", google::cloud::spanner::Value(singer_id)}});
    return client.ExecuteDml(std::move(txn), std::move(sql));
  };

  auto const transfer_amount = 20000;
  auto commit = client.Commit(
      [&](google::cloud::spanner::Transaction const& txn)
          -> google::cloud::StatusOr<google::cloud::spanner::Mutations> {
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
        return google::cloud::spanner::Mutations{};
      });
  if (!commit) throw std::runtime_error(commit.status().message());
  std::cout << "Update was successful.\n";
}
// [END spanner_postgresql_dml_getting_started_update]

void DropDatabase(google::cloud::spanner_admin::DatabaseAdminClient client,
                  google::cloud::spanner::Database const& database) {
  auto status = client.DropDatabase(database.FullName());
  if (!status.ok()) throw std::runtime_error(status.message());
  std::cout << "Database " << database << " dropped.\n";
}

}  // namespace samples

google::cloud::spanner::Database Database(std::vector<std::string> argv) {
  if (argv.size() != 4) {
    std::string args = " <project-id> <instance-id> <database-id>";
    throw std::runtime_error(argv[0] + args);
  }
  google::cloud::spanner::Instance instance(std::move(argv[1]),
                                            std::move(argv[2]));
  return google::cloud::spanner::Database(instance, std::move(argv[3]));
}

using CommandType = std::function<void(std::vector<std::string>)>;

CommandType Command(
    void (*sample)(google::cloud::spanner_admin::DatabaseAdminClient,
                   google::cloud::spanner::Database const&)) {
  return [sample](std::vector<std::string> argv) {
    auto database = Database(std::move(argv));
    google::cloud::spanner_admin::DatabaseAdminClient client(
        google::cloud::spanner_admin::MakeDatabaseAdminConnection());
    sample(std::move(client), database);
  };
}

CommandType Command(void (*sample)(google::cloud::spanner::Client)) {
  return [sample](std::vector<std::string> argv) {
    auto database = Database(std::move(argv));
    google::cloud::spanner::Client client(
        google::cloud::spanner::MakeConnection(database));
    sample(std::move(client));
  };
}

int RunOneCommand(std::vector<std::string> argv) {
  std::map<std::string, CommandType> commands = {
      {"create-database", Command(samples::CreateDatabase)},
      {"create-tables", Command(samples::CreateTables)},
      {"add-column", Command(samples::AddColumn)},
      {"create-storing-index", Command(samples::CreateStoringIndex)},
      {"insert-data", Command(samples::InsertData)},
      {"query-with-parameter", Command(samples::QueryWithParameter)},
      {"dml-getting-started-update", Command(samples::DmlGettingStartedUpdate)},
      {"drop-database", Command(samples::DropDatabase)},
  };
  auto it = commands.find(argv[0]);
  if (it == commands.end()) {
    throw std::runtime_error(argv[0] + ": Unknown command");
  }
  it->second(std::move(argv));
  return 0;
}

void SampleBanner(std::string const& name) {
  std::cout << "\nRunning " << name << " sample at "
            << absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", absl::Now(),
                                absl::UTCTimeZone())
            << std::endl;
  GCP_LOG(DEBUG) << "Running " << name << " sample";
}

int RunAll() {
  auto generator = google::cloud::internal::MakeDefaultPRNG();

  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  if (project_id.empty()) {
    throw std::runtime_error("GOOGLE_CLOUD_PROJECT is not set or is empty");
  }

  auto random_instance = google::cloud::spanner_testing::PickRandomInstance(
      generator, project_id,
      "labels.samples:yes AND NOT name:/instances/test-instance-mr-");
  if (!random_instance) {
    throw std::runtime_error("Cannot find an instance to run the samples: " +
                             random_instance.status().message());
  }
  auto const instance_id = *std::move(random_instance);
  std::cout << "Running samples on " << instance_id << std::endl;

  auto const database_id =
      google::cloud::spanner_testing::RandomDatabaseName(generator);

  google::cloud::spanner::Instance instance(project_id, instance_id);
  google::cloud::spanner::Database database(instance, database_id);

  google::cloud::spanner_admin::DatabaseAdminClient database_admin_client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());

  SampleBanner("spanner_postgresql_create_database");
  samples::CreateDatabase(database_admin_client, database);

  SampleBanner("spanner_postgresql_create_tables");
  samples::CreateTables(database_admin_client, database);

  SampleBanner("spanner_postgresql_add_column");
  samples::AddColumn(database_admin_client, database);

  SampleBanner("spanner_postgresql_create_storing_index");
  samples::CreateStoringIndex(database_admin_client, database);

  google::cloud::spanner::Client client(
      google::cloud::spanner::MakeConnection(database));

  SampleBanner("spanner_insert_data");
  samples::InsertData(client);

  SampleBanner("spanner_postgresql_query_with_parameter");
  samples::QueryWithParameter(client);

  SampleBanner("spanner_postgresql_dml_getting_started_update");
  samples::DmlGettingStartedUpdate(client);

  /*
   * Remaining samples to add:
   *   spanner_postgresql_batch_dml
   *   spanner_postgresql_case_sensitivity
   *   spanner_postgresql_cast_data_type
   *   spanner_postgresql_create_clients
   *   spanner_postgresql_dml_with_parameters
   *   spanner_postgresql_functions
   *   spanner_postgresql_information_schema
   *   spanner_postgresql_interleaved_table
   *   spanner_postgresql_order_nulls
   *   spanner_postgresql_partitioned_dml
   *   spanner_postgresql_numeric_data_type
   */

  SampleBanner("spanner_drop_database");
  samples::DropDatabase(database_admin_client, database);

  return 0;
}

}  // namespace

int main(int ac, char* av[]) try {
  const auto* const emulator_host = "SPANNER_EMULATOR_HOST";
  if (google::cloud::internal::GetEnv(emulator_host).has_value()) {
    return 0;  // emulator does not support PostgreSQL dialect
  }
  const auto* const auto_run = "GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES";
  if (google::cloud::internal::GetEnv(auto_run).value_or("") == "yes") {
    return RunAll();
  }
  std::string program(ac ? (ac--, *av++) : "pg_samples");
  if (ac == 0) {
    throw std::runtime_error("Usage: " + program +
                             " <command> [<argument> ...]");
  }
  return RunOneCommand({av, av + ac});
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  google::cloud::LogSink::Instance().Flush();
  return 1;
}
