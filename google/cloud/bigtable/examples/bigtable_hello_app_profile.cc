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

#include "google/cloud/bigtable/admin/bigtable_instance_admin_client.h"
#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"

//! [cbt namespace]
namespace cbt = ::google::cloud::bigtable;
//! [cbt namespace]

namespace {

using ::google::cloud::bigtable::examples::Usage;

void HelloWorldAppProfile(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw Usage{
        "hello-world-app-profile <project-id> <instance-id>"
        " <table-id> <profile-id>"};
  }
  std::string const& project_id = argv[0];
  std::string const& instance_id = argv[1];
  std::string const& table_id = argv[2];
  std::string const& profile_id = argv[3];

  // Create an object to access the Cloud Bigtable Data API.
  auto data_client = cbt::MakeDataClient(project_id, instance_id);

  // Use the default profile to write some data.
  cbt::Table write(data_client, table_id);

  // Modify (and create if necessary) a row.
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
    google::cloud::Status status = write.Apply(cbt::SingleRowMutation(
        std::move(row_key), cbt::SetCell("fam", "c0", greeting)));
    if (!status.ok()) throw std::runtime_error(status.message());
    ++i;
  }

  std::cout << "Wrote some greetings to " << table_id << "\n";

  // Access Cloud Bigtable using a different profile
  //! [read with app profile]
  cbt::Table read(data_client, profile_id, table_id);

  google::cloud::StatusOr<std::pair<bool, cbt::Row>> result =
      read.ReadRow("key-0", cbt::Filter::ColumnRangeClosed("fam", "c0", "c0"));
  if (!result) throw std::runtime_error(result.status().message());
  if (!result->first) throw std::runtime_error("missing row with key = key-0");
  cbt::Cell const& cell = result->second.cells().front();
  std::cout << cell.family_name() << ":" << cell.column_qualifier() << "    @ "
            << cell.timestamp().count() << "us\n"
            << '"' << cell.value() << '"' << "\n";
  //! [read with app profile]

  // Read multiple rows.
  //! [scan all with app profile]
  std::cout << "Scanning all the data from " << table_id << "\n";
  for (google::cloud::StatusOr<cbt::Row>& row : read.ReadRows(
           cbt::RowRange::InfiniteRange(), cbt::Filter::PassAllFilter())) {
    if (!row) throw std::runtime_error(row.status().message());
    std::cout << row->row_key() << ":\n";
    for (cbt::Cell const& c : row->cells()) {
      std::cout << "\t" << c.family_name() << ":" << c.column_qualifier()
                << "    @ " << c.timestamp().count() << "us\n"
                << "\t\"" << c.value() << '"' << "\n";
    }
  }
  //! [scan all with app profile]
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
  auto admin = cbta::BigtableTableAdminClient(std::move(conn));

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto table_id = cbt::testing::RandomTableId(generator);

  // Create a table to run the tests on
  google::bigtable::admin::v2::GcRule gc;
  gc.set_max_num_versions(10);
  google::bigtable::admin::v2::Table t;
  auto& families = *t.mutable_column_families();
  *families["fam"].mutable_gc_rule() = std::move(gc);
  auto schema = admin.CreateTable(cbt::InstanceName(project_id, instance_id),
                                  table_id, std::move(t));
  if (!schema) throw std::runtime_error(schema.status().message());

  auto profile_id = "hw-app-profile-" +
                    google::cloud::internal::Sample(
                        generator, 8, "abcdefghilklmnopqrstuvwxyz0123456789");

  auto instance_admin = cbta::BigtableInstanceAdminClient(
      cbta::MakeBigtableInstanceAdminConnection());
  google::bigtable::admin::v2::AppProfile ap;
  ap.mutable_multi_cluster_routing_use_any()->Clear();
  auto profile = instance_admin.CreateAppProfile(
      cbt::InstanceName(project_id, instance_id), profile_id, std::move(ap));
  if (!profile) throw std::runtime_error(profile.status().message());

  std::cout << "\nRunning the AppProfile hello world example" << std::endl;
  HelloWorldAppProfile({project_id, instance_id, table_id, profile_id});

  google::bigtable::admin::v2::DeleteAppProfileRequest req;
  req.set_name(cbt::AppProfileName(project_id, instance_id, profile_id));
  req.set_ignore_warnings(true);
  instance_admin.DeleteAppProfile(std::move(req));
  admin.DeleteTable(schema->name());
}

}  // namespace

int main(int argc, char* argv[]) try {
  google::cloud::bigtable::examples::Example example({
      {"auto", RunAll},
      {"hello-world-app-profile", HelloWorldAppProfile},
  });
  return example.Run(argc, argv);
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  ::google::cloud::LogSink::Instance().Flush();
  return 1;
}
