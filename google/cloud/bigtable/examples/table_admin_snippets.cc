// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include <chrono>
#include <sstream>
#include <thread>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void CreateTable(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                 std::vector<std::string> const& argv) {
  //! [create table] [START bigtable_create_table]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    // Example garbage collection rule.
    google::bigtable::admin::v2::GcRule gc;
    gc.set_max_num_versions(10);

    google::bigtable::admin::v2::Table t;
    auto& families = *t.mutable_column_families();
    *families["fam"].mutable_gc_rule() = std::move(gc);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.CreateTable(instance_name, table_id, std::move(t));
    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Table successfully created: " << schema->DebugString()
              << "\n";
  }
  //! [create table] [END bigtable_create_table]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void ListTables(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                std::vector<std::string> const& argv) {
  //! [list tables] [START bigtable_list_tables]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StreamRange;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);

    google::bigtable::admin::v2::ListTablesRequest r;
    r.set_parent(instance_name);
    r.set_view(google::bigtable::admin::v2::Table::NAME_ONLY);

    StreamRange<google::bigtable::admin::v2::Table> tables =
        admin.ListTables(std::move(r));
    for (auto const& table : tables) {
      if (!table) throw std::runtime_error(table.status().message());
      std::cout << table->name() << "\n";
    }
  }
  //! [list tables] [END bigtable_list_tables]
  (std::move(admin), argv.at(0), argv.at(1));
}

