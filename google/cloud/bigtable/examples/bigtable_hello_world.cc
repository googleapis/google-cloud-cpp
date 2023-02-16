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

//! [all code]

//! [bigtable includes] [START bigtable_hw_imports]
#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/table.h"
//! [bigtable includes] [END bigtable_hw_imports]
#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include <iostream>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void BigtableHelloWorld(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw Usage{"hello-world <project-id> <instance-id> <table-id>"};
  }
  std::string const& project_id = argv[0];
  std::string const& instance_id = argv[1];
  std::string const& table_id = argv[2];

  // Create a namespace alias to make the code easier to read.
  //! [aliases]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  //! [aliases]

  //! [connect admin] [START bigtable_hw_connect]
  // Connect to the Cloud Bigtable Admin API.
  cbta::BigtableTableAdminClient table_admin(
      cbta::MakeBigtableTableAdminConnection());

  //! [connect data]
  // Create an object to access the Cloud Bigtable Data API.
  cbt::Table table(cbt::MakeDataConnection(),
                   cbt::TableResource(project_id, instance_id, table_id));
  //! [connect data]
  //! [connect admin] [END bigtable_hw_connect]

  //! [create table] [START bigtable_hw_create_table]
  // Define the desired schema for the Table.
  google::bigtable::admin::v2::Table t;
  auto& families = *t.mutable_column_families();
  families["family"].mutable_gc_rule()->set_max_num_versions(1);

  // Create a table.
  std::string instance_name = cbt::InstanceName(project_id, instance_id);
  StatusOr<google::bigtable::admin::v2::Table> schema =
      table_admin.CreateTable(instance_name, table_id, std::move(t));
  //! [create table] [END bigtable_hw_create_table]

  // Modify (and create if necessary) a row.
  //! [write rows] [START bigtable_hw_write_rows]
  std::vector<std::string> greetings{"Hello World!", "Hello Cloud Bigtable!",
                                     "Hello C++!"};
  int i = 0;
  for (auto const& greeting : greetings) {
    // Each row has a unique row key.
    //
    // Note: This example uses sequential numeric IDs for simplicity, but
    // this can result in poor performance in a production application.
    // Since rows are stored in sorted order by key, sequential keys can
    // result in poor distribution of operations across nodes.
    //
    // For more information about how to design a Bigtable schema for the
    // best performance, see the documentation:
    //
    //     https://cloud.google.com/bigtable/docs/schema-design
    std::string row_key = "key-" + std::to_string(i);
    google::cloud::Status status = table.Apply(cbt::SingleRowMutation(
        std::move(row_key), cbt::SetCell("family", "c0", greeting)));

    if (!status.ok()) throw std::runtime_error(status.message());
    ++i;
  }
  //! [write rows] [END bigtable_hw_write_rows]

  //! [create filter] [START bigtable_hw_create_filter]
  cbt::Filter filter = cbt::Filter::ColumnRangeClosed("family", "c0", "c0");
  //! [create filter] [END bigtable_hw_create_filter]

  // Read a single row.
  //! [read row] [START bigtable_hw_get_with_filter]
  StatusOr<std::pair<bool, cbt::Row>> result = table.ReadRow("key-0", filter);
  if (!result) throw std::move(result).status();
  if (!result->first) {
    std::cout << "Cannot find row 'key-0' in the table: " << table.table_name()
              << "\n";
    return;
  }
  cbt::Cell const& cell = result->second.cells().front();
  std::cout << cell.family_name() << ":" << cell.column_qualifier() << "    @ "
            << cell.timestamp().count() << "us\n"
            << '"' << cell.value() << '"' << "\n";
  //! [read row] [END bigtable_hw_get_with_filter]

  // Read all rows.
  //! [scan all] [START bigtable_hw_scan_with_filter]
  for (auto& row : table.ReadRows(cbt::RowRange::InfiniteRange(),
                                  cbt::Filter::PassAllFilter())) {
    if (!row) throw std::move(row).status();
    std::cout << row->row_key() << ":\n";
    for (cbt::Cell const& c : row->cells()) {
      std::cout << "\t" << c.family_name() << ":" << c.column_qualifier()
                << "    @ " << c.timestamp().count() << "us\n"
                << "\t\"" << c.value() << '"' << "\n";
    }
  }
  //! [scan all] [END bigtable_hw_scan_with_filter]

  // Delete the table
  //! [delete table] [START bigtable_hw_delete_table]
  google::cloud::Status status = table_admin.DeleteTable(table.table_name());
  if (!status.ok()) throw std::runtime_error(status.message());
  //! [delete table] [END bigtable_hw_delete_table]
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = ::google::cloud::bigtable;

  if (!argv.empty()) throw Usage{"auto"};
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

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto table_id = cbt::testing::RandomTableId(generator);

  std::cout << "\nRunning the BigtableHelloWorld() example" << std::endl;
  BigtableHelloWorld({project_id, instance_id, table_id});
}

}  // namespace

int main(int argc, char* argv[]) try {
  google::cloud::bigtable::examples::Example example({
      {"auto", RunAll},
      {"hello-world", BigtableHelloWorld},
  });
  return example.Run(argc, argv);
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  ::google::cloud::LogSink::Instance().Flush();
  return 1;
}
//! [all code]
