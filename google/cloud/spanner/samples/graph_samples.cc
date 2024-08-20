// Copyright 2024 Google LLC
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
#include "google/cloud/spanner/row.h"
#include "google/cloud/spanner/timestamp.h"
#include "google/cloud/spanner/testing/debug_log.h"  // TODO(#4758): remove
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include "google/cloud/project.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
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

google::cloud::spanner::Client MakeSampleClient(
    std::string const& project_id, std::string const& instance_id,
    std::string const& database_id) {
  return google::cloud::spanner::Client(google::cloud::spanner::MakeConnection(
      google::cloud::spanner::Database(project_id, instance_id, database_id)));
}

std::string Basename(absl::string_view name) {
  auto last_sep = name.find_last_of("/\\");
  if (last_sep != absl::string_view::npos) name.remove_prefix(last_sep + 1);
  return std::string(name);
}

//! [START spanner_create_database_with_property_graph]
void CreateDatabaseWithPropertyGraph(
		google::cloud::spanner_admin::DatabaseAdminClient client,
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
    CREATE TABLE Person (
      id               INT64 NOT NULL,
      name             STRING(MAX),
      birthday         TIMESTAMP,
      country          STRING(MAX),
      city             STRING(MAX),
    ) PRIMARY KEY (id))""");
  request.add_extra_statements(R"""(
    CREATE TABLE Account (
      id               INT64 NOT NULL,
      create_time      TIMESTAMP,
      is_blocked       BOOL,
      nick_name        STRING(MAX),
    ) PRIMARY KEY (id))""");
  request.add_extra_statements(R"""(
    CREATE TABLE PersonOwnAccount (
      id               INT64 NOT NULL,
      account_id       INT64 NOT NULL,
      create_time      TIMESTAMP,
      FOREIGN KEY (account_id)
      REFERENCES Account (id)
    ) PRIMARY KEY (id, account_id),
      INTERLEAVE IN PARENT Person ON DELETE CASCADE)""");
  request.add_extra_statements(R"""(
    CREATE TABLE AccountTransferAccount (
      id               INT64 NOT NULL,
      to_id            INT64 NOT NULL,
      amount           FLOAT64,
      create_time      TIMESTAMP NOT NULL OPTIONS
        (allow_commit_timestamp=true),
      order_number     STRING(MAX),
      FOREIGN KEY (to_id) REFERENCES Account (id)
    ) PRIMARY KEY (id, to_id, create_time),
      INTERLEAVE IN PARENT Account ON DELETE CASCADE)""");
  request.add_extra_statements(R"""(
    CREATE OR REPLACE PROPERTY GRAPH FinGraph
      NODE TABLES (Account, Person)
      EDGE TABLES (
        PersonOwnAccount
          SOURCE KEY(id) REFERENCES Person(id)
          DESTINATION KEY(account_id) REFERENCES Account(id)
          LABEL Owns,
        AccountTransferAccount
          SOURCE KEY(id) REFERENCES Account(id)
          DESTINATION KEY(to_id) REFERENCES Account(id)
          LABEL Transfers))""");
  auto db = client.CreateDatabase(request).get();
  if (!db) throw std::move(db).status();
  std::cout << "Database " << db->name() << " created with property graph.\n";
}
//! [END spanner_create_database_with_property_graph]

//! [START spanner_insert_graph_data]
void InsertData(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  auto insert_accounts = spanner::InsertMutationBuilder(
    "Account", {"id", "create_time", "is_blocked", "nick_name"})
    .EmplaceRow(7, google::cloud::spanner::Value("2020-01-10T06:22:20.12Z"), false, "Vacation Fund")
    .EmplaceRow(16, google::cloud::spanner::Value("2020-01-27T17:55:09.12Z"), true, "Vacation Fund")
    .EmplaceRow(20, google::cloud::spanner::Value("2020-02-18T05:44:20.12Z"), false, "Rainy Day Fund")
    .Build();

  auto insert_persons = spanner::InsertMutationBuilder(
    "Person", {"id", "name", "birthday", "country", "city"})
    .EmplaceRow(1, "Alex", google::cloud::spanner::Value("1991-12-21T00:00:00.12Z"), "Australia"," Adelaide")
    .EmplaceRow(2, "Dana", google::cloud::spanner::Value("1980-10-31T00:00:00.12Z"),"Czech_Republic", "Moravia")
    .EmplaceRow(3, "Lee", google::cloud::spanner::Value("1986-12-07T00:00:00.12Z"), "India", "Kollam")
    .Build();


  auto insert_transfers = spanner::InsertMutationBuilder(
    "AccountTransferAccount", {"id", "to_id", "amount", "create_time", "order_number"})
    .EmplaceRow(7, 16, 300.0, google::cloud::spanner::Value("2020-08-29T15:28:58.12Z"), "304330008004315")
    .EmplaceRow(7, 16, 100.0, google::cloud::spanner::Value("2020-10-04T16:55:05.12Z"), "304120005529714")
    .EmplaceRow(16, 20, 300.0, google::cloud::spanner::Value("2020-09-25T02:36:14.12Z"), "103650009791820")
    .EmplaceRow(20, 7, 500.0, google::cloud::spanner::Value("2020-10-04T16:55:05.12Z"), "304120005529714")
    .EmplaceRow(20, 16, 200.0, google::cloud::spanner::Value("2020-10-17T03:59:40.12Z"), "302290001255747") 
    .Build();

  auto insert_ownerships = spanner::InsertMutationBuilder(
    "PersonOwnAccount", {"id", "account_id", "create_time"})
    .EmplaceRow(1, 7, google::cloud::spanner::Value("2020-01-10T06:22:20.12Z"))
    .EmplaceRow(2, 20, google::cloud::spanner::Value("2020-01-27T17:55:09.12Z"))
    .EmplaceRow(3, 16, google::cloud::spanner::Value("2020-02-18T05:44:20.12Z"))
    .Build();

  auto commit_result =
      client.Commit(spanner::Mutations{insert_accounts, insert_persons, insert_transfers, insert_ownerships});
  if (!commit_result) throw std::move(commit_result).status();
  std::cout << "Insert was successful [spanner_insert_data]\n";
}
//! [END spanner_insert_graph_data]

//! [START spanner_insert_graph_data_with_dml]
void InsertDataWithDml(google::cloud::spanner::Client client) {
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
                "INSERT INTO Account (id, create_time, is_blocked) "
                "  VALUES"
                "    (1, CAST('2000-08-10 08:18:48.463959-07:52' AS TIMESTAMP), false),"
                "    (2, CAST('2000-08-12 08:18:48.463959-07:52' AS TIMESTAMP), true)"));
        if (!insert) return std::move(insert).status();
        rows_inserted = insert->RowsModified();
        return spanner::Mutations{};
      });
  if (!commit_result) throw std::move(commit_result).status();
  std::cout << "Rows inserted into Account: " << rows_inserted;

  commit_result = client.Commit(
      [&client, &rows_inserted](
          spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto insert = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "INSERT INTO AccountTransferAccount (id, to_id, create_time, amount) "
                "  VALUES"
                "    (1, 2, PENDING_COMMIT_TIMESTAMP(), 100),"
                "    (1, 1, PENDING_COMMIT_TIMESTAMP(), 200) "));
        if (!insert) return std::move(insert).status();
        rows_inserted = insert->RowsModified();
        return spanner::Mutations{};
      });
  if (!commit_result) throw std::move(commit_result).status();
  std::cout << "Rows inserted into AccountTransferAccount: " << rows_inserted;
 
  //! [execute-dml]
  std::cout << "Insert was successful [spanner_insert_graph_data_with_dml]\n";
}
//! [END spanner_insert_graph_data_with_dml]

//! [START spanner_update_graph_data_with_dml]
void UpdateDataWithDml(google::cloud::spanner::Client client) {
  using ::google::cloud::StatusOr;
  namespace spanner = ::google::cloud::spanner;
  auto commit_result = client.Commit(
      [&client](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto update = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "UPDATE Account SET is_blocked = false WHERE id = 2"));
        if (!update) return std::move(update).status();
        return spanner::Mutations{};
      });
  if (!commit_result) throw std::move(commit_result).status();

  commit_result = client.Commit(
      [&client](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto update = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "UPDATE AccountTransferAccount SET amount = 300 WHERE id = 1 AND to_id = 2"));
        if (!update) return std::move(update).status();
        return spanner::Mutations{};
      });
  if (!commit_result) throw std::move(commit_result).status();

  std::cout << "Update was successful [spanner_update_graph_data_with_dml]\n";
}
//! [END spanner_update_graph_data_with_dml]

//! [START spanner_update_graph_data_with_graph_query_in_dml]
void UpdateDataWithGraphQueryInDml(google::cloud::spanner::Client client) {
  using ::google::cloud::StatusOr;
  namespace spanner = ::google::cloud::spanner;
  auto commit_result = client.Commit(
      [&client](spanner::Transaction txn) -> StatusOr<spanner::Mutations> {
        auto update = client.ExecuteDml(
            std::move(txn),
            spanner::SqlStatement(
                "UPDATE Account SET is_blocked = true "
                "WHERE id IN {"
                "  GRAPH FinGraph"
                "  MATCH (a:Account WHERE a.id = 1)-[:TRANSFERS]->{1,2}(b:Account)"
                "  RETURN b.id}"));
        if (!update) return std::move(update).status();
        return spanner::Mutations{};
      });
  if (!commit_result) throw std::move(commit_result).status();
  std::cout << "Update was successful [spanner_update_graph_data_with_graph_query_in_dml]\n";
}
//! [END spanner_update_graph_data_with_graph_query_in_dml]

//! [START spanner_query_graph_data] [spanner-query-graph-data]
void QueryData(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  spanner::SqlStatement select(
    "Graph FinGraph "
    "MATCH (a:Person)-[o:Owns]->()-[t:Transfers]->()<-[p:Owns]-(b:Person)"
    "RETURN a.name AS sender, b.name AS receiver, t.amount, t.create_time AS transfer_at");
  using RowType = std::tuple<std::string, std::string, double, spanner::Timestamp>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::move(row).status();
    std::cout << "sender: " << std::get<0>(*row) << "\t";
    std::cout << "receiver: " << std::get<1>(*row) << "\t";
    std::cout << "amount: " << std::get<2>(*row) << "\t";
    std::cout << "transfer_at: " << std::get<3>(*row) << "\n";
  }

  std::cout << "Query completed for [spanner_query_graph_data]\n";
}
//! [END spanner_query_graph_data] [spanner-query-graph-data]


//! [START spanner_query_graph_data_with_parameter]
void QueryWithParameter(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  spanner::SqlStatement select(
      "Graph FinGraph "
      "MATCH (a:Person)-[o:Owns]->()-[t:Transfers]->()<-[p:Owns]-(b:Person) "
      "WHERE t.amount >= @min " 
      "RETURN a.name AS sender, b.name AS receiver, t.amount, t.create_time AS transfer_at",
      {{"min", spanner::Value(500)}});
  using RowType = std::tuple<std::string, std::string, double, spanner::Timestamp>;
  auto rows = client.ExecuteQuery(std::move(select));
  for (auto& row : spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::move(row).status();
    std::cout << "sender: " << std::get<0>(*row) << "\t";
    std::cout << "receiver: " << std::get<1>(*row) << "\t";
    std::cout << "amount: " << std::get<2>(*row) << "\t";
    std::cout << "transfer_at: " << std::get<3>(*row) << "\n";
  }

  std::cout << "Query completed for [spanner_query_with_parameter]\n";
}
//! [END spanner_query_graph_data_with_parameter]

//! [START spanner_delete_graph_data_with_dml]
void DeleteDataWithDml(google::cloud::spanner::Client client) {
  using ::google::cloud::StatusOr;
  namespace spanner = ::google::cloud::spanner;
  auto commit_result = client.Commit([&client](spanner::Transaction txn)
                                         -> StatusOr<spanner::Mutations> {
    auto dele = client.ExecuteDml(
        std::move(txn),
        spanner::SqlStatement(
            "DELETE FROM AccountTransferAccount WHERE id = 1 AND to_id = 2"));
    if (!dele) return std::move(dele).status();
    return spanner::Mutations{};
  });
  if (!commit_result) throw std::move(commit_result).status();

  commit_result = client.Commit([&client](spanner::Transaction txn)
                                         -> StatusOr<spanner::Mutations> {
    auto dele = client.ExecuteDml(
        std::move(txn),
        spanner::SqlStatement("DELETE FROM Account WHERE id = 2"));
    if (!dele) return std::move(dele).status();
    return spanner::Mutations{};
  });
  if (!commit_result) throw std::move(commit_result).status();

  std::cout << "Delete was successful [spanner_delete_graph_data_with_dml]\n";
}
//! [END spanner_delete_graph_data_with_dml]

//! [START spanner_delete_graph_data]
void DeleteData(google::cloud::spanner::Client client) {
  namespace spanner = ::google::cloud::spanner;

  // Delete the 'Owns' relationships with key (1,7) and (2,20).
  auto delete_ownerships = spanner::DeleteMutationBuilder(
                           "PersonOwnAccount", spanner::KeySet()
                                         .AddKey(spanner::MakeKey(1, 7))
                                         .AddKey(spanner::MakeKey(2, 20)))
                           .Build();

  // Delete transfers using the keys in the range [1, 8]
  auto delete_transfer_range =
      spanner::DeleteMutationBuilder(
            "AccountTransferAccount",
            spanner::KeySet().AddRange(
                spanner::MakeKeyBoundClosed(1),
                spanner::MakeKeyBoundOpen(8)))
          .Build();

  // Deletes rows from the Account table and the AccountTransferAccount table,
  // because the AccountTransferAccount table is defined with ON DELETE CASCADE.
  auto delete_accounts_all =
      spanner::MakeDeleteMutation("Account", spanner::KeySet::All());

  // Deletes rows from the Person table and the PersonOwnAccount table, because
  // the PersonOwnAccount table is defined with ON DELETE CASCADE.
  auto delete_persons_all =
      spanner::MakeDeleteMutation("Person", spanner::KeySet::All());

  auto commit_result = client.Commit(spanner::Mutations{
      delete_ownerships, delete_transfer_range, delete_accounts_all, delete_persons_all});
  if (!commit_result) throw std::move(commit_result).status();
  std::cout << "Delete was successful [spanner_delete_graph_data]\n";
}
//! [END spanner_delete_graph_data]

int RunOneCommand(std::vector<std::string> argv) {
  using CommandType = std::function<void(std::vector<std::string> const&)>;
  using CommandMap = std::map<std::string, CommandType>;

  using SampleFunction = void (*)(google::cloud::spanner::Client);
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
      make_database_command_entry("create-database-with-property-graph",
                                  CreateDatabaseWithPropertyGraph),
      make_command_entry("insert-data", InsertData),
      make_command_entry("insert-data-with-dml", InsertDataWithDml),
      make_command_entry("update-data-with-dml", UpdateDataWithDml),
      make_command_entry("update-data-with-graph-query-in-dml", UpdateDataWithGraphQueryInDml),
      make_command_entry("query-data", QueryData),
      make_command_entry("query-data-with-parameter", QueryWithParameter),
      make_command_entry("delete-data-with-dml", DeleteDataWithDml),
      make_command_entry("delete-data", DeleteData),
  };

  static std::string usage_msg = [&argv, &commands] {
    std::string usage;
    usage += "Usage: ";
    usage += Basename(argv[0]);
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

}  // namespace

int main(int ac, char* av[]) try {
  return RunOneCommand({av, av + ac});
} catch (google::cloud::Status const& status) {
  std::cerr << status << "\n";
  google::cloud::LogSink::Instance().Flush();
  return 1;
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  return 1;
}
