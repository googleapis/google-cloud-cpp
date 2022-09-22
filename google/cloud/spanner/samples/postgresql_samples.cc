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
#include "google/cloud/spanner/bytes.h"
#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/numeric.h"
#include "google/cloud/spanner/testing/debug_log.h"  // TODO(#4758): remove
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
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace samples {

// [START spanner_postgresql_create_clients]
void CreateClients(google::cloud::spanner::Database const& database) {
  google::cloud::spanner_admin::DatabaseAdminClient database_admin_client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());
  google::cloud::spanner::Client client(
      google::cloud::spanner::MakeConnection(database));
}
// [END spanner_postgresql_create_clients]

// [START spanner_postgresql_create_database]
void CreateDatabase(google::cloud::spanner_admin::DatabaseAdminClient client,
                    google::cloud::spanner::Database const& database) {
  google::spanner::admin::database::v1::CreateDatabaseRequest request;
  request.set_parent(database.instance().FullName());
  request.set_create_statement("CREATE DATABASE \"" + database.database_id() +
                               "\"");
  request.set_database_dialect(
      google::spanner::admin::database::v1::DatabaseDialect::POSTGRESQL);
  auto db = client.CreateDatabase(request).get();
  if (!db) throw std::move(db).status();
  std::cout << "Database " << db->name() << " created.\n";
}
// [END spanner_postgresql_create_database]

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
  google::cloud::spanner_testing::LogUpdateDatabaseDdl(  //! TODO(#4758)
      client, database, metadata.status());              //! TODO(#4758)
  if (!metadata) throw std::move(metadata).status();
  std::cout << "Column added.\nNew DDL:\n" << metadata->DebugString();
}
// [END spanner_postgresql_add_column]

void InsertData(google::cloud::spanner::Client client) {
  auto insert_singers =  //
      google::cloud::spanner::InsertMutationBuilder(
          "Singers", {"SingerId", "FirstName", "LastName"})
          .EmplaceRow(1, "Marc", "Richards")
          .EmplaceRow(2, "Catalina", "Smith")
          .Build();
  auto insert_albums =  //
      google::cloud::spanner::InsertMutationBuilder(
          "Albums", {"SingerId", "AlbumId", "AlbumTitle", "MarketingBudget"})
          .EmplaceRow(1, 1, "Total Junk", 100000)
          .EmplaceRow(1, 2, "Go, Go, Go", 200000)
          .EmplaceRow(2, 1, "Green", 300000)
          .EmplaceRow(2, 2, "Forever Hold Your Peace", 400000)
          .EmplaceRow(2, 3, "Terrified", 500000)
          .Build();
  auto insert_users =  //
      google::cloud::spanner::InsertMutationBuilder(
          "users", {"user_id", "user_name", "active"})
          .EmplaceRow(1, "User 1", false)
          .EmplaceRow(2, "User 2", false)
          .EmplaceRow(3, "User 3", true)
          .Build();
  auto commit = client.Commit(google::cloud::spanner::Mutations{
      insert_singers, insert_albums, insert_users});
  if (!commit) throw std::move(commit).status();
  std::cout << "Insert was successful.\n";
}

