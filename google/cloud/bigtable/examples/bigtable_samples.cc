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

/**
 * The key used for ReadRow(), ReadModifyWrite(), CheckAndMutate.
 *
 * Using the same key makes it possible for the user to see the effect of
 * the different APIs on one row.
 */
const std::string MAGIC_ROW_KEY = "key-000009";

//! [create table]
void CreateTable(google::cloud::bigtable::TableAdmin admin,
                 std::string const& table_id) {
  auto schema = admin.CreateTable(
      table_id,
      google::cloud::bigtable::TableConfig(
          {{"fam", google::cloud::bigtable::GcRule::MaxNumVersions(10)},
           {"foo",
            google::cloud::bigtable::GcRule::MaxAge(std::chrono::hours(72))}},
          {}));
}
//! [create table]

//! [list tables]
void ListTables(google::cloud::bigtable::TableAdmin& admin) {
  auto tables =
      admin.ListTables(google::bigtable::admin::v2::Table::VIEW_UNSPECIFIED);
  for (auto const& table : tables) {
    std::cout << table.name() << std::endl;
  }
}
//! [list tables]

//! [get table]
void GetTable(google::cloud::bigtable::TableAdmin admin,
              std::string const& table_id) {
  auto table =
      admin.GetTable(table_id, google::bigtable::admin::v2::Table::FULL);
  std::cout << table.name() << "\n";
  for (auto const& family : table.column_families()) {
    std::string const& family_name = family.first;
    std::string gc_rule;
    google::protobuf::TextFormat::PrintToString(family.second.gc_rule(),
                                                &gc_rule);
    std::cout << "\t" << family_name << "\t\t" << gc_rule << std::endl;
  }
}
//! [get table]

//! [delete table]
void DeleteTable(google::cloud::bigtable::TableAdmin admin,
                 std::string const& table_id) {
  admin.DeleteTable(table_id);
}
//! [delete table]

//! [modify table]
void ModifyTable(google::cloud::bigtable::TableAdmin admin,
                 std::string const& table_id) {
  auto schema = admin.ModifyColumnFamilies(
      table_id,
      {google::cloud::bigtable::ColumnFamilyModification::Drop("foo"),
       google::cloud::bigtable::ColumnFamilyModification::Update(
           "fam", google::cloud::bigtable::GcRule::Union(
                      google::cloud::bigtable::GcRule::MaxNumVersions(5),
                      google::cloud::bigtable::GcRule::MaxAge(
                          std::chrono::hours(24 * 7)))),
       google::cloud::bigtable::ColumnFamilyModification::Create(
           "bar", google::cloud::bigtable::GcRule::Intersection(
                      google::cloud::bigtable::GcRule::MaxNumVersions(3),
                      google::cloud::bigtable::GcRule::MaxAge(
                          std::chrono::hours(72))))});

  std::string formatted;
  google::protobuf::TextFormat::PrintToString(schema, &formatted);
  std::cout << "Schema modified to: " << formatted << std::endl;
}
//! [modify table]

//! [drop all rows]
void DropAllRows(google::cloud::bigtable::TableAdmin admin,
                 std::string const& table_id) {
  admin.DropAllRows(table_id);
}
//! [drop all rows]

//! [drop rows by prefix]
void DropRowsByPrefix(google::cloud::bigtable::TableAdmin admin,
                      std::string const& table_id) {
  admin.DropRowsByPrefix(table_id, "key-00004");
}
//! [drop rows by prefix]

