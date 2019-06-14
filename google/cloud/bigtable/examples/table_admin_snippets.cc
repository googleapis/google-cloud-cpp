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

//! [all code]

//! [bigtable includes]
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
//! [bigtable includes]
#include <google/protobuf/text_format.h>
#include <deque>
#include <list>
#include <sstream>

namespace {
struct Usage {
  std::string msg;
};

char const* ConsumeArg(int& argc, char* argv[]) {
  if (argc < 2) {
    return nullptr;
  }
  char const* result = argv[1];
  std::copy(argv + 2, argv + argc, argv + 1);
  argc--;
  return result;
}

std::string command_usage;

void PrintUsage(int, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void CreateTable(google::cloud::bigtable::TableAdmin admin, int argc,
                 char* argv[]) {
  if (argc != 2) {
    throw Usage{"create-table <project-id> <instance-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  //! [create table] [START bigtable_create_table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id) {
    StatusOr<google::bigtable::admin::v2::Table> schema = admin.CreateTable(
        table_id,
        cbt::TableConfig({{"fam", cbt::GcRule::MaxNumVersions(10)},
                          {"foo", cbt::GcRule::MaxAge(std::chrono::hours(72))}},
                         {}));
    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    std::cout << "Table successfully created: " << schema->DebugString()
              << "\n";
  }
  //! [create table] [END bigtable_create_table]
  (std::move(admin), table_id);
}

void ListTables(google::cloud::bigtable::TableAdmin admin, int argc, char*[]) {
  if (argc != 1) {
    throw Usage{"list-tables <project-id> <instance-id>"};
  }

  //! [list tables] [START bigtable_list_tables]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin) {
    StatusOr<std::vector<google::bigtable::admin::v2::Table>> tables =
        admin.ListTables(cbt::TableAdmin::NAME_ONLY);

    if (!tables) {
      throw std::runtime_error(tables.status().message());
    }
    for (auto const& table : *tables) {
      std::cout << table.name() << "\n";
    }
  }
  //! [list tables] [END bigtable_list_tables]
  (std::move(admin));
}

void GetTable(google::cloud::bigtable::TableAdmin admin, int argc,
              char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-table <project-id> <instance-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  //! [get table] [START bigtable_get_table_metadata]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id) {
    StatusOr<google::bigtable::admin::v2::Table> table =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);
    if (!table) {
      throw std::runtime_error(table.status().message());
    }
    std::cout << table->name() << " details=\n" << table->DebugString() << "\n";
  }
  //! [get table] [END bigtable_get_table_metadata]
  (std::move(admin), table_id);
}

void CheckTableExists(google::cloud::bigtable::TableAdmin admin, int argc,
                      char* argv[]) {
  if (argc != 2) {
    throw Usage{"check-table-exists <project-id> <instance-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  //! [START bigtable_check_table_exists]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id) {
    StatusOr<google::bigtable::admin::v2::Table> table =
        admin.GetTable(table_id, cbt::TableAdmin::NAME_ONLY);
    if (!table) {
      if (table.status().code() == google::cloud::StatusCode::kNotFound) {
        throw std::runtime_error("Table " + table_id + " does not exist");
      }
      throw std::runtime_error(table.status().message());
    }

    std::cout << "Table " << table->name() << " was found\n";
  }
  //! [END bigtable_check_table_exists]
  (std::move(admin), table_id);
}

void GetOrCreateTable(google::cloud::bigtable::TableAdmin admin, int argc,
                      char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-or-create-table <project-id> <instance-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  // [START bigtable_get_or_create_table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id) {
    StatusOr<google::bigtable::admin::v2::Table> table =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);
    if (!table &&
        table.status().code() == google::cloud::StatusCode::kNotFound) {
      // The table does not exist, try to create the table.
      table = admin.CreateTable(
          table_id,
          cbt::TableConfig({{"fam", cbt::GcRule::MaxNumVersions(10)}}, {}));
      if (!table) {
        throw std::runtime_error(table.status().message());
      }
      // The schema returned by a `CreateTable()` request does not include all
      // the metadata for a table, we need to explicitly request the rest:
      table = admin.GetTable(table_id, cbt::TableAdmin::FULL);
    }
    if (!table) {
      throw std::runtime_error(table.status().message());
    }
    std::cout << "Table metadata: " << table->DebugString() << "\n";
  }
  // [END bigtable_get_or_create_table]
  (std::move(admin), table_id);
}

