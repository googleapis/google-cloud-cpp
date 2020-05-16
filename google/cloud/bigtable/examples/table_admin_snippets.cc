// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/crash_handler.h"
#include <sstream>

namespace {

using google::cloud::bigtable::examples::Usage;

void CreateTable(google::cloud::bigtable::TableAdmin const& admin,
                 std::vector<std::string> const& argv) {
  //! [create table] [START bigtable_create_table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    StatusOr<google::bigtable::admin::v2::Table> schema = admin.CreateTable(
        table_id,
        cbt::TableConfig({{"fam", cbt::GcRule::MaxNumVersions(10)},
                          {"foo", cbt::GcRule::MaxAge(std::chrono::hours(72))}},
                         {}));
    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Table successfully created: " << schema->DebugString()
              << "\n";
  }
  //! [create table] [END bigtable_create_table]
  (std::move(admin), argv.at(0));
}

void ListTables(google::cloud::bigtable::TableAdmin const& admin,
                std::vector<std::string> const&) {
  //! [list tables] [START bigtable_list_tables]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin) {
    StatusOr<std::vector<google::bigtable::admin::v2::Table>> tables =
        admin.ListTables(cbt::TableAdmin::NAME_ONLY);

    if (!tables) throw std::runtime_error(tables.status().message());
    for (auto const& table : *tables) {
      std::cout << table.name() << "\n";
    }
  }
  //! [list tables] [END bigtable_list_tables]
  (std::move(admin));
}

void GetTable(google::cloud::bigtable::TableAdmin const& admin,
              std::vector<std::string> const& argv) {
  //! [get table] [START bigtable_get_table_metadata]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    StatusOr<google::bigtable::admin::v2::Table> table =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);
    if (!table) throw std::runtime_error(table.status().message());
    std::cout << table->name() << " details=\n" << table->DebugString() << "\n";
  }
  //! [get table] [END bigtable_get_table_metadata]
  (std::move(admin), argv.at(0));
}

void CheckTableExists(google::cloud::bigtable::TableAdmin const& admin,
                      std::vector<std::string> const& argv) {
  //! [START bigtable_check_table_exists]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    StatusOr<google::bigtable::admin::v2::Table> table =
        admin.GetTable(table_id, cbt::TableAdmin::NAME_ONLY);
    if (!table) {
      if (table.status().code() == google::cloud::StatusCode::kNotFound) {
        std::cout << "Table " << table_id << " does not exist\n";
        return;
      }
      throw std::runtime_error(table.status().message());
    }

    std::cout << "Table " << table_id << " was found\n";
  }
  //! [END bigtable_check_table_exists]
  (std::move(admin), argv.at(0));
}

void GetOrCreateTable(google::cloud::bigtable::TableAdmin const& admin,
                      std::vector<std::string> const& argv) {
  // [START bigtable_get_or_create_table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    StatusOr<google::bigtable::admin::v2::Table> table =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);
    if (!table &&
        table.status().code() == google::cloud::StatusCode::kNotFound) {
      // The table does not exist, try to create the table.
      table = admin.CreateTable(
          table_id,
          cbt::TableConfig({{"fam", cbt::GcRule::MaxNumVersions(10)}}, {}));
      if (!table) throw std::runtime_error(table.status().message());
      // The schema returned by a `CreateTable()` request does not include all
      // the metadata for a table, we need to explicitly request the rest:
      table = admin.GetTable(table_id, cbt::TableAdmin::FULL);
    }
    if (!table) throw std::runtime_error(table.status().message());
    std::cout << "Table metadata: " << table->DebugString() << "\n";
  }
  // [END bigtable_get_or_create_table]
  (std::move(admin), argv.at(0));
}

void DeleteTable(google::cloud::bigtable::TableAdmin const& admin,
                 std::vector<std::string> const& argv) {
  //! [delete table] [START bigtable_delete_table]
  namespace cbt = google::cloud::bigtable;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    google::cloud::Status status = admin.DeleteTable(table_id);
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Table successfully deleted\n";
  }
  //! [delete table] [END bigtable_delete_table]
  (std::move(admin), argv.at(0));
}

void ModifyTable(google::cloud::bigtable::TableAdmin const& admin,
                 std::vector<std::string> const& argv) {
  //! [modify table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id,
            {cbt::ColumnFamilyModification::Drop("foo"),
             cbt::ColumnFamilyModification::Update(
                 "fam", cbt::GcRule::Union(
                            cbt::GcRule::MaxNumVersions(5),
                            cbt::GcRule::MaxAge(std::chrono::hours(24 * 7)))),
             cbt::ColumnFamilyModification::Create(
                 "bar", cbt::GcRule::Intersection(
                            cbt::GcRule::MaxNumVersions(3),
                            cbt::GcRule::MaxAge(std::chrono::hours(72))))});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  //! [modify table]
  (std::move(admin), argv.at(0));
}