//! [apply]
void Apply(google::cloud::bigtable::Table table) {
  // Write several rows with some trivial data.
  for (int i = 0; i != 20; ++i) {
    // Note: This example uses sequential numeric IDs for simplicity, but
    // this can result in poor performance in a production application.
    // Since rows are stored in sorted order by key, sequential keys can
    // result in poor distribution of operations across nodes.
    //
    // For more information about how to design a Bigtable schema for the
    // best performance, see the documentation:
    //
    //     https://cloud.google.com/bigtable/docs/schema-design
    char buf[32];
    snprintf(buf, sizeof(buf), "key-%06d", i);
    google::cloud::bigtable::SingleRowMutation mutation(buf);
    mutation.emplace_back(google::cloud::bigtable::SetCell(
        "fam", "col0", "value0-" + std::to_string(i)));
    mutation.emplace_back(google::cloud::bigtable::SetCell(
        "fam", "col1", "value2-" + std::to_string(i)));
    mutation.emplace_back(google::cloud::bigtable::SetCell(
        "fam", "col2", "value3-" + std::to_string(i)));
    mutation.emplace_back(google::cloud::bigtable::SetCell(
        "fam", "col3", "value4-" + std::to_string(i)));
    table.Apply(std::move(mutation));
  }
}
//! [apply]

//! [bulk apply]
void BulkApply(google::cloud::bigtable::Table table) {
  // Write several rows in a single operation, each row has some trivial data.
  google::cloud::bigtable::BulkMutation bulk;
  for (int i = 0; i != 5000; ++i) {
    // Note: This example uses sequential numeric IDs for simplicity, but
    // this can result in poor performance in a production application.
    // Since rows are stored in sorted order by key, sequential keys can
    // result in poor distribution of operations across nodes.
    //
    // For more information about how to design a Bigtable schema for the
    // best performance, see the documentation:
    //
    //     https://cloud.google.com/bigtable/docs/schema-design
    char buf[32];
    snprintf(buf, sizeof(buf), "key-%06d", i);
    google::cloud::bigtable::SingleRowMutation mutation(buf);
    mutation.emplace_back(google::cloud::bigtable::SetCell(
        "fam", "col0", "value0-" + std::to_string(i)));
    mutation.emplace_back(google::cloud::bigtable::SetCell(
        "fam", "col1", "value2-" + std::to_string(i)));
    mutation.emplace_back(google::cloud::bigtable::SetCell(
        "fam", "col2", "value3-" + std::to_string(i)));
    mutation.emplace_back(google::cloud::bigtable::SetCell(
        "fam", "col3", "value4-" + std::to_string(i)));
    bulk.emplace_back(std::move(mutation));
  }
  table.BulkApply(std::move(bulk));
}
//! [bulk apply]

//! [read row]
void ReadRow(google::cloud::bigtable::Table table) {
  // Filter the results, only include the latest value on each cell.
  auto filter = google::cloud::bigtable::Filter::Latest(1);
  // Read a row, this returns a tuple (bool, row)
  std::pair<bool, google::cloud::bigtable::Row> tuple =
      table.ReadRow(MAGIC_ROW_KEY, std::move(filter));
  if (not tuple.first) {
    std::cout << "Row " << MAGIC_ROW_KEY << " not found" << std::endl;
    return;
  }
  std::cout << "key: " << tuple.second.row_key() << "\n";
  for (auto& cell : tuple.second.cells()) {
    std::cout << "    " << cell.family_name() << ":" << cell.column_qualifier()
              << " = <";
    if (cell.column_qualifier() == "counter") {
      // This example uses "counter" to store 64-bit numbers in BigEndian
      // format, extract them as follows:
      std::cout
          << cell.value_as<google::cloud::bigtable::bigendian64_t>().get();
    } else {
      std::cout << cell.value();
    }
    std::cout << ">\n";
  }
  std::cout << std::flush;
}
//! [read row]

//! [read rows]
void ReadRows(google::cloud::bigtable::Table table) {
  // Create the range of rows to read.
  auto range =
      google::cloud::bigtable::RowRange::Range("key-000010", "key-000020");
  // Filter the results, only include values from the "col0" column in the
  // "fam" column family, and only get the latest value.
  auto filter = google::cloud::bigtable::Filter::Chain(
      google::cloud::bigtable::Filter::ColumnRangeClosed("fam", "col0", "col0"),
      google::cloud::bigtable::Filter::Latest(1));
  // Read and print the rows.
  for (auto const& row : table.ReadRows(range, filter)) {
    if (row.cells().size() != 1) {
      std::ostringstream os;
      os << "Unexpected number of cells in " << row.row_key();
      throw std::runtime_error(os.str());
    }
    auto const& cell = row.cells().at(0);
    std::cout << cell.row_key() << " = [" << cell.value() << "]\n";
  }
  std::cout << std::flush;
}
//! [read rows]