void GetTable(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
              std::vector<std::string> const& argv) {
  //! [get table] [START bigtable_get_table_metadata]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    google::bigtable::admin::v2::GetTableRequest r;
    r.set_name(table_name);
    r.set_view(google::bigtable::admin::v2::Table::FULL);

    StatusOr<google::bigtable::admin::v2::Table> table =
        admin.GetTable(std::move(r));
    if (!table) throw std::runtime_error(table.status().message());
    std::cout << table->name() << " details=\n" << table->DebugString() << "\n";
  }
  //! [get table] [END bigtable_get_table_metadata]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void CheckTableExists(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  //! [START bigtable_check_table_exists]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    google::bigtable::admin::v2::GetTableRequest r;
    r.set_name(table_name);
    r.set_view(google::bigtable::admin::v2::Table::NAME_ONLY);

    StatusOr<google::bigtable::admin::v2::Table> table =
        admin.GetTable(std::move(r));
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
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void GetOrCreateTable(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_get_or_create_table]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    google::bigtable::admin::v2::GetTableRequest r;
    r.set_name(table_name);
    r.set_view(google::bigtable::admin::v2::Table::FULL);

    StatusOr<google::bigtable::admin::v2::Table> table = admin.GetTable(r);
    if (!table &&
        table.status().code() == google::cloud::StatusCode::kNotFound) {
      // The table does not exist, try to create the table.
      table = admin.CreateTable(instance_name, table_id, {});
      if (!table) throw std::runtime_error(table.status().message());
      // The schema returned by a `CreateTable()` request does not include all
      // the metadata for a table, we need to explicitly request the rest:
      table = admin.GetTable(std::move(r));
    }
    if (!table) throw std::runtime_error(table.status().message());
    std::cout << "Table metadata: " << table->DebugString() << "\n";
  }
  // [END bigtable_get_or_create_table]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void DeleteTable(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                 std::vector<std::string> const& argv) {
  //! [delete table] [START bigtable_delete_table]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::Status;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);
    Status status = admin.DeleteTable(table_name);
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Table successfully deleted\n";
  }
  //! [delete table] [END bigtable_delete_table]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void ModifyTable(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                 std::vector<std::string> const& argv) {
  //! [modify table]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::bigtable::admin::v2::ModifyColumnFamiliesRequest;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);
    auto constexpr kSecondsPerDay =
        std::chrono::seconds(std::chrono::hours(24)).count();

    // Drop
    ModifyColumnFamiliesRequest::Modification m1;
    m1.set_id("foo");
    m1.set_drop(true);

    // Update
    ModifyColumnFamiliesRequest::Modification m2;
    m2.set_id("fam");
    google::bigtable::admin::v2::GcRule gc2;
    gc2.set_max_num_versions(5);
    *m2.mutable_update()->mutable_gc_rule() = std::move(gc2);

    // Create
    ModifyColumnFamiliesRequest::Modification m3;
    m3.set_id("fam");
    google::bigtable::admin::v2::GcRule gc3;
    gc3.mutable_max_age()->set_seconds(7 * kSecondsPerDay);
    *m3.mutable_update()->mutable_gc_rule() = std::move(gc3);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(
            table_name, {std::move(m1), std::move(m2), std::move(m3)});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  //! [modify table]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void CreateMaxAgeFamily(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_create_family_gc_max_age]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::bigtable::admin::v2::ModifyColumnFamiliesRequest;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& family_name) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);
    auto constexpr kSecondsPerDay =
        std::chrono::seconds(std::chrono::hours(24)).count();

    ModifyColumnFamiliesRequest::Modification mod;
    mod.set_id(family_name);
    google::bigtable::admin::v2::GcRule gc;
    gc.mutable_max_age()->set_seconds(5 * kSecondsPerDay);
    *mod.mutable_create()->mutable_gc_rule() = std::move(gc);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(table_name, {std::move(mod)});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_max_age]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void CreateMaxVersionsFamily(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_create_family_gc_max_versions]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::bigtable::admin::v2::ModifyColumnFamiliesRequest;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& family_name) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    ModifyColumnFamiliesRequest::Modification mod;
    mod.set_id(family_name);
    google::bigtable::admin::v2::GcRule gc;
    gc.set_max_num_versions(2);
    *mod.mutable_create()->mutable_gc_rule() = std::move(gc);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(table_name, {std::move(mod)});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_max_versions]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void CreateUnionFamily(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_create_family_gc_union]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::bigtable::admin::v2::ModifyColumnFamiliesRequest;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& family_name) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);
    auto constexpr kSecondsPerDay =
        std::chrono::seconds(std::chrono::hours(24)).count();

    google::bigtable::admin::v2::GcRule gc1;
    gc1.set_max_num_versions(1);

    google::bigtable::admin::v2::GcRule gc2;
    gc2.mutable_max_age()->set_seconds(5 * kSecondsPerDay);

    google::bigtable::admin::v2::GcRule gc_union;
    *gc_union.mutable_union_()->add_rules() = std::move(gc1);
    *gc_union.mutable_union_()->add_rules() = std::move(gc2);

    ModifyColumnFamiliesRequest::Modification mod;
    mod.set_id(family_name);
    *mod.mutable_create()->mutable_gc_rule() = std::move(gc_union);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(table_name, {std::move(mod)});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_union]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void CreateIntersectionFamily(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_create_family_gc_intersection]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::bigtable::admin::v2::ModifyColumnFamiliesRequest;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& family_name) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);
    auto constexpr kSecondsPerDay =
        std::chrono::seconds(std::chrono::hours(24)).count();

    google::bigtable::admin::v2::GcRule gc1;
    gc1.set_max_num_versions(1);

    google::bigtable::admin::v2::GcRule gc2;
    gc2.mutable_max_age()->set_seconds(5 * kSecondsPerDay);

    google::bigtable::admin::v2::GcRule gc_intersection;
    *gc_intersection.mutable_intersection()->add_rules() = std::move(gc1);
    *gc_intersection.mutable_intersection()->add_rules() = std::move(gc2);

    ModifyColumnFamiliesRequest::Modification mod;
    mod.set_id(family_name);
    *mod.mutable_create()->mutable_gc_rule() = std::move(gc_intersection);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(table_name, {std::move(mod)});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_intersection]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void CreateNestedFamily(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_create_family_gc_nested]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::bigtable::admin::v2::ModifyColumnFamiliesRequest;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& family_name) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);
    auto constexpr kSecondsPerDay =
        std::chrono::seconds(std::chrono::hours(24)).count();

    google::bigtable::admin::v2::GcRule gc1;
    gc1.set_max_num_versions(10);

    google::bigtable::admin::v2::GcRule gc2_1;
    gc2_1.set_max_num_versions(1);
    google::bigtable::admin::v2::GcRule gc2_2;
    gc2_2.mutable_max_age()->set_seconds(5 * kSecondsPerDay);

    google::bigtable::admin::v2::GcRule gc2;
    *gc2.mutable_intersection()->add_rules() = std::move(gc2_1);
    *gc2.mutable_intersection()->add_rules() = std::move(gc2_2);

    google::bigtable::admin::v2::GcRule gc;
    *gc.mutable_union_()->add_rules() = std::move(gc1);
    *gc.mutable_union_()->add_rules() = std::move(gc2);

    ModifyColumnFamiliesRequest::Modification mod;
    mod.set_id(family_name);
    *mod.mutable_create()->mutable_gc_rule() = std::move(gc);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(table_name, {std::move(mod)});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_create_family_gc_nested]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void GetFamilyMetadata(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_get_family_metadata]
  // [START bigtable_get_family]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& family_name) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    google::bigtable::admin::v2::GetTableRequest r;
    r.set_name(table_name);
    r.set_view(google::bigtable::admin::v2::Table::FULL);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(std::move(r));

    if (!schema) throw std::runtime_error(schema.status().message());
    auto pos = schema->column_families().find(family_name);
    if (pos == schema->column_families().end()) {
      std::cout << "Cannot find family <" << family_name << "> in table\n";
      return;
    }
    std::cout << "Column family metadata for <" << family_name << "> is "
              << pos->second.DebugString() << "\n";
  }
  // [END bigtable_get_family]
  // [END bigtable_get_family_metadata]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void GetOrCreateFamily(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_get_or_create_family]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::bigtable::admin::v2::ModifyColumnFamiliesRequest;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& family_name) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    google::bigtable::admin::v2::GetTableRequest r;
    r.set_name(table_name);
    r.set_view(google::bigtable::admin::v2::Table::FULL);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(std::move(r));

    if (!schema) throw std::runtime_error(schema.status().message());
    auto pos = schema->column_families().find(family_name);
    if (pos == schema->column_families().end()) {
      // Try to create the column family instead:
      ModifyColumnFamiliesRequest::Modification mod;
      mod.set_id(family_name);
      google::bigtable::admin::v2::GcRule gc;
      gc.set_max_num_versions(5);
      *mod.mutable_create()->mutable_gc_rule() = std::move(gc);

      auto modified = admin.ModifyColumnFamilies(table_name, {std::move(mod)});
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
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void DeleteColumnFamily(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_delete_family]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::bigtable::admin::v2::ModifyColumnFamiliesRequest;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& family_name) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    ModifyColumnFamiliesRequest::Modification mod;
    mod.set_id(family_name);
    mod.set_drop(true);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(table_name, {std::move(mod)});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_delete_family]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void CheckFamilyExists(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_check_family_exists]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& family_name) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    google::bigtable::admin::v2::GetTableRequest r;
    r.set_name(table_name);
    r.set_view(google::bigtable::admin::v2::Table::FULL);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(std::move(r));

    if (!schema) throw std::runtime_error(schema.status().message());
    auto pos = schema->column_families().find(family_name);
    if (pos == schema->column_families().end()) {
      std::cout << "The column family <" << family_name << "> does not exist";
      return;
    }
    std::cout << "The column family <" << family_name << "> does exist\n";
  }
  // [END bigtable_check_family_exists]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void ListColumnFamilies(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_list_column_families]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    google::bigtable::admin::v2::GetTableRequest r;
    r.set_name(table_name);
    r.set_view(google::bigtable::admin::v2::Table::FULL);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.GetTable(std::move(r));

    if (!schema) throw std::runtime_error(schema.status().message());
    for (auto const& kv : schema->column_families()) {
      std::string const& column_family_name = kv.first;
      google::bigtable::admin::v2::ColumnFamily const& family = kv.second;
      std::cout << "Column family name: " << column_family_name << "\n"
                << "Column family metadata: " << family.DebugString() << "\n";
    }
  }
  // [END bigtable_list_column_families]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void UpdateGcRule(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                  std::vector<std::string> const& argv) {
  // [START bigtable_update_gc_rule]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::bigtable::admin::v2::ModifyColumnFamiliesRequest;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& family_name) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    ModifyColumnFamiliesRequest::Modification mod;
    mod.set_id(family_name);
    google::bigtable::admin::v2::GcRule gc;
    gc.set_max_num_versions(1);
    *mod.mutable_update()->mutable_gc_rule() = std::move(gc);

    StatusOr<google::bigtable::admin::v2::Table> schema =
        admin.ModifyColumnFamilies(table_name, {std::move(mod)});

    if (!schema) throw std::runtime_error(schema.status().message());
    std::cout << "Schema modified to: " << schema->DebugString() << "\n";
  }
  // [END bigtable_update_gc_rule]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void DropAllRows(google::cloud::bigtable_admin::BigtableTableAdminClient admin,
                 std::vector<std::string> const& argv) {
  //! [drop all rows]
  // [START bigtable_truncate_table]
  // [START bigtable_delete_rows]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::Status;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    google::bigtable::admin::v2::DropRowRangeRequest r;
    r.set_name(table_name);
    r.set_delete_all_data_from_table(true);

    Status status = admin.DropRowRange(std::move(r));
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "All rows successfully deleted\n";
  }
  // [END bigtable_delete_rows]
  // [END bigtable_truncate_table]
  //! [drop all rows]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void DropRowsByPrefix(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  //! [drop rows by prefix] [START bigtable_delete_rows_prefix]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::Status;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& prefix) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);

    google::bigtable::admin::v2::DropRowRangeRequest r;
    r.set_name(table_name);
    r.set_row_key_prefix(prefix);

    Status status = admin.DropRowRange(std::move(r));
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "All rows starting with " << prefix
              << " successfully deleted\n";
  }
  //! [drop rows by prefix] [END bigtable_delete_rows_prefix]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