void CreateMaxAgeFamily(google::cloud::bigtable::TableAdmin const& admin,
                        std::vector<std::string> const& argv) {
  // [START bigtable_create_family_gc_max_age]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id,
            {cbt::ColumnFamilyModification::Create(
                family_name, cbt::GcRule::MaxAge(std::chrono::hours(5 * 24)))});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_max_age]
  (std::move(admin), argv.at(0), argv.at(1));
}

void CreateMaxVersionsFamily(google::cloud::bigtable::TableAdmin const& admin,
                             std::vector<std::string> const& argv) {
  // [START bigtable_create_family_gc_max_versions]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id, {cbt::ColumnFamilyModification::Create(
                          family_name, cbt::GcRule::MaxNumVersions(2))});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_max_versions]
  (std::move(admin), argv.at(0), argv.at(1));
}

void CreateUnionFamily(google::cloud::bigtable::TableAdmin const& admin,
                       std::vector<std::string> const& argv) {
  // [START bigtable_create_family_gc_union]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id,
            {cbt::ColumnFamilyModification::Create(
                family_name,
                cbt::GcRule::Union(
                    cbt::GcRule::MaxNumVersions(1),
                    cbt::GcRule::MaxAge(5 * std::chrono::hours(24))))});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_union]
  (std::move(admin), argv.at(0), argv.at(1));
}

void CreateIntersectionFamily(google::cloud::bigtable::TableAdmin const& admin,
                              std::vector<std::string> const& argv) {
  // [START bigtable_create_family_gc_intersection]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id,
            {cbt::ColumnFamilyModification::Create(
                family_name,
                cbt::GcRule::Intersection(
                    cbt::GcRule::MaxNumVersions(1),
                    cbt::GcRule::MaxAge(5 * std::chrono::hours(24))))});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_intersection]
  (std::move(admin), argv.at(0), argv.at(1));
}

void CreateNestedFamily(google::cloud::bigtable::TableAdmin const& admin,
                        std::vector<std::string> const& argv) {
  // [START bigtable_create_family_gc_nested]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id,
            {cbt::ColumnFamilyModification::Create(
                family_name,
                cbt::GcRule::Union(
                    cbt::GcRule::MaxNumVersions(10),
                    cbt::GcRule::Intersection(
                        cbt::GcRule::MaxNumVersions(1),
                        cbt::GcRule::MaxAge(5 * std::chrono::hours(24)))))});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_nested]
  (std::move(admin), argv.at(0), argv.at(1));
}

void GetFamilyMetadata(google::cloud::bigtable::TableAdmin const& admin,
                       std::vector<std::string> const& argv) {
  // [START bigtable_get_family_metadata] [START bigtable_get_family]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);

    if (!schema) throw std::runtime_error(schema.status().message());
    auto pos = schema->column_families().find(family_name);
    if (pos == schema->column_families().end()) {
      std::cout << "Cannot find family <" << family_name << "> in table\n";
      return;
    }
    std::cout << "Column family metadata for <" << family_name << "> is "
              << pos->second.DebugString() << "\n";
  }
  // [END bigtable_get_family_metadata] [END bigtable_get_family]
  (std::move(admin), argv.at(0), argv.at(1));
}

void GetOrCreateFamily(google::cloud::bigtable::TableAdmin const& admin,
                       std::vector<std::string> const& argv) {
  // [START bigtable_get_or_create_family]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);

    if (!schema) throw std::runtime_error(schema.status().message());
    auto pos = schema->column_families().find(family_name);
    if (pos == schema->column_families().end()) {
      // Try to create the column family instead:
      auto modified = admin.ModifyColumnFamilies(
          table_id,
          {cbt::ColumnFamilyModification::Create(
              family_name, cbt::GcRule::MaxAge(std::chrono::hours(5 * 24)))});

      if (!modified) throw std::runtime_error(schema.status().message());
      schema = *std::move(modified);
      pos = schema->column_families().find(family_name);
    }

    if (pos == schema->column_families().end()) {
      throw std::runtime_error("GetOrCreateFamily failed");
    }

    google::bigtable::admin::v2::ColumnFamily family = pos->second;
    std::cout << "Column family name: " << pos->first
              << "\nColumn family details: " << family.DebugString() << "\n";
  }
  // [END bigtable_get_or_create_family]
  (std::move(admin), argv.at(0), argv.at(1));
}

