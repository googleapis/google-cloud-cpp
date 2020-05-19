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
#include "google/cloud/bigtable/table_admin.h"
//! [bigtable includes]
#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/crash_handler.h"
#include <iostream>

namespace {

using google::cloud::bigtable::examples::Usage;

void HelloWorldTableAdmin(std::vector<std::string> const& argv) {
  if (argv.size() != 3) {
    throw Usage{
        "hello-world-table-admin <project-id> <instance-id> "
        "<table-id>"};
  }
  std::string const project_id = argv[0];
  std::string const instance_id = argv[1];
  std::string const table_id = argv[2];

  //! [aliases]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  //! [aliases]

  // Connect to the Cloud Bigtable admin endpoint.
  //! [connect admin]
  cbt::TableAdmin admin(
      cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions()),
      instance_id);
  //! [connect admin]

  //! [create table]
  std::cout << "Creating a table:\n";
  StatusOr<google::bigtable::admin::v2::Table> schema = admin.CreateTable(
      table_id,
      cbt::TableConfig({{"fam", cbt::GcRule::MaxNumVersions(10)},
                        {"foo", cbt::GcRule::MaxAge(std::chrono::hours(72))}},
                       {}));
  std::cout << "DONE\n";
  //! [create table]

  //! [listing tables]
  std::cout << "Listing tables:\n";
  auto tables = admin.ListTables(cbt::TableAdmin::NAME_ONLY);
  if (!tables) throw std::runtime_error(tables.status().message());
  for (auto const& table : *tables) {
    std::cout << "    " << table.name() << "\n";
  }
  std::cout << "DONE\n";
  //! [listing tables]

  //! [retrieve table metadata]
  std::cout << "Get table metadata:\n";
  StatusOr<google::bigtable::admin::v2::Table> table =
      admin.GetTable(table_id, cbt::TableAdmin::FULL);
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
  StatusOr<google::bigtable::admin::v2::Table> updated_schema =
      admin.ModifyColumnFamilies(
          table_id,
          {cbt::ColumnFamilyModification::Update(
              "fam", cbt::GcRule::Union(
                         cbt::GcRule::MaxNumVersions(5),
                         cbt::GcRule::MaxAge(std::chrono::hours(24 * 7))))});
  if (!updated_schema) {
    throw std::runtime_error(updated_schema.status().message());
  }
  std::cout << "Schema modified to: " << updated_schema->DebugString() << "\n";
  //! [modify column family]

  //! [drop all rows]
  std::cout << "Deleting all the rows in " << table_id << "\n";
  google::cloud::Status status = admin.DropAllRows(table_id);
  if (!status.ok()) throw std::runtime_error(status.message());
  std::cout << "DONE\n";
  //! [drop all rows]

  //! [delete table]
  std::cout << "Deleting table:\n";
  google::cloud::Status delete_status = admin.DeleteTable(table_id);
  if (!delete_status.ok()) throw std::runtime_error(delete_status.message());
  std::cout << "DONE\n";
  //! [delete table]
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = google::cloud::bigtable;

  if (!argv.empty()) throw google::cloud::bigtable::examples::Usage{"auto"};
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

  examples::CleanupOldTables("hw-admin-tbl-", admin);

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto table_id = examples::RandomTableId("hw-admin-tbl-", generator);

  std::cout << "\nRunning the HelloWorldTableAdmin() example" << std::endl;
  HelloWorldTableAdmin({project_id, instance_id, table_id});
}

}  // namespace

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InstallCrashHandler(argv[0]);

  google::cloud::bigtable::examples::Example example({
      {"auto", RunAll},
      {"hello-world-table-admin", HelloWorldTableAdmin},
  });
  return example.Run(argc, argv);
}
//! [all code]
