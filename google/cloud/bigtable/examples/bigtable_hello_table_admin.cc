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

//! [bigtable includes]
#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/resource_names.h"
//! [bigtable includes]
#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include <chrono>
#include <iostream>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void HelloWorldTableAdmin(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw Usage{
        "hello-world-table-admin <project-id> <instance-id> "
        "<table-id>"};
  }
  std::string const& project_id = argv[0];
  std::string const& instance_id = argv[1];
  std::string const& table_id = argv[2];

  //! [aliases]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  //! [aliases]

  // Connect to the Cloud Bigtable admin endpoint.
  //! [connect admin]
  cbta::BigtableTableAdminClient admin(
      cbta::MakeBigtableTableAdminConnection());
  //! [connect admin]

  //! [create table]
  // Define the schema
  auto constexpr kSecondsPerDay =
      std::chrono::seconds(std::chrono::hours(24)).count();

  google::bigtable::admin::v2::Table t;
  auto& families = *t.mutable_column_families();
  families["fam"].mutable_gc_rule()->set_max_num_versions(10);
  families["foo"].mutable_gc_rule()->mutable_max_age()->set_seconds(
      3 * kSecondsPerDay);

  std::cout << "Creating a table:\n";
  std::string instance_name = cbt::InstanceName(project_id, instance_id);
  StatusOr<google::bigtable::admin::v2::Table> schema =
      admin.CreateTable(instance_name, table_id, std::move(t));
  std::cout << "DONE\n";
  //! [create table]

  //! [listing tables]
  std::cout << "Listing tables:\n";
  google::bigtable::admin::v2::ListTablesRequest list_req;
  list_req.set_parent(instance_name);
  list_req.set_view(google::bigtable::admin::v2::Table::NAME_ONLY);
  auto tables = admin.ListTables(std::move(list_req));
  for (auto const& table : tables) {
    if (!table) throw std::runtime_error(table.status().message());
    std::cout << "    " << table->name() << "\n";
  }
  std::cout << "DONE\n";
  //! [listing tables]

  //! [retrieve table metadata]
  std::cout << "Get table metadata:\n";
  google::bigtable::admin::v2::GetTableRequest get_req;
  get_req.set_name(schema->name());
  get_req.set_view(google::bigtable::admin::v2::Table::FULL);
  StatusOr<google::bigtable::admin::v2::Table> table =
      admin.GetTable(std::move(get_req));
  if (!table) throw std::runtime_error(table.status().message());
  std::cout << "Table name : " << table->name() << "\n";

  std::cout << "List table families and GC rules:\n";
  for (auto const& kv : table->column_families()) {
    std::string const& family_name = kv.first;
    google::bigtable::admin::v2::ColumnFamily const& metadata = kv.second;
    std::cout << "Column Family :" << family_name << "\t"
              << metadata.DebugString() << "\n";
  }
  std::cout << "DONE\n";
  //! [retrieve table metadata]

  //! [modify column family]
  std::cout << "Update a column family GC rule:\n";
  using ::google::bigtable::admin::v2::ModifyColumnFamiliesRequest;
  ModifyColumnFamiliesRequest::Modification mod;
  mod.set_id("fam");
  mod.mutable_update()->mutable_gc_rule()->set_max_num_versions(5);

  StatusOr<google::bigtable::admin::v2::Table> updated_schema =
      admin.ModifyColumnFamilies(table->name(), {std::move(mod)});
  if (!updated_schema) {
    throw std::runtime_error(updated_schema.status().message());
  }
  std::cout << "Schema modified to: " << updated_schema->DebugString() << "\n";
  //! [modify column family]

  //! [drop all rows]
  std::cout << "Deleting all the rows in " << table_id << "\n";
  google::bigtable::admin::v2::DropRowRangeRequest drop_req;
  drop_req.set_name(table->name());
  drop_req.set_delete_all_data_from_table(true);
  google::cloud::Status status = admin.DropRowRange(std::move(drop_req));
  if (!status.ok()) throw std::runtime_error(status.message());
  std::cout << "DONE\n";
  //! [drop all rows]

  //! [delete table]
  std::cout << "Deleting table:\n";
  google::cloud::Status delete_status = admin.DeleteTable(table->name());
  if (!delete_status.ok()) throw std::runtime_error(delete_status.message());
  std::cout << "DONE\n";
  //! [delete table]
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
  cbt::testing::CleanupStaleTables(conn, project_id, instance_id);

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto table_id = cbt::testing::RandomTableId(generator);

  std::cout << "\nRunning the HelloWorldTableAdmin() example" << std::endl;
  HelloWorldTableAdmin({project_id, instance_id, table_id});
}

}  // namespace

int main(int argc, char* argv[]) try {
  google::cloud::bigtable::examples::Example example({
      {"auto", RunAll},
      {"hello-world-table-admin", HelloWorldTableAdmin},
  });
  return example.Run(argc, argv);
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  ::google::cloud::LogSink::Instance().Flush();
  return 1;
}
//! [all code]