// [START spanner_postgresql_query_with_parameter]
void QueryWithParameter(google::cloud::spanner::Client client) {
  std::cout << "Listing all singers with a last name that starts with 'S'\n";
  auto sql = google::cloud::spanner::SqlStatement(
      "SELECT SingerId, FirstName, LastName FROM Singers"
      "  WHERE LastName LIKE $1",
      {{"p1", google::cloud::spanner::Value("S%")}});
  using RowType = std::tuple<std::int64_t, std::string, std::string>;
  auto rows = client.ExecuteQuery(std::move(sql));
  for (auto& row : google::cloud::spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::move(row).status();
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
                           std::int64_t singer_id, std::int64_t album_id,
                           std::int64_t budget) {
    auto sql = google::cloud::spanner::SqlStatement(
        "UPDATE Albums SET MarketingBudget = $1"
        "  WHERE SingerId = $2 AND AlbumId = $3",
        {{"p1", google::cloud::spanner::Value(budget)},
         {"p2", google::cloud::spanner::Value(singer_id)},
         {"p3", google::cloud::spanner::Value(album_id)}});
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
  if (!commit) throw std::move(commit).status();
  std::cout << "Update was successful.\n";
}
// [END spanner_postgresql_dml_getting_started_update]

// [START spanner_postgresql_batch_dml]
void BatchDml(google::cloud::spanner::Client client) {
  auto commit = client.Commit(
      [&client](google::cloud::spanner::Transaction const& txn)
          -> google::cloud::StatusOr<google::cloud::spanner::Mutations> {
        std::vector<google::cloud::spanner::SqlStatement> sql = {
            google::cloud::spanner::SqlStatement(
                "INSERT INTO Singers (SingerId, FirstName, LastName)"
                "    VALUES ($1, $2, $3)",
                {{"p1", google::cloud::spanner::Value(3)},
                 {"p2", google::cloud::spanner::Value("Alice")},
                 {"p3", google::cloud::spanner::Value("Trentor")}}),
            google::cloud::spanner::SqlStatement(
                "INSERT INTO Singers (SingerId, FirstName, LastName)"
                "    VALUES ($1, $2, $3)",
                {{"p1", google::cloud::spanner::Value(4)},
                 {"p2", google::cloud::spanner::Value("Lea")},
                 {"p3", google::cloud::spanner::Value("Martin")}}),
        };
        auto result = client.ExecuteBatchDml(txn, std::move(sql));
        if (!result) return std::move(result).status();
        for (std::size_t i = 0; i < result->stats.size(); ++i) {
          std::cout << result->stats[i].row_count << " row(s) affected"
                    << " for statement " << (i + 1) << ".\n";
        }
        // Batch operations may have partial failures, in which case
        // ExecuteBatchDml() returns with success, but the application
        // should verify that all statements completed successfully.
        if (!result->status.ok()) return result->status;
        return google::cloud::spanner::Mutations{};
      });
  if (!commit) throw std::move(commit).status();
  std::cout << "Update was successful.\n";
}
// [END spanner_postgresql_batch_dml]

// [START spanner_postgresql_case_sensitivity]
void CaseSensitivity(
    google::cloud::spanner_admin::DatabaseAdminClient admin_client,
    google::cloud::spanner::Database const& database,
    google::cloud::spanner::Client client) {
  std::vector<std::string> statements = {
      R"""(
        CREATE TABLE Singers (
            -- SingerId will be folded to "singerid"
            SingerId        BIGINT NOT NULL,
            -- FirstName and LastName are double-quoted and will therefore
            -- retain their mixed case and are case-sensitive. This means
            -- that any statement that references any of these columns must
            -- use double quotes.
            "FirstName"     CHARACTER VARYING(1024),
            "LastName"      CHARACTER VARYING(1024),
            SingerInfo      BYTEA,
            PRIMARY KEY(singerid)
        )
      )""",
      R"""(
        CREATE TABLE Albums (
            SingerId        BIGINT NOT NULL,
            AlbumId         BIGINT NOT NULL,
            AlbumTitle      CHARACTER VARYING,
            MarketingBudget BIGINT,
            PRIMARY KEY(SingerId, AlbumId),
            FOREIGN KEY(SingerId) REFERENCES Singers(SingerId)
        )
      )""",
  };
  auto metadata =
      admin_client.UpdateDatabaseDdl(database.FullName(), statements).get();
  google::cloud::spanner_testing::LogUpdateDatabaseDdl(  //! TODO(#4758)
      admin_client, database, metadata.status());        //! TODO(#4758)
  if (!metadata) throw std::move(metadata).status();
  std::cout << "Tables created.\nNew DDL:\n" << metadata->DebugString();

  // Column names in mutations are always case-insensitive, regardless
  // of whether the columns were double-quoted or not during creation.
  auto insert_singers =  //
      google::cloud::spanner::InsertMutationBuilder(
          "Singers", {"singerid", "firstname", "lastname"})
          .EmplaceRow(1, "Marc", "Richards")
          .EmplaceRow(2, "Catalina", "Smith")
          .Build();
  auto commit =
      client.Commit(google::cloud::spanner::Mutations{insert_singers});
  if (!commit) throw std::move(commit).status();
  std::cout << "Insert was successful.\n";

  // DML statements must also follow the PostgreSQL case rules.
  commit = client.Commit(
      [&client](google::cloud::spanner::Transaction txn)
          -> google::cloud::StatusOr<google::cloud::spanner::Mutations> {
        auto sql = google::cloud::spanner::SqlStatement(
            R"""(
                INSERT INTO Singers (SingerId, "FirstName", "LastName")
                    VALUES (3, 'Alice', 'Trentor')
            )""");
        auto insert = client.ExecuteDml(std::move(txn), std::move(sql));
        if (!insert) return std::move(insert).status();
        return google::cloud::spanner::Mutations{};
      });
  if (!commit) throw std::move(commit).status();
  std::cout << "Insert was successful.\n";

  auto sql = google::cloud::spanner::SqlStatement("SELECT * FROM Singers");
  for (auto& row : client.ExecuteQuery(std::move(sql))) {
    if (!row) throw std::move(row).status();

    // SingerId is automatically folded to lower case. Accessing the
    // column by its name must therefore use all lower-case letters.
    if (auto singer_id = row->get<std::int64_t>("singerid")) {
      std::cout << "SingerId: " << *singer_id << "\t";
    } else {
      std::cerr << singer_id.status();
    }

    // FirstName and LastName were double-quoted during creation,
    // and retain their mixed case when returned in a row.
    if (auto first_name = row->get<std::string>("FirstName")) {
      std::cout << "FirstName: " << *first_name << "\t";
    } else {
      std::cerr << first_name.status();
    }
    if (auto last_name = row->get<std::string>("LastName")) {
      std::cout << "LastName: " << *last_name;
    } else {
      std::cerr << last_name.status();
    }
    std::cout << "\n";
  }

  // Aliases are also identifiers, and specifying an alias in
  // double quotes will make the alias retain its case.
  sql = google::cloud::spanner::SqlStatement(R"""(
      SELECT singerid AS "SingerId",
          CONCAT("FirstName", ' '::VARCHAR, "LastName") AS "FullName"
          FROM Singers
  )""");
  for (auto& row : client.ExecuteQuery(std::move(sql))) {
    if (!row) throw std::move(row).status();

    // The aliases are double-quoted and therefore retain their mixed case.
    if (auto singer_id = row->get<std::int64_t>("SingerId")) {
      std::cout << "SingerId: " << *singer_id << "\t";
    } else {
      std::cerr << singer_id.status();
    }
    if (auto full_name = row->get<std::string>("FullName")) {
      std::cout << "FullName: " << *full_name;
    } else {
      std::cerr << full_name.status();
    }
    std::cout << "\n";
  }
}
// [END spanner_postgresql_case_sensitivity]

// [START spanner_postgresql_cast_data_type]
void CastDataType(google::cloud::spanner::Client client) {
  // The `::` operator can be used to cast from one data type to another.
  auto sql = google::cloud::spanner::SqlStatement(R"""(
      SELECT 1::VARCHAR as str,
             '2'::INT as int,
             3::DECIMAL as dec,
             '4'::BYTEA as bytes,
             5::FLOAT as float,
             'true'::BOOL as bool,
             '2021-11-03T09:35:01UTC'::TIMESTAMPTZ as timestamp
  )""");
  using RowType =
      std::tuple<std::string, std::int64_t, google::cloud::spanner::PgNumeric,
                 google::cloud::spanner::Bytes, double, bool,
                 google::cloud::spanner::Timestamp>;
  auto rows = client.ExecuteQuery(std::move(sql));
  for (auto& row : google::cloud::spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::move(row).status();
    std::cout << "String:    " << std::get<0>(*row) << "\n";
    std::cout << "Int:       " << std::get<1>(*row) << "\n";
    std::cout << "Decimal:   " << std::get<2>(*row) << "\n";
    std::cout << "Bytes:     " << std::get<3>(*row) << "\n";
    std::cout << "Float:     " << std::get<4>(*row) << "\n";
    std::cout << "Bool:      " << std::boolalpha << std::get<5>(*row) << "\n";
    std::cout << "Timestamp: " << std::get<6>(*row) << "\n";
  }
}
// [END spanner_postgresql_cast_data_type]

// [START spanner_postgresql_order_nulls]
void OrderNulls(google::cloud::spanner::Client client) {
  // Spanner PostgreSQL follows the ORDER BY rules for NULL values of
  // PostgreSQL. This means that:
  //   1. NULL values are ordered last by default when a query result is
  //      ordered in ascending order.
  //   2. NULL values are ordered first by default when a query result is
  //      ordered in descending order.
  //   3. NULL values can be order first or last by specifying NULLS FIRST
  //      or NULLS LAST in the ORDER BY clause.
  auto commit = client.Commit(
      [&client](google::cloud::spanner::Transaction txn)
          -> google::cloud::StatusOr<google::cloud::spanner::Mutations> {
        auto sql = google::cloud::spanner::SqlStatement(
            R"""(
                INSERT INTO Singers (SingerId, "FirstName", "LastName")
                    VALUES (4, 'Cher', NULL)
            )""");
        auto insert = client.ExecuteDml(std::move(txn), std::move(sql));
        if (!insert) return std::move(insert).status();
        return google::cloud::spanner::Mutations{};
      });
  if (!commit) throw std::move(commit).status();
  std::cout << "Insertion of NULL LastName was successful.\n";

  using RowType = std::tuple<absl::optional<std::string>>;
  for (std::string option : {"", " DESC", " NULLS FIRST", " NULLS LAST"}) {
    auto sql = google::cloud::spanner::SqlStatement(
        R"""(SELECT "LastName" FROM Singers ORDER BY "LastName")""" + option);
    std::cout << sql.sql() << "\n";
    auto rows = client.ExecuteQuery(std::move(sql));
    for (auto& row : google::cloud::spanner::StreamOf<RowType>(rows)) {
      if (!row) throw std::move(row).status();
      std::cout << "    ";
      if (std::get<0>(*row).has_value()) {
        std::cout << *std::get<0>(*row);
      } else {
        std::cout << "NULL";
      }
      std::cout << "\n";
    }
  }
}
// [END spanner_postgresql_order_nulls]

// [START spanner_postgresql_dml_with_parameters]
void DmlWithParameters(google::cloud::spanner::Client client) {
  google::cloud::spanner::DmlResult dml_result;
  auto commit = client.Commit(
      [&client, &dml_result](google::cloud::spanner::Transaction txn)
          -> google::cloud::StatusOr<google::cloud::spanner::Mutations> {
        auto sql = google::cloud::spanner::SqlStatement(
            R"""(
                INSERT INTO Singers (SingerId, "FirstName", "LastName")
                    VALUES ($1, $2, $3),
                           ($4, $5, $6)
            )""",
            {{"p1", google::cloud::spanner::Value(5)},
             {"p2", google::cloud::spanner::Value("Alice")},
             {"p3", google::cloud::spanner::Value("Henderson")},
             {"p4", google::cloud::spanner::Value(6)},
             {"p5", google::cloud::spanner::Value("Bruce")},
             {"p6", google::cloud::spanner::Value("Allison")}});
        auto insert = client.ExecuteDml(std::move(txn), std::move(sql));
        if (!insert) return std::move(insert).status();
        dml_result = *std::move(insert);
        return google::cloud::spanner::Mutations{};
      });
  if (!commit) throw std::move(commit).status();
  std::cout << "Inserted " << dml_result.RowsModified() << " singers.\n";
}
// [END spanner_postgresql_dml_with_parameters]

// [START spanner_postgresql_functions]
void Functions(google::cloud::spanner::Client client) {
  // Use the PostgreSQL `to_timestamp` function to convert a number of
  // seconds after the Unix epoch to a timestamp.
  //   $ date --utc --iso-8601=seconds --date=@1284352323
  //   2010-09-13T04:32:03+00:00
  auto sql = google::cloud::spanner::SqlStatement(R"""(
      SELECT to_timestamp(1284352323) AS t
  )""");
  using RowType = std::tuple<google::cloud::spanner::Timestamp>;
  auto rows = client.ExecuteQuery(std::move(sql));
  for (auto& row : google::cloud::spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::move(row).status();
    std::cout << "1284352323 seconds after the epoch is " << std::get<0>(*row)
              << "\n";
  }
}
// [END spanner_postgresql_functions]

// [START spanner_postgresql_interleaved_table]
void InterleavedTable(google::cloud::spanner_admin::DatabaseAdminClient client,
                      google::cloud::spanner::Database const& database) {
  // The Spanner PostgreSQL dialect extends the PostgreSQL dialect with
  // certain Spanner specific features, such as interleaved tables. See
  // https://cloud.google.com/spanner/docs/postgresql/data-definition-language#create_table
  // for the full CREATE TABLE syntax.
  std::vector<std::string> statements = {
      R"""(
        CREATE TABLE Singers (
            SingerId        BIGINT NOT NULL,
            FirstName       CHARACTER VARYING(1024) NOT NULL,
            LastName        CHARACTER VARYING(1024) NOT NULL,
            PRIMARY KEY(SingerId)
        )
      )""",
      R"""(
        CREATE TABLE Albums (
            SingerId        BIGINT NOT NULL,
            AlbumId         BIGINT NOT NULL,
            AlbumTitle      CHARACTER VARYING NOT NULL,
            MarketingBudget BIGINT,
            PRIMARY KEY(SingerId, AlbumId)
        ) INTERLEAVE IN PARENT Singers ON DELETE CASCADE
      )""",
  };
  auto metadata =
      client.UpdateDatabaseDdl(database.FullName(), statements).get();
  google::cloud::spanner_testing::LogUpdateDatabaseDdl(  //! TODO(#4758)
      client, database, metadata.status());              //! TODO(#4758)
  if (!metadata) throw std::move(metadata).status();
  std::cout << "Tables created.\nNew DDL:\n" << metadata->DebugString();
}
// [END spanner_postgresql_interleaved_table]

// [START spanner_postgresql_create_storing_index]
void CreateStoringIndex(
    google::cloud::spanner_admin::DatabaseAdminClient client,
    google::cloud::spanner::Database const& database) {
  std::vector<std::string> statements = {
      R"""(
        CREATE INDEX AlbumsByAlbumTitle
            ON Albums (AlbumTitle NULLS FIRST)
            INCLUDE (MarketingBudget)
      )""",
  };
  auto metadata =
      client.UpdateDatabaseDdl(database.FullName(), statements).get();
  google::cloud::spanner_testing::LogUpdateDatabaseDdl(  //! TODO(#4758)
      client, database, metadata.status());              //! TODO(#4758)
  if (!metadata) throw std::move(metadata).status();
  std::cout << "Index added.\nNew DDL:\n" << metadata->DebugString();
}
// [END spanner_postgresql_create_storing_index]

// [START spanner_postgresql_information_schema]
void InformationSchema(
    google::cloud::spanner_admin::DatabaseAdminClient admin_client,
    google::cloud::spanner::Database const& database,
    google::cloud::spanner::Client client) {
  std::vector<std::string> statements = {
      R"""(
        CREATE TABLE Venues (
            VenueId  BIGINT NOT NULL PRIMARY KEY,
            Name     CHARACTER VARYING(1024) NOT NULL,
            Revenue  NUMERIC,
            Picture  BYTEA
        )
      )""",
  };
  auto metadata =
      admin_client.UpdateDatabaseDdl(database.FullName(), statements).get();
  google::cloud::spanner_testing::LogUpdateDatabaseDdl(  //! TODO(#4758)
      admin_client, database, metadata.status());        //! TODO(#4758)
  if (!metadata) throw std::move(metadata).status();
  std::cout << "Table created.\nNew DDL:\n" << metadata->DebugString();

  // Get all the user tables in the database. PostgreSQL uses the `public`
  // schema for user tables. The table_catalog is equal to the database name.
  // The `user_defined_` columns are only available for PostgreSQL databases.
  auto sql = google::cloud::spanner::SqlStatement(R"""(
      SELECT table_catalog, table_schema, table_name,
             user_defined_type_catalog,
             user_defined_type_schema,
             user_defined_type_name
          FROM INFORMATION_SCHEMA.tables
          WHERE table_schema = 'public'
  )""");
  using RowType =
      std::tuple<absl::optional<std::string>, std::string, std::string,
                 absl::optional<std::string>, absl::optional<std::string>,
                 absl::optional<std::string>>;
  auto rows = client.ExecuteQuery(std::move(sql));
  for (auto& row : google::cloud::spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::move(row).status();
    std::string user_defined_type = "null";
    if (std::get<3>(*row).has_value()) {
      user_defined_type = std::get<3>(*row).value() + "." +
                          std::get<4>(*row).value() + "." +
                          std::get<5>(*row).value();
    }
    std::cout << "Table: " << std::get<2>(*row)
              << " (User defined type: " << user_defined_type << ")\n";
  }
}
// [END spanner_postgresql_information_schema]

// [START spanner_postgresql_numeric_data_type]
void NumericDataType(google::cloud::spanner::Client client) {
  // Insert a Venue with a valid value for the Revenue column.
  google::cloud::spanner::DmlResult dml_result;
  auto commit = client.Commit(
      [&client, &dml_result](google::cloud::spanner::Transaction txn)
          -> google::cloud::StatusOr<google::cloud::spanner::Mutations> {
        auto sql = google::cloud::spanner::SqlStatement(
            R"""(
                INSERT INTO Venues (VenueId, Name, Revenue)
                    VALUES ($1, $2, $3)
            )""",
            {{"p1", google::cloud::spanner::Value(1)},
             {"p2", google::cloud::spanner::Value("Venue 1")},
             {"p3",
              google::cloud::spanner::Value(
                  google::cloud::spanner::MakePgNumeric("3150.25").value())}});
        auto insert = client.ExecuteDml(std::move(txn), std::move(sql));
        if (!insert) return std::move(insert).status();
        dml_result = *std::move(insert);
        return google::cloud::spanner::Mutations{};
      });
  if (!commit) throw std::move(commit).status();
  std::cout << "Inserted " << dml_result.RowsModified() << " venue(s).\n";

  // Insert a Venue with a NULL value for the Revenue column.
  commit = client.Commit(
      [&client, &dml_result](google::cloud::spanner::Transaction txn)
          -> google::cloud::StatusOr<google::cloud::spanner::Mutations> {
        auto sql = google::cloud::spanner::SqlStatement(
            R"""(
                INSERT INTO Venues (VenueId, Name, Revenue)
                    VALUES ($1, $2, $3)
            )""",
            {{"p1", google::cloud::spanner::Value(2)},
             {"p2", google::cloud::spanner::Value("Venue 2")},
             {"p3", google::cloud::spanner::MakeNullValue<
                        google::cloud::spanner::PgNumeric>()}});
        auto insert = client.ExecuteDml(std::move(txn), std::move(sql));
        if (!insert) return std::move(insert).status();
        dml_result = *std::move(insert);
        return google::cloud::spanner::Mutations{};
      });
  if (!commit) throw std::move(commit).status();
  std::cout << "Inserted " << dml_result.RowsModified() << " venue(s)"
            << " with NULL revenue.\n";

  // Insert a Venue with a NaN value for the Revenue column.
  commit = client.Commit(
      [&client, &dml_result](google::cloud::spanner::Transaction txn)
          -> google::cloud::StatusOr<google::cloud::spanner::Mutations> {
        auto sql = google::cloud::spanner::SqlStatement(
            R"""(
                INSERT INTO Venues (VenueId, Name, Revenue)
                    VALUES ($1, $2, $3)
            )""",
            {{"p1", google::cloud::spanner::Value(3)},
             {"p2", google::cloud::spanner::Value("Venue 3")},
             {"p3",
              google::cloud::spanner::Value(
                  google::cloud::spanner::MakePgNumeric("NaN").value())}});
        auto insert = client.ExecuteDml(std::move(txn), std::move(sql));
        if (!insert) return std::move(insert).status();
        dml_result = *std::move(insert);
        return google::cloud::spanner::Mutations{};
      });
  if (!commit) throw std::move(commit).status();
  std::cout << "Inserted " << dml_result.RowsModified() << " venue(s)"
            << " with NaN revenue.\n";

  // Mutations can also be used to insert/update values, including NaNs.
  auto insert_venues =  //
      google::cloud::spanner::InsertMutationBuilder(
          "Venues", {"VenueId", "Name", "Revenue"})
          .EmplaceRow(4, "Venue 4",
                      google::cloud::spanner::MakePgNumeric("125.10").value())
          .EmplaceRow(5, "Venue 5",
                      google::cloud::spanner::MakePgNumeric("NaN").value())
          .Build();
  commit = client.Commit(google::cloud::spanner::Mutations{insert_venues});
  if (!commit) throw std::move(commit).status();
  std::cout << "Inserted 2 venues using mutations at "
            << commit->commit_timestamp << ".\n";

  // Get all Venues and inspect the Revenue values.
  auto sql = google::cloud::spanner::SqlStatement(R"""(
      SELECT Name, Revenue FROM Venues
  )""");
  using RowType = std::tuple<std::string,
                             absl::optional<google::cloud::spanner::PgNumeric>>;
  auto rows = client.ExecuteQuery(std::move(sql));
  for (auto& row : google::cloud::spanner::StreamOf<RowType>(rows)) {
    if (!row) throw std::move(row).status();
    std::cout << "Revenue of " << std::get<0>(*row) << ": ";
    if (std::get<1>(*row).has_value()) {
      std::cout << *std::get<1>(*row);
    } else {
      std::cout << "NULL";
    }
    std::cout << "\n";
  }
}
// [END spanner_postgresql_numeric_data_type]

// [START spanner_postgresql_partitioned_dml]
void PartitionedDml(google::cloud::spanner::Client client) {
  // Spanner PostgreSQL has the same transaction limits as normal
  // Spanner. This includes a maximum of 20,000 mutations in a single
  // read/write transaction. Large update operations can be executed using
  // Partitioned DML. This is also supported on Spanner PostgreSQL. See
  // https://cloud.google.com/spanner/docs/dml-partitioned for information.
  auto sql = google::cloud::spanner::SqlStatement(R"""(
      DELETE FROM users WHERE active = FALSE
  )""");
  auto result = client.ExecutePartitionedDml(std::move(sql));
  if (!result) throw std::move(result).status();
  // The returned count is the lower bound on the number of rows modified.
  std::cout << "Deleted at least " << result->row_count_lower_bound
            << " inactive users\n";
}
// [END spanner_postgresql_partitioned_dml]

void DropDatabase(google::cloud::spanner_admin::DatabaseAdminClient client,
                  google::cloud::spanner::Database const& database) {
  auto status = client.DropDatabase(database.FullName());
  if (!status.ok()) throw std::move(status);
  std::cout << "Database " << database << " dropped.\n";
}

}  // namespace samples