//! [read rows with limit]
void ReadRowsWithLimit(google::cloud::bigtable::Table table) {
  // Create the range of rows to read.
  auto range =
      google::cloud::bigtable::RowRange::Range("key-000010", "key-000020");
  // Filter the results, only include values from the "col0" column in the
  // "fam" column family, and only get the latest value.
  auto filter = google::cloud::bigtable::Filter::Chain(
      google::cloud::bigtable::Filter::ColumnRangeClosed("fam", "col0", "col0"),
      google::cloud::bigtable::Filter::Latest(1));
  // Read and print the first 5 rows in the range.
  for (auto const& row : table.ReadRows(range, 5, filter)) {
    if (row.cells().size() != 1) {
      std::ostringstream os;
      os << "Unexpected number of cells in " << row.row_key();
      throw std::runtime_error(os.str());
    }
    auto const& cell = row.cells().at(0);
    std::cout << cell.row_key() << " = [" << cell.value() << "]\n";
  }
  std::cout << std::flush;
}
//! [read rows with limit]

//! [check and mutate]
void CheckAndMutate(google::cloud::bigtable::Table table) {
  // Check if the latest value of the flip-flop column is "on".
  auto predicate = google::cloud::bigtable::Filter::Chain(
      google::cloud::bigtable::Filter::ColumnRangeClosed("fam", "flip-flop",
                                                         "flip-flop"),
      google::cloud::bigtable::Filter::Latest(1),
      google::cloud::bigtable::Filter::ValueRegex("on"));
  // If the predicate matches, change the latest value to "off", otherwise,
  // change the latest value to "on".  Modify the "flop-flip" column at the
  // same time.
  table.CheckAndMutateRow(
      MAGIC_ROW_KEY, std::move(predicate),
      {google::cloud::bigtable::SetCell("fam", "flip-flop", "off"),
       google::cloud::bigtable::SetCell("fam", "flop-flip", "on")},
      {google::cloud::bigtable::SetCell("fam", "flip-flop", "on"),
       google::cloud::bigtable::SetCell("fam", "flop-flip", "off")});
}
//! [check and mutate]

//! [read modify write]
void ReadModifyWrite(google::cloud::bigtable::Table table) {
  auto row = table.ReadModifyWriteRow(
      MAGIC_ROW_KEY,
      google::cloud::bigtable::ReadModifyWriteRule::IncrementAmount(
          "fam", "counter", 1),
      google::cloud::bigtable::ReadModifyWriteRule::AppendValue("fam", "list",
                                                                ";element"));
  std::cout << row.row_key() << "\n";
  for (auto const& cell : row.cells()) {
  }
}
//! [read modify write]

//! [sample row keys]
void SampleRows(google::cloud::bigtable::Table table) {
  auto samples = table.SampleRows<>();
  for (auto const& sample : samples) {
    std::cout << "key=" << sample.row_key << " - " << sample.offset_bytes
              << "\n";
  }
  std::cout << std::flush;
}
//! [sample row keys]

//! [sample row keys collections]
void SampleRowsCollections(google::cloud::bigtable::Table table) {
  auto list_samples = table.SampleRows<std::list>();
  for (auto const& sample : list_samples) {
    std::cout << "key=" << sample.row_key << " - " << sample.offset_bytes
              << "\n";
  }
  auto deque_samples = table.SampleRows<std::deque>();
  for (auto const& sample : deque_samples) {
    std::cout << "key=" << sample.row_key << " - " << sample.offset_bytes
              << "\n";
  }
  std::cout << std::flush;
}
//! [sample row keys collections]