void DeleteColumnFamily(google::cloud::bigtable::TableAdmin const& admin,
                        std::vector<std::string> const& argv) {
  // [START bigtable_delete_family]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id, {cbt::ColumnFamilyModification::Drop(family_name)});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_delete_family]
  (std::move(admin), argv.at(0), argv.at(1));
}

void CheckFamilyExists(google::cloud::bigtable::TableAdmin const& admin,
                       std::vector<std::string> const& argv) {
  // [START bigtable_check_family_exists]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);

    if (!schema) throw std::runtime_error(schema.status().message());
    auto pos = schema->column_families().find(family_name);
    if (pos == schema->column_families().end()) {
      std::cout << "The column family <" << family_name << "> does not exist";
      return;
    }
    std::cout << "The column family <" << family_name << "> does exist\n";
  }
  // [END bigtable_check_family_exists]
  (std::move(admin), argv.at(0), argv.at(1));
}

void ListColumnFamilies(google::cloud::bigtable::TableAdmin const& admin,
                        std::vector<std::string> const& argv) {
  // [START bigtable_list_column_families]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);

    if (!schema) throw std::runtime_error(schema.status().message());
    for (auto const& kv : schema->column_families()) {
      std::string const& column_family_name = kv.first;
      google::bigtable::admin::v2::ColumnFamily const& family = kv.second;
      std::cout << "Column family name: " << column_family_name << "\n"
                << "Column family metadata: " << family.DebugString() << "\n";
    }
  }
  // [END bigtable_list_column_families]
  (std::move(admin), argv.at(0));
}

void UpdateGcRule(google::cloud::bigtable::TableAdmin const& admin,
                  std::vector<std::string> const& argv) {
  // [START bigtable_update_gc_rule]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id, {cbt::ColumnFamilyModification::Update(
                          family_name, cbt::GcRule::MaxNumVersions(1))});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_update_gc_rule]
  (std::move(admin), argv.at(0), argv.at(1));
}

void DropAllRows(google::cloud::bigtable::TableAdmin const& admin,
                 std::vector<std::string> const& argv) {
  //! [drop all rows]
  // [START bigtable_truncate_table] [START bigtable_delete_rows]
  namespace cbt = google::cloud::bigtable;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    google::cloud::Status status = admin.DropAllRows(table_id);
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "All rows successfully deleted\n";
  }
  // [END bigtable_truncate_table] [END bigtable_delete_rows]
  //! [drop all rows]
  (std::move(admin), argv.at(0));
}

void DropRowsByPrefix(google::cloud::bigtable::TableAdmin const& admin,
                      std::vector<std::string> const& argv) {
  //! [drop rows by prefix] [START bigtable_delete_rows_prefix]
  namespace cbt = google::cloud::bigtable;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& prefix) {
    google::cloud::Status status = admin.DropRowsByPrefix(table_id, prefix);
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "All rows starting with " << prefix
              << " successfully deleted\n";
  }
  //! [drop rows by prefix] [END bigtable_delete_rows_prefix]
  (std::move(admin), argv.at(0), argv.at(1));
}

void WaitForConsistencyCheck(google::cloud::bigtable::TableAdmin const& admin,
                             std::vector<std::string> const& argv) {
  //! [wait for consistency check]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    StatusOr<std::string> consistency_token =
        admin.GenerateConsistencyToken(table_id);
    if (!consistency_token) {
      throw std::runtime_error(consistency_token.status().message());
    }
    future<StatusOr<cbt::Consistency>> consistent_future =
        admin.WaitForConsistency(table_id, *consistency_token);
    auto final = consistent_future.then(
        [&consistency_token](future<StatusOr<cbt::Consistency>> f) {
          auto is_consistent = f.get();
          if (!is_consistent) {
            throw std::runtime_error(is_consistent.status().message());
          }
          if (*is_consistent == cbt::Consistency::kConsistent) {
            std::cout << "Table is consistent with token " << *consistency_token
                      << "\n";
          } else {
            std::cout
                << "Table is not yet consistent, Please try again later with"
                << " the same token (" << *consistency_token << ")\n";
          }
        });
    final.get();  // simplify example by blocking until operation is done.
  }
  //! [wait for consistency check]
  (std::move(admin), argv.at(0));
}