// TODO(#7732) - update this sample to use the helper method
void WaitForConsistencyCheck(
    google::cloud::bigtable_admin::BigtableTableAdminClient const&,
    std::vector<std::string> const& argv) {
  auto old_admin = google::cloud::bigtable::TableAdmin(
      google::cloud::bigtable::MakeAdminClient(argv.at(0)), argv.at(1));

  //! [wait for consistency check]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](cbt::TableAdmin admin, std::string const& table_id) {
    StatusOr<std::string> consistency_token =
        admin.GenerateConsistencyToken(table_id);
    if (!consistency_token) {
      throw std::runtime_error(consistency_token.status().message());
    }
    future<StatusOr<cbt::Consistency>> consistent_future =
        admin.WaitForConsistency(table_id, *consistency_token);
    future<void> fut = consistent_future.then(
        [&consistency_token](future<StatusOr<cbt::Consistency>> f) {
          auto is_consistent = f.get();
          if (!is_consistent) {
            throw std::runtime_error(is_consistent.status().message());
          }
          std::cout << "Table is consistent with token " << *consistency_token
                    << "\n";
        });
    fut.get();  // simplify example by blocking until operation is done.
  }
  //! [wait for consistency check]
  (std::move(old_admin), argv.at(2));
}

void CheckConsistency(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  //! [check consistency]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id,
     std::string const& consistency_token) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);
    StatusOr<google::bigtable::admin::v2::CheckConsistencyResponse> result =
        admin.CheckConsistency(table_name, consistency_token);
    if (!result) throw std::runtime_error(result.status().message());
    if (result->consistent()) {
      std::cout << "Table is consistent with token " << consistency_token
                << "\n";
    } else {
      std::cout
          << "Table is not yet consistent, Please try again later with the"
          << " same token (" << consistency_token << ")\n";
    }
  }
  //! [check consistency]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void GenerateConsistencyToken(
    google::cloud::bigtable_admin::BigtableTableAdminClient admin,
    std::vector<std::string> const& argv) {
  //! [generate consistency token]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableTableAdminClient admin, std::string const& project_id,
     std::string const& instance_id, std::string const& table_id) {
    std::string table_name = cbt::TableName(project_id, instance_id, table_id);
    StatusOr<google::bigtable::admin::v2::GenerateConsistencyTokenResponse>
        token = admin.GenerateConsistencyToken(table_name);
    if (!token) throw std::runtime_error(token.status().message());
    std::cout << "generated token is : " << token->consistency_token() << "\n";
  }
  //! [generate consistency token]
  (std::move(admin), argv.at(0), argv.at(1), argv.at(2));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;

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

  auto conn = cbta::MakeBigtableTableAdminConnection();
  // If a previous run of these samples crashes before cleaning up there may be
  // old tables left over. As there are quotas on the total number of tables we
  // remove stale tables after 48 hours.
  std::cout << "\nCleaning up old tables" << std::endl;
  cbt::testing::CleanupStaleTables(conn, project_id, instance_id);
  auto admin = cbta::BigtableTableAdminClient(std::move(conn));

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  // This table is actually created and used to test the positive case (e.g.
  // GetTable() and "table does exist")
  auto table_id_1 = cbt::testing::RandomTableId(generator);
  // This table does not exist and used to test the negative case (e.g.
  // GetTable() but "table does not exist")
  auto table_id_2 = cbt::testing::RandomTableId(generator);

  // Create a table to run the tests on.
  google::bigtable::admin::v2::Table t;
  auto& families = *t.mutable_column_families();
  google::bigtable::admin::v2::GcRule gc1;
  gc1.set_max_num_versions(10);
  *families["fam"].mutable_gc_rule() = std::move(gc1);
  google::bigtable::admin::v2::GcRule gc2;
  gc2.set_max_num_versions(3);
  *families["foo"].mutable_gc_rule() = std::move(gc2);

  auto table_1 = admin.CreateTable(cbt::InstanceName(project_id, instance_id),
                                   table_id_1, std::move(t));
  if (!table_1) throw std::runtime_error(table_1.status().message());

  std::cout << "\nRunning ListTables() example" << std::endl;
  ListTables(admin, {project_id, instance_id});

  std::cout << "\nRunning GetTable() example" << std::endl;
  GetTable(admin, {project_id, instance_id, table_id_1});

  std::cout << "\nRunning CheckTableExists() example [1]" << std::endl;
  CheckTableExists(admin, {project_id, instance_id, table_id_1});

  std::cout << "\nRunning CheckTableExists() example [2]" << std::endl;
  CheckTableExists(admin, {project_id, instance_id, table_id_2});

  std::cout << "\nRunning GetOrCreateTable() example [1]" << std::endl;
  GetOrCreateTable(admin, {project_id, instance_id, table_id_1});

  std::cout << "\nRunning GetOrCreateTable() example [2]" << std::endl;
  GetOrCreateTable(admin, {project_id, instance_id, table_id_2});

  std::cout << "\nRunning DeleteTable() example" << std::endl;
  DeleteTable(admin, {project_id, instance_id, table_id_2});

  std::cout << "\nRunning ModifyTable() example" << std::endl;
  ModifyTable(admin, {project_id, instance_id, table_id_1});

  std::cout << "\nRunning CreateMaxAgeFamily() example" << std::endl;
  CreateMaxAgeFamily(admin,
                     {project_id, instance_id, table_id_1, "max-age-family"});

  std::cout << "\nRunning CreateMaxAgeFamily() example" << std::endl;
  CreateMaxVersionsFamily(
      admin, {project_id, instance_id, table_id_1, "max-versions-family"});

  std::cout << "\nRunning CreateUnionFamily() example" << std::endl;
  CreateUnionFamily(admin,
                    {project_id, instance_id, table_id_1, "union-family"});

  std::cout << "\nRunning CreateIntersectionFamily() example" << std::endl;
  CreateIntersectionFamily(
      admin, {project_id, instance_id, table_id_1, "intersection-family"});

  std::cout << "\nRunning CreateNestedFamily() example" << std::endl;
  CreateNestedFamily(admin,
                     {project_id, instance_id, table_id_1, "nested-family"});

  std::cout << "\nRunning ListColumnFamilies() example" << std::endl;
  ListColumnFamilies(admin, {project_id, instance_id, table_id_1});

  std::cout << "\nRunning GetFamilyMetadata() example" << std::endl;
  GetFamilyMetadata(admin,
                    {project_id, instance_id, table_id_1, "nested-family"});

  std::cout << "\nRunning GetOrCreateFamily() example" << std::endl;
  GetOrCreateFamily(
      admin, {project_id, instance_id, table_id_1, "get-or-create-family"});

  std::cout << "\nRunning DeleteColumnFamily() example" << std::endl;
  DeleteColumnFamily(admin,
                     {project_id, instance_id, table_id_1, "nested-family"});

  std::cout << "\nRunning CheckFamilyExists() example [1]" << std::endl;
  CheckFamilyExists(admin,
                    {project_id, instance_id, table_id_1, "nested-family"});

  std::cout << "\nRunning CheckFamilyExists() example [2]" << std::endl;
  CheckFamilyExists(admin,
                    {project_id, instance_id, table_id_1, "max-age-family"});

  std::cout << "\nRunning UpdateGcRule() example" << std::endl;
  UpdateGcRule(admin, {project_id, instance_id, table_id_1, "max-age-family"});

  std::cout << "\nRunning WaitForConsistencyCheck() example" << std::endl;
  WaitForConsistencyCheck(admin, {project_id, instance_id, table_id_1});

  std::cout << "\nRunning GenerateConsistencyToken() example" << std::endl;
  GenerateConsistencyToken(admin, {project_id, instance_id, table_id_1});

  auto token = admin.GenerateConsistencyToken(table_1->name());
  if (!token) throw std::runtime_error(token.status().message());

  std::cout << "\nRunning CheckConsistency() example" << std::endl;
  CheckConsistency(
      admin, {project_id, instance_id, table_id_1, token->consistency_token()});

  std::cout << "\nRunning DropRowsByPrefix() example" << std::endl;
  DropRowsByPrefix(
      admin, {project_id, instance_id, table_id_1, "test-prefix/foo/bar/"});

  std::cout << "\nRunning DropAllRows() example" << std::endl;
  DropAllRows(admin, {project_id, instance_id, table_id_1});

  (void)admin.DeleteTable(table_1->name());
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  namespace examples = ::google::cloud::bigtable::examples;
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
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  ::google::cloud::LogSink::Instance().Flush();
  return 1;
}