//! [run table operations]
void RunTableOperations(google::cloud::bigtable::TableAdmin admin,
                        std::string const& table_id) {
  std::cout << "Creating a table: " << std::endl;
  auto schema = admin.CreateTable(
      table_id,
      google::cloud::bigtable::TableConfig(
          {{"fam", google::cloud::bigtable::GcRule::MaxNumVersions(10)},
           {"foo",
            google::cloud::bigtable::GcRule::MaxAge(std::chrono::hours(72))}},
          {}));
  std::cout << " Done" << std::endl;

  std::cout << "Listing tables: " << std::endl;
  auto tables =
      admin.ListTables(google::bigtable::admin::v2::Table::VIEW_UNSPECIFIED);
  for (auto const& table : tables) {
    std::cout << table.name() << std::endl;
  }

  std::cout << "Get table: " << std::endl;
  auto table =
      admin.GetTable(table_id, google::bigtable::admin::v2::Table::FULL);
  std::cout << table.name() << "\n";
  std::cout << "Table name : " << table.name() << std::endl;

  std::cout << "List table families and GC rules: " << std::endl;
  for (auto const& family : table.column_families()) {
    std::string const& family_name = family.first;
    std::string gc_rule;
    google::protobuf::TextFormat::PrintToString(family.second.gc_rule(),
                                                &gc_rule);
    std::cout << "Table Families :" << family_name << "\t\t" << gc_rule
              << std::endl;
  }

  std::cout << "Update a column family GC rule: " << std::endl;
  auto schema1 = admin.ModifyColumnFamilies(
      table_id,
      {google::cloud::bigtable::ColumnFamilyModification::Drop("foo"),
       google::cloud::bigtable::ColumnFamilyModification::Update(
           "fam", google::cloud::bigtable::GcRule::Union(
                      google::cloud::bigtable::GcRule::MaxNumVersions(5),
                      google::cloud::bigtable::GcRule::MaxAge(
                          std::chrono::hours(24 * 7)))),
       google::cloud::bigtable::ColumnFamilyModification::Create(
           "bar", google::cloud::bigtable::GcRule::Intersection(
                      google::cloud::bigtable::GcRule::MaxNumVersions(3),
                      google::cloud::bigtable::GcRule::MaxAge(
                          std::chrono::hours(72))))});

  std::string formatted;
  google::protobuf::TextFormat::PrintToString(schema1, &formatted);
  std::cout << "Schema modified to: " << formatted << std::endl;

  std::cout << "Deleting table: " << std::endl;
  admin.DeleteTable(table_id);
  std::cout << " Done" << std::endl;
}
//! [run table operations]