void DeleteTable(google::cloud::bigtable::TableAdmin admin, int argc,
                 char* argv[]) {
  if (argc != 2) {
    throw Usage{"delete-table <project-id> <instance-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  //! [delete table] [START bigtable_delete_table]
  namespace cbt = google::cloud::bigtable;
  [](cbt::TableAdmin admin, std::string table_id) {
    google::cloud::Status status = admin.DeleteTable(table_id);
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "Table successfully deleted\n";
  }
  //! [delete table] [END bigtable_delete_table]
  (std::move(admin), table_id);
}

void ModifyTable(google::cloud::bigtable::TableAdmin admin, int argc,
                 char* argv[]) {
  if (argc != 2) {
    throw Usage{"modify-table <project-id> <instance-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  //! [modify table]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id) {
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

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  //! [modify table]
  (std::move(admin), table_id);
}

void CreateMaxAgeFamily(google::cloud::bigtable::TableAdmin admin, int argc,
                        char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "create-max-age-family <project-id> <instance-id> <table-id>"
        " <family-name>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);
  std::string const family_name = ConsumeArg(argc, argv);

  // [START bigtable_create_family_gc_max_age]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id,
            {cbt::ColumnFamilyModification::Create(
                family_name, cbt::GcRule::MaxAge(std::chrono::hours(5 * 24)))});

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_max_age]
  (std::move(admin), table_id, family_name);
}

void CreateMaxVersionsFamily(google::cloud::bigtable::TableAdmin admin,
                             int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "create-max-versions-family <project-id> <instance-id> <table-id>"
        " <family-name>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);
  std::string const family_name = ConsumeArg(argc, argv);

  // [START bigtable_create_family_gc_max_versions]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id, {cbt::ColumnFamilyModification::Create(
                          family_name, cbt::GcRule::MaxNumVersions(2))});

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_max_versions]
  (std::move(admin), table_id, family_name);
}

void CreateUnionFamily(google::cloud::bigtable::TableAdmin admin, int argc,
                       char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "create-union-family <project-id> <instance-id> <table-id>"
        " <family-name>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);
  std::string const family_name = ConsumeArg(argc, argv);

  // [START bigtable_create_family_gc_union]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id,
            {cbt::ColumnFamilyModification::Create(
                family_name,
                cbt::GcRule::Union(
                    cbt::GcRule::MaxNumVersions(1),
                    cbt::GcRule::MaxAge(5 * std::chrono::hours(24))))});

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_union]
  (std::move(admin), table_id, family_name);
}

void CreateIntersectionFamily(google::cloud::bigtable::TableAdmin admin,
                              int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "create-intersection-family <project-id> <instance-id> <table-id>"
        " <family-name>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);
  std::string const family_name = ConsumeArg(argc, argv);

  // [START bigtable_create_family_gc_intersection]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id,
            {cbt::ColumnFamilyModification::Create(
                family_name,
                cbt::GcRule::Intersection(
                    cbt::GcRule::MaxNumVersions(1),
                    cbt::GcRule::MaxAge(5 * std::chrono::hours(24))))});

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_intersection]
  (std::move(admin), table_id, family_name);
}

void CreateNestedFamily(google::cloud::bigtable::TableAdmin admin, int argc,
                        char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "create-nested-family <project-id> <instance-id> <table-id>"
        " <family-name>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);
  std::string const family_name = ConsumeArg(argc, argv);

  // [START bigtable_create_family_gc_nested]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string family_name) {
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

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_nested]
  (std::move(admin), table_id, family_name);
}

void GetFamilyMetadata(google::cloud::bigtable::TableAdmin admin, int argc,
                       char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "get-family-metadata <project-id> <instance-id> <table-id>"
        " <family-name>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);
  std::string const family_name = ConsumeArg(argc, argv);

  // [START bigtable_get_family_metadata] [START bigtable_get_family]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    auto pos = schema->column_families().find(family_name);
    if (pos == schema->column_families().end()) {
      std::cout << "Cannot find family <" << family_name << "> in table\n";
      return;
    }
    std::cout << "Column family metadata for <" << family_name << "> is "
              << pos->second.DebugString() << "\n";
  }
  // [END bigtable_get_family_metadata] [END bigtable_get_family]
  (std::move(admin), table_id, family_name);
}