namespace helpers {

void CreateTables(google::cloud::spanner_admin::DatabaseAdminClient client,
                  google::cloud::spanner::Database const& database) {
  std::vector<std::string> statements = {
      R"""(
        CREATE TABLE Singers (
            SingerId   BIGINT NOT NULL,
            FirstName  CHARACTER VARYING(1024),
            LastName   CHARACTER VARYING(1024),
            SingerInfo BYTEA,
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
      R"""(
        CREATE TABLE users (
            user_id    BIGINT NOT NULL,
            user_name  CHARACTER VARYING(1024),
            active     BOOLEAN,
            PRIMARY KEY(user_id)
        )
      )""",
  };
  auto metadata =
      client.UpdateDatabaseDdl(database.FullName(), statements).get();
  google::cloud::spanner_testing::LogUpdateDatabaseDdl(  //! TODO(#4758)
      client, database, metadata.status());              //! TODO(#4758)
  if (!metadata) throw std::move(metadata).status();
  std::cout << "Tables created.\nNew DDL:\n" << metadata->DebugString();
}

void DropTables(google::cloud::spanner_admin::DatabaseAdminClient client,
                google::cloud::spanner::Database const& database) {
  std::vector<std::string> statements = {
      R"""(
        DROP TABLE Albums
      )""",
      R"""(
        DROP TABLE Singers
      )""",
  };
  auto metadata =
      client.UpdateDatabaseDdl(database.FullName(), statements).get();
  google::cloud::spanner_testing::LogUpdateDatabaseDdl(  //! TODO(#4758)
      client, database, metadata.status());              //! TODO(#4758)
  if (!metadata) throw std::move(metadata).status();
  std::cout << "Tables dropped.\nNew DDL:\n" << metadata->DebugString();
}

}  // namespace helpers

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

CommandType Command(void (*sample)(google::cloud::spanner::Database const&)) {
  return [sample](std::vector<std::string> argv) {
    auto database = Database(std::move(argv));
    sample(database);
  };
}

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

CommandType Command(void (*sample)(
    google::cloud::spanner_admin::DatabaseAdminClient,
    google::cloud::spanner::Database const&, google::cloud::spanner::Client)) {
  return [sample](std::vector<std::string> argv) {
    auto database = Database(std::move(argv));
    google::cloud::spanner_admin::DatabaseAdminClient admin_client(
        google::cloud::spanner_admin::MakeDatabaseAdminConnection());
    google::cloud::spanner::Client client(
        google::cloud::spanner::MakeConnection(database));
    sample(std::move(admin_client), database, std::move(client));
  };
}

CommandType HelpCommand(std::map<std::string, CommandType>& commands) {
  return [&commands](std::vector<std::string> const&) {
    std::cout << "Available commands are:\n";
    for (auto const& command : commands) {
      std::cout << "  " << command.first << "\n";
    }
  };
}

int RunOneCommand(std::vector<std::string> argv,
                  std::string const& extra_help) {
  std::map<std::string, CommandType> commands = {
      {"create-clients", Command(samples::CreateClients)},
      {"create-database", Command(samples::CreateDatabase)},
      {"create-tables", Command(helpers::CreateTables)},
      {"add-column", Command(samples::AddColumn)},
      {"insert-data", Command(samples::InsertData)},
      {"query-with-parameter", Command(samples::QueryWithParameter)},
      {"dml-getting-started-update", Command(samples::DmlGettingStartedUpdate)},
      {"batch-dml", Command(samples::BatchDml)},
      {"drop-tables", Command(helpers::DropTables)},
      {"case-sensitivity", Command(samples::CaseSensitivity)},
      {"cast-data-type", Command(samples::CastDataType)},
      {"dml-with-parameters", Command(samples::DmlWithParameters)},
      {"order-nulls", Command(samples::OrderNulls)},
      {"functions", Command(samples::Functions)},
      {"interleaved-table", Command(samples::InterleavedTable)},
      {"create-storing-index", Command(samples::CreateStoringIndex)},
      {"numeric-data-type", Command(samples::NumericDataType)},
      {"information-schema", Command(samples::InformationSchema)},
      {"partitioned-dml", Command(samples::PartitionedDml)},
      {"drop-database", Command(samples::DropDatabase)},
      {"help", HelpCommand(commands)},
  };
  auto it = commands.find(argv[0]);
  if (it == commands.end()) {
    throw std::runtime_error(argv[0] + ": Unknown command" + extra_help);
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

  SampleBanner("spanner_postgresql_create_clients");
  samples::CreateClients(database);

  google::cloud::spanner_admin::DatabaseAdminClient database_admin_client(
      google::cloud::spanner_admin::MakeDatabaseAdminConnection());

  SampleBanner("spanner_postgresql_create_database");
  samples::CreateDatabase(database_admin_client, database);

  try {
    helpers::CreateTables(database_admin_client, database);

    SampleBanner("spanner_postgresql_add_column");
    samples::AddColumn(database_admin_client, database);

    google::cloud::spanner::Client client(
        google::cloud::spanner::MakeConnection(database));

    SampleBanner("spanner_insert_data");
    samples::InsertData(client);

    SampleBanner("spanner_postgresql_query_with_parameter");
    samples::QueryWithParameter(client);

    SampleBanner("spanner_postgresql_dml_getting_started_update");
    samples::DmlGettingStartedUpdate(client);

    SampleBanner("spanner_postgresql_batch_dml");
    samples::BatchDml(client);

    helpers::DropTables(database_admin_client, database);

    SampleBanner("spanner_postgresql_case_sensitivity");
    samples::CaseSensitivity(database_admin_client, database, client);

    SampleBanner("spanner_postgresql_cast_data_type");
    samples::CastDataType(client);

    SampleBanner("spanner_postgresql_dml_with_parameters");
    samples::DmlWithParameters(client);

    SampleBanner("spanner_postgresql_order_nulls");
    samples::OrderNulls(client);

    SampleBanner("spanner_postgresql_functions");
    samples::Functions(client);

    helpers::DropTables(database_admin_client, database);

    SampleBanner("spanner_postgresql_interleaved_table");
    samples::InterleavedTable(database_admin_client, database);

    SampleBanner("spanner_postgresql_create_storing_index");
    samples::CreateStoringIndex(database_admin_client, database);

    SampleBanner("spanner_postgresql_information_schema");
    samples::InformationSchema(database_admin_client, database, client);

    SampleBanner("spanner_postgresql_numeric_data_type");
    samples::NumericDataType(client);

    SampleBanner("spanner_postgresql_partitioned_dml");
    samples::PartitionedDml(client);
  } catch (...) {
    // Try to clean up after a failure.
    samples::DropDatabase(database_admin_client, database);
    throw;
  }

  SampleBanner("spanner_drop_database");
  samples::DropDatabase(database_admin_client, database);

  return 0;
}

}  // namespace

int main(int ac, char* av[]) try {
  auto const* const emulator_host = "SPANNER_EMULATOR_HOST";
  if (google::cloud::internal::GetEnv(emulator_host).has_value()) {
    return 0;  // emulator does not support PostgreSQL dialect
  }
  auto const* const auto_run = "GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES";
  if (google::cloud::internal::GetEnv(auto_run).value_or("") == "yes") {
    return RunAll();
  }
  std::string program(ac ? (ac--, *av++) : "pg_samples");
  auto extra_help =
      "\nUse \"" + program + " help\" to list the available commands.";
  if (ac == 0) {
    throw std::runtime_error("Usage: " + program +
                             " <command> [<argument> ...]" + extra_help);
  }
  return RunOneCommand({av, av + ac}, extra_help);
} catch (google::cloud::Status const& status) {
  std::cerr << "\n" << status << "\n";
  google::cloud::LogSink::Instance().Flush();
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "\n" << ex.what() << "\n";
  return 1;
}