void CheckConsistency(google::cloud::bigtable::TableAdmin const& admin,
                      std::vector<std::string> const& argv) {
  //! [check consistency]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id,
     std::string const& consistency_token) {
    StatusOr<cbt::Consistency> result =
        admin.CheckConsistency(table_id, consistency_token);
    if (!result) throw std::runtime_error(result.status().message());
    if (*result == cbt::Consistency::kConsistent) {
      std::cout << "Table is consistent with token " << consistency_token
                << "\n";
    } else {
      std::cout
          << "Table is not yet consistent, Please try again later with the"
          << " same token (" << consistency_token << ")\n";
    }
  }
  //! [check consistency]
  (std::move(admin), argv.at(0), argv.at(1));
}

void GenerateConsistencyToken(google::cloud::bigtable::TableAdmin const& admin,
                              std::vector<std::string> const& argv) {
  //! [generate consistency token]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    StatusOr<std::string> token = admin.GenerateConsistencyToken(table_id);
    if (!token) throw std::runtime_error(token.status().message());
    std::cout << "generated token is : " << *token << "\n";
  }
  //! [generate consistency token]
  (std::move(admin), argv.at(0));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = google::cloud::bigtable;

  if (!argv.empty()) throw examples::Usage{"auto"};
  if (!examples::RunAdminIntegrationTests()) return;
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const instance_id = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID")
                               .value();

  cbt::TableAdmin admin(
      cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions{}),
      instance_id);

  // If a previous run of these samples crashes before cleaning up there may be
  // old tables left over. As there are quotas on the total number of tables we
  // remove stale tables after 48 hours.
  std::cout << "\nCleaning up old tables" << std::endl;
  std::string const prefix = "table-admin-snippets-";
  examples::CleanupOldTables(prefix, admin);

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  // This table is actually created and used to test the positive case (e.g.
  // GetTable() and "table does exist")
  auto table_id_1 = examples::RandomTableId(prefix, generator);
  // This table does not exist and used to test the negative case (e.g.
  // GetTable() but "table does not exist")
  auto table_id_2 = examples::RandomTableId(prefix, generator);

  auto table_1 = admin.CreateTable(
      table_id_1, cbt::TableConfig(
                      {
                          {"fam", cbt::GcRule::MaxNumVersions(10)},
                          {"foo", cbt::GcRule::MaxNumVersions(3)},
                      },
                      {}));
  if (!table_1) throw std::runtime_error(table_1.status().message());

  std::cout << "\nRunning ListTables() example" << std::endl;
  ListTables(admin, {});

  std::cout << "\nRunning GetTable() example" << std::endl;
  GetTable(admin, {table_id_1});

  std::cout << "\nRunning CheckTableExists() example [1]" << std::endl;
  CheckTableExists(admin, {table_id_1});

  std::cout << "\nRunning CheckTableExists() example [2]" << std::endl;
  CheckTableExists(admin, {table_id_2});

  std::cout << "\nRunning GetOrCreateTable() example [1]" << std::endl;
  GetOrCreateTable(admin, {table_id_1});

  std::cout << "\nRunning GetOrCreateTable() example [2]" << std::endl;
  GetOrCreateTable(admin, {table_id_2});

  std::cout << "\nRunning DeleteTable() example" << std::endl;
  DeleteTable(admin, {table_id_2});

  std::cout << "\nRunning ModifyTable() example" << std::endl;
  ModifyTable(admin, {table_id_1});

  std::cout << "\nRunning CreateMaxAgeFamily() example" << std::endl;
  CreateMaxAgeFamily(admin, {table_id_1, "max-age-family"});

  std::cout << "\nRunning CreateMaxAgeFamily() example" << std::endl;
  CreateMaxVersionsFamily(admin, {table_id_1, "max-versions-family"});

  std::cout << "\nRunning CreateUnionFamily() example" << std::endl;
  CreateUnionFamily(admin, {table_id_1, "union-family"});

  std::cout << "\nRunning CreateIntersectionFamily() example" << std::endl;
  CreateIntersectionFamily(admin, {table_id_1, "intersection-family"});

  std::cout << "\nRunning CreateNestedFamily() example" << std::endl;
  CreateNestedFamily(admin, {table_id_1, "nested-family"});

  std::cout << "\nRunning ListColumnFamilies() example" << std::endl;
  ListColumnFamilies(admin, {table_id_1});

  std::cout << "\nRunning GetFamilyMetadata() example" << std::endl;
  GetFamilyMetadata(admin, {table_id_1, "nested-family"});

  std::cout << "\nRunning GetOrCreateFamily() example" << std::endl;
  GetOrCreateFamily(admin, {table_id_1, "get-or-create-family"});

  std::cout << "\nRunning DeleteColumnFamily() example" << std::endl;
  DeleteColumnFamily(admin, {table_id_1, "nested-family"});

  std::cout << "\nRunning CheckFamilyExists() example [1]" << std::endl;
  CheckFamilyExists(admin, {table_id_1, "nested-family"});

  std::cout << "\nRunning CheckFamilyExists() example [2]" << std::endl;
  CheckFamilyExists(admin, {table_id_1, "max-age-family"});

  std::cout << "\nRunning UpdateGcRule() example" << std::endl;
  UpdateGcRule(admin, {table_id_1, "max-age-family"});

  std::cout << "\nRunning WaitForConsistencyCheck() example" << std::endl;
  WaitForConsistencyCheck(admin, {table_id_1});

  std::cout << "\nRunning GenerateConsistencyToken() example" << std::endl;
  GenerateConsistencyToken(admin, {table_id_1});

  auto token = admin.GenerateConsistencyToken(table_id_1);
  if (!token) throw std::runtime_error(token.status().message());

  std::cout << "\nRunning CheckConsistency() example" << std::endl;
  CheckConsistency(admin, {table_id_1, *token});

  std::cout << "\nRunning DropRowsByPrefix() example" << std::endl;
  DropRowsByPrefix(admin, {table_id_1, "test-prefix/foo/bar/"});

  std::cout << "\nRunning DropAllRows() example" << std::endl;
  DropAllRows(admin, {table_id_1});

  (void)admin.DeleteTable(table_id_1);
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InstallCrashHandler(argv[0]);

  namespace examples = google::cloud::bigtable::examples;
  google::cloud::bigtable::examples::Example example({
      examples::MakeCommandEntry("create-table", {"<table-id>"}, CreateTable),
      examples::MakeCommandEntry("list-tables", {}, ListTables),
      examples::MakeCommandEntry("get-table", {"<table-id>"}, GetTable),
      examples::MakeCommandEntry("check-table-exists", {"<table-id>"},
                                 CheckTableExists),
      examples::MakeCommandEntry("get-or-create-table", {"<table-id>"},
                                 GetOrCreateTable),
      examples::MakeCommandEntry("delete-table", {"<table-id>"}, DeleteTable),
      examples::MakeCommandEntry("modify-table", {"<table-id>"}, ModifyTable),
      examples::MakeCommandEntry("create-max-age-family",
                                 {"<table-id>", "<family-name>"},
                                 CreateMaxAgeFamily),
      examples::MakeCommandEntry("create-max-versions-family",
                                 {"<table-id>", "<family-name>"},
                                 CreateMaxVersionsFamily),
      examples::MakeCommandEntry("create-union-family",
                                 {"<table-id>", "<family-name>"},
                                 CreateUnionFamily),
      examples::MakeCommandEntry("create-intersection-family",
                                 {"<table-id>", "<family-name>"},
                                 CreateIntersectionFamily),
      examples::MakeCommandEntry("create-nested-family",
                                 {"<table-id>", "<family-name>"},
                                 CreateNestedFamily),
      examples::MakeCommandEntry("get-family-metadata",
                                 {"<table-id>", "<family-name>"},
                                 GetFamilyMetadata),
      examples::MakeCommandEntry("get-or-create-family",
                                 {"<table-id>", "<family-name>"},
                                 GetOrCreateFamily),
      examples::MakeCommandEntry("delete-column-family",
                                 {"<table-id>", "<family-name>"},
                                 DeleteColumnFamily),
      examples::MakeCommandEntry("check-family-exists",
                                 {"<table-id>", "<family-name>"},
                                 CheckFamilyExists),
      examples::MakeCommandEntry("list-column-families", {"<table-id>"},
                                 ListColumnFamilies),
      examples::MakeCommandEntry("update-gc-rule",
                                 {"<table-id>", "<family-name>"}, UpdateGcRule),
      examples::MakeCommandEntry("drop-all-rows", {"<table-id>"}, DropAllRows),
      examples::MakeCommandEntry("drop-rows-by-prefix",
                                 {"<table-id>", "<prefix>"}, DropRowsByPrefix),
      examples::MakeCommandEntry("wait-for-consistency-check", {"<table-id>"},
                                 WaitForConsistencyCheck),
      examples::MakeCommandEntry("check-consistency",
                                 {"<table-id>", "<consistency-token>"},
                                 CheckConsistency),
      examples::MakeCommandEntry("generate-consistency-token", {"<table-id>"},
                                 GenerateConsistencyToken),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