void GetOrCreateFamily(google::cloud::bigtable::TableAdmin admin, int argc,
                       char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "get-or-create-family <project-id> <instance-id> <table-id>"
        " <family-name>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);
  std::string const family_name = ConsumeArg(argc, argv);

  // [START bigtable_get_or_create_family]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    auto pos = schema->column_families().find(family_name);
    if (pos == schema->column_families().end()) {
      // Try to create the column family instead:
      auto modified = admin.ModifyColumnFamilies(
          table_id,
          {cbt::ColumnFamilyModification::Create(
              family_name, cbt::GcRule::MaxAge(std::chrono::hours(5 * 24)))});

      if (!modified) {
        throw std::runtime_error(schema.status().message());
      }
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
  (std::move(admin), table_id, family_name);
}

void DeleteColumnFamily(google::cloud::bigtable::TableAdmin admin, int argc,
                        char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "delete-column-family <project-id> <instance-id> <table-id>"
        " <family-name>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);
  std::string const family_name = ConsumeArg(argc, argv);

  // [START bigtable_delete_family]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id, {cbt::ColumnFamilyModification::Drop(family_name)});

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_delete_family]
  (std::move(admin), table_id, family_name);
}

void CheckFamilyExists(google::cloud::bigtable::TableAdmin admin, int argc,
                       char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "check-family-exists <project-id> <instance-id> <table-id>"
        " <family-name>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);
  std::string const family_name = ConsumeArg(argc, argv);

  // [START bigtable_check_family_exists]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    auto pos = schema->column_families().find(family_name);
    if (pos == schema->column_families().end()) {
      std::string msg =
          "The column family <" + family_name + "> does not exist";
      throw std::runtime_error(msg);
    }
    std::cout << "The column family <" << family_name << "> does exist\n";
  }
  // [END bigtable_check_family_exists]
  (std::move(admin), table_id, family_name);
}

void ListColumnFamilies(google::cloud::bigtable::TableAdmin admin, int argc,
                        char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-column-families <project-id> <instance-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  // [START bigtable_list_column_families]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(table_id, cbt::TableAdmin::FULL);

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    for (auto const& kv : schema->column_families()) {
      std::string const& column_family_name = kv.first;
      google::bigtable::admin::v2::ColumnFamily const& family = kv.second;
      std::cout << "Column family name: " << column_family_name << "\n"
                << "Column family metadata: " << family.DebugString() << "\n";
    }
  }
  // [END bigtable_list_column_families]
  (std::move(admin), table_id);
}

void UpdateGcRule(google::cloud::bigtable::TableAdmin admin, int argc,
                  char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "update-gc-rule <project-id> <instance-id> <table-id>"
        " <family-name>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);
  std::string const family_name = ConsumeArg(argc, argv);

  // [START bigtable_update_gc_rule]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id, std::string family_name) {
    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_id, {cbt::ColumnFamilyModification::Update(
                          family_name, cbt::GcRule::MaxNumVersions(1))});

    if (!schema) {
      throw std::runtime_error(schema.status().message());
    }
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_update_gc_rule]
  (std::move(admin), table_id, family_name);
}

void DropAllRows(google::cloud::bigtable::TableAdmin admin, int argc,
                 char* argv[]) {
  if (argc != 2) {
    throw Usage{"drop-all-rows <project-id> <instance-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  //! [drop all rows]
  // [START bigtable_truncate_table] [START bigtable_delete_rows]
  namespace cbt = google::cloud::bigtable;
  [](cbt::TableAdmin admin, std::string table_id) {
    google::cloud::Status status = admin.DropAllRows(table_id);
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "All rows successfully deleted\n";
  }
  // [END bigtable_truncate_table] [END bigtable_delete_rows]
  //! [drop all rows]
  (std::move(admin), table_id);
}

void DropRowsByPrefix(google::cloud::bigtable::TableAdmin admin, int argc,
                      char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "drop-rows-by-prefix <project-id> <instance-id> <table-id>"
        " <prefix>"};
  }
  auto table_id = ConsumeArg(argc, argv);
  auto prefix = ConsumeArg(argc, argv);

  //! [drop rows by prefix] [START bigtable_delete_rows_prefix]
  namespace cbt = google::cloud::bigtable;
  [](cbt::TableAdmin admin, std::string table_id, std::string prefix) {
    google::cloud::Status status = admin.DropRowsByPrefix(table_id, prefix);
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "All rows starting with " << prefix
              << " successfully deleted\n";
  }
  //! [drop rows by prefix] [END bigtable_delete_rows_prefix]
  (std::move(admin), table_id, prefix);
}