// This full example demonstrate various instance operations
void RunFullExample(google::cloud::bigtable::TableAdmin admin,
                    std::string const& table_id) {
  // [START bigtable_create_table]
  std::cout << "Creating a table: " << std::endl;
  auto schema = admin.CreateTable(
      table_id,
      google::cloud::bigtable::TableConfig(
          {{"fam", google::cloud::bigtable::GcRule::MaxNumVersions(10)},
           {"foo",
            google::cloud::bigtable::GcRule::MaxAge(std::chrono::hours(72))}},
          {}));
  std::cout << " Done" << std::endl;
  // [END bigtable_create_table]

  // [START bigtable_list_table]
  std::cout << "Listing tables: " << std::endl;
  auto tables =
      admin.ListTables(google::bigtable::admin::v2::Table::VIEW_UNSPECIFIED);
  for (auto const& table : tables) {
    std::cout << table.name() << std::endl;
  }
  // [END bigtable_list_table]

  // [START bigtable_get_table]
  std::cout << "Get table: " << std::endl;
  auto table =
      admin.GetTable(table_id, google::bigtable::admin::v2::Table::FULL);
  std::cout << table.name() << "\n";
  std::cout << "Table name : " << table.name() << std::endl;
  // [END bigtable_get_table]

  // [START bigtable_table_famalies]
  for (auto const& family : table.column_families()) {
    std::string const& family_name = family.first;
    std::string gc_rule;
    google::protobuf::TextFormat::PrintToString(family.second.gc_rule(),
                                                &gc_rule);
    std::cout << "Table Families :" << family_name << "\t\t" << gc_rule
              << std::endl;
  }
  // [END bigtable_table_famalies]

  // [START bigtable_update_column_famaly]
  std::cout << "Update a column family GC rule: " << std::endl;
  auto schema1 = admin.ModifyColumnFamilies(
      table_id,
      {google::cloud::bigtable::ColumnFamilyModification::Drop("foo"),
       google::cloud::bigtable::ColumnFamilyModification::Update(
           "fam", google::cloud::bigtable::GcRule::Union(
                      google::cloud::bigtable::GcRule::MaxNumVersions(5),
                      google::cloud::bigtable::GcRule::MaxAge(
                          std::chrono::hours(24 * 7)))),
       google::cloud::bigtable::ColumnFamilyModification::Create(
           "bar", google::cloud::bigtable::GcRule::Intersection(
                      google::cloud::bigtable::GcRule::MaxNumVersions(3),
                      google::cloud::bigtable::GcRule::MaxAge(
                          std::chrono::hours(72))))});

  std::string formatted;
  google::protobuf::TextFormat::PrintToString(schema1, &formatted);
  std::cout << "Schema modified to: " << formatted << std::endl;
  // [END bigtable_update_column_famaly]

  // [START bigtable_delete_table]
  std::cout << "Deleting table: " << std::endl;
  admin.DeleteTable(table_id);
  std::cout << " Done" << std::endl;
  // [END bigtable_delete_table]
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  auto print_usage = [argv]() {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    auto program = cmd.substr(last_slash + 1);
    std::cerr << "\nUsage: " << program
              << " <command> <project_id> [arguments]\n\n"
              << "Examples:\n";
    for (auto example : {"run my-project my-instance my-table",
                         "create-table my-project my-instance my-table",
                         "list-tables my-project my-instance",
                         "get-table my-project my-instance my-table",
                         "modify-table my-project my-instance my-table",
                         "drop-all-rows my-project my-instance my-table",
                         "delete-table my-project my-instance my-table",
                         "run-full-example my-project my-instance my-table"}) {
      std::cerr << "  " << program << " " << example << "\n";
    }
    std::cerr << std::flush;
  };

  if (argc != 5) {
    print_usage();
    return 1;
  }

  std::string const command = argv[1];
  std::string const project_id = argv[2];
  std::string const instance_id = argv[3];
  std::string const table_id = argv[4];

  // Connect to the Cloud Bigtable admin endpoint.
  //! [connect admin]
  google::cloud::bigtable::TableAdmin admin(
      google::cloud::bigtable::CreateDefaultAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()),
      instance_id);
  //! [connect admin]

  // Connect to the Cloud Bigtable endpoint.
  //! [connect data]
  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          project_id, instance_id, google::cloud::bigtable::ClientOptions()),
      table_id);
  //! [connect data]

  if (command == "run") {
    RunTableOperations(admin, table_id);
  } else if (command == "create-table") {
    CreateTable(admin, table_id);
  } else if (command == "list-tables") {
    ListTables(admin);
  } else if (command == "get-table") {
    GetTable(admin, table_id);
  } else if (command == "delete-table") {
    DeleteTable(admin, table_id);
  } else if (command == "modify-table") {
    ModifyTable(admin, table_id);
  } else if (command == "drop-all-rows") {
    DropAllRows(admin, table_id);
  } else if (command == "drop-rows-by-prefix") {
    DropRowsByPrefix(admin, table_id);
  } else if (command == "apply") {
    Apply(table);
  } else if (command == "bulk-apply") {
    BulkApply(table);
  } else if (command == "read-row" or command == "read") {
    ReadRow(table);
  } else if (command == "read-rows" or command == "scan") {
    ReadRows(table);
  } else if (command == "read-rows-with-limit") {
    ReadRowsWithLimit(table);
  } else if (command == "check-and-mutate") {
    CheckAndMutate(table);
  } else if (command == "read-modify-write") {
    ReadModifyWrite(table);
  } else if (command == "sample-rows") {
    SampleRows(table);
  } else if (command == "run-full-example") {
    RunFullExample(admin, table_id);
  } else {
    std::cerr << "Unknown command: " << command << std::endl;
    print_usage();
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
//! [all code]