void WaitForConsistencyCheck(google::cloud::bigtable::TableAdmin admin,
                             int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{
        "wait-for-consistency-check <project-id> <instance-id> <table-id>"};
  }
  auto table_id = ConsumeArg(argc, argv);

  //! [wait for consistency check]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id) {
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
  (std::move(admin), table_id);
}

void CheckConsistency(google::cloud::bigtable::TableAdmin admin, int argc,
                      char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "check-consistency <project-id> <instance-id> <table-id> "
        "<consistency_token>"};
  }
  auto table_id = ConsumeArg(argc, argv);
  auto consistency_token = ConsumeArg(argc, argv);

  //! [check consistency]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id,
     std::string consistency_token) {
    StatusOr<cbt::Consistency> result =
        admin.CheckConsistency(table_id, consistency_token);
    if (!result) {
      throw std::runtime_error(result.status().message());
    }
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
  (std::move(admin), table_id, consistency_token);
}

void GenerateConsistencyToken(google::cloud::bigtable::TableAdmin admin,
                              int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{
        "generate-consistency-token <project-id> <instance-id> <table-id>"};
  }
  std::string const table_id = ConsumeArg(argc, argv);

  //! [generate consistency token]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string table_id) {
    StatusOr<std::string> token = admin.GenerateConsistencyToken(table_id);
    if (!token) {
      throw std::runtime_error(token.status().message());
    }
    std::cout << "generated token is : " << *token << "\n";
  }
  //! [generate consistency token]
  (std::move(admin), table_id);
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType =
      std::function<void(google::cloud::bigtable::TableAdmin, int, char*[])>;

  std::map<std::string, CommandType> commands = {
      {"create-table", &CreateTable},
      {"list-tables", &ListTables},
      {"get-table", &GetTable},
      {"check-table-exists", &CheckTableExists},
      {"get-or-create-table", &GetOrCreateTable},
      {"delete-table", &DeleteTable},
      {"modify-table", &ModifyTable},
      {"create-max-age-family", &CreateMaxAgeFamily},
      {"create-max-versions-family", &CreateMaxVersionsFamily},
      {"create-union-family", &CreateUnionFamily},
      {"create-intersection-family", &CreateIntersectionFamily},
      {"create-nested-family", &CreateNestedFamily},
      {"get-family-metadata", &GetFamilyMetadata},
      {"get-or-create-family", &GetOrCreateFamily},
      {"delete-column-family", &DeleteColumnFamily},
      {"check-family-exists", &CheckFamilyExists},
      {"list-column-families", &ListColumnFamilies},
      {"update-gc-rule", &UpdateGcRule},
      {"drop-all-rows", &DropAllRows},
      {"drop-rows-by-prefix", &DropRowsByPrefix},
      {"wait-for-consistency-check", &WaitForConsistencyCheck},
      {"check-consistency", &CheckConsistency},
      {"generate-consistency-token", &GenerateConsistencyToken},
  };

  {
    // Force each command to generate its Usage string, so we can provide a good
    // usage string for the whole program. We need to create an TableAdmin
    // object to do this, but that object is never used, it is passed to the
    // commands, without any calls made to it.
    google::cloud::bigtable::TableAdmin unused(
        google::cloud::bigtable::CreateDefaultAdminClient(
            "unused-project", google::cloud::bigtable::ClientOptions()),
        "Unused-instance");
    for (auto&& kv : commands) {
      try {
        int fake_argc = 0;
        kv.second(unused, fake_argc, argv);
      } catch (Usage const& u) {
        command_usage += "    ";
        command_usage += u.msg;
        command_usage += "\n";
      } catch (...) {
        // ignore other exceptions.
      }
    }
  }

  if (argc < 4) {
    PrintUsage(argc, argv, "Missing command and/or project-id/ or instance-id");
    return 1;
  }

  std::string const command_name = ConsumeArg(argc, argv);
  std::string const project_id = ConsumeArg(argc, argv);
  std::string const instance_id = ConsumeArg(argc, argv);

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    PrintUsage(argc, argv, "Unknown command: " + command_name);
    return 1;
  }

  // Connect to the Cloud Bigtable admin endpoint.
  //! [connect admin]
  google::cloud::bigtable::TableAdmin admin(
      google::cloud::bigtable::CreateDefaultAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()),
      instance_id);
  //! [connect admin]

  command->second(admin, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
//! [all code]
