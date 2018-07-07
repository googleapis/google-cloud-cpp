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

#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"

namespace cbt = google::cloud::bigtable;

int main(int argc, char* argv[]) try {
  // A little helper to extract the last component of a '/' separated path.
  auto basename = [](std::string const& path) -> std::string {
    auto last_slash = path.find_last_of('/');
    return path.substr(last_slash + 1);
  };

  if (argc != 4) {
    std::string const cmd = argv[0];
    std::cerr << "Usage: " << basename(cmd)
              << " <project_id> <instance_id> <table_id>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const table_id = argv[3];

  // Verify the instance is configured for replication.
  cbt::InstanceAdmin instance_admin(cbt::CreateDefaultInstanceAdminClient(
      project_id, google::cloud::bigtable::ClientOptions()));

  std::string instructions = R""(
Please follow the instructions at:
    https://cloud.google.com/bigtable/docs/creating-instance
to create a production instance with more than 1 cluster and try again.)"";

  auto instance = instance_admin.GetInstance(instance_id);
  if (instance.type() != cbt::InstanceConfig::PRODUCTION) {
    std::cerr << "This example only works on PRODUCTION instances, but"
              << " " << instance_id << " is a DEVELOPMENT instance"
              << instructions << std::endl;
    return 1;
  }

  auto clusters = instance_admin.ListClusters(instance_id);
  if (clusters.size() < 2) {
    std::cerr << "This example only works with replicated instances, but"
              << " " << instance_id << " only has " << clusters.size()
              << " cluster(s)" << instructions << std::endl;
    return 1;
  }

  bool found_app_profile = false;
  std::string profile_id;
  auto profiles = instance_admin.ListAppProfiles(instance_id);
  for (auto const& p : profiles) {
    if (p.has_single_cluster_routing()) {
      found_app_profile = true;
      profile_id = basename(p.name());
      std::cout << "Found an existing profile (" << profile_id << ") that"
                << " routes to a single cluster="
                << p.single_cluster_routing().cluster_id()
                << "\nwill use this this profile in the example" << std::endl;
    }
  }
  if (not found_app_profile) {
    std::cout << "Found no profiles with single cluster routing.\n"
              << "Will attempt to create a new profile for the example."
              << std::endl;
    profile_id = "sample-profile";
    auto cluster_id = basename(clusters[0].name());
    auto profile = instance_admin.CreateAppProfile(
        cbt::InstanceId(instance_id),
        cbt::AppProfileConfig::SingleClusterRouting(
            cbt::AppProfileId(profile_id), cbt::ClusterId(cluster_id)));
    std::cout << "Created new profile: " << profile.name() << std::endl;
  }

  // Create the table to run this example.
  std::cout << "Creating " << table_id << " ..." << std::flush;
  cbt::TableAdmin table_admin(
      cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions()),
      instance_id);

  // Define the desired schema for the Table.
  auto gc_rule = cbt::GcRule::MaxNumVersions(1);
  cbt::TableConfig schema({{"family", gc_rule}}, {});

  // Create a table.
  auto returned_schema = table_admin.CreateTable(table_id, schema);
  std::cout << " DONE" << std::endl;

  // Create an object to access the Cloud Bigtable Data API, using the profile
  // created (or identified) above, this example uses this object to insert
  // some data on the table.
  auto data_client = cbt::CreateDefaultDataClient(project_id, instance_id,
                                                  cbt::ClientOptions());
  cbt::Table write(data_client, cbt::AppProfileId(profile_id), table_id);

  // Create a different object using the default profile, we will use this
  // object to read the data back.
  cbt::Table read(data_client, table_id);

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
    write.Apply(cbt::SingleRowMutation(std::move(row_key),
                                       cbt::SetCell("family", "c0", greeting)));
    ++i;
  }

  std::cout << "Wrote some data to " << table_id << std::endl;

  // Now create a checkpoint and create a future to wait until the replication
  // completes.
  auto token = table_admin.GenerateConsistencyToken(table_id);
  std::future<bool> is_consistent = table_admin.WaitForConsistencyCheck(
      cbt::TableId(table_id), cbt::ConsistencyToken(token));

  // Wait for the replication to catch up.
  std::cout << "Waiting for consistent replication state using " << token
            << " ..." << std::flush;
  if (not is_consistent.get()) {
    std::cerr << "\nTable is not in a consistent state. Aborting test."
              << std::endl;
    return 1;
  }
  std::cout << " DONE" << std::endl;

  // Read a single row.
  //! [read row]
  std::cout << "Reading data from key-0 on " << table_id << std::endl;
  auto result = read.ReadRow(
      "key-0", cbt::Filter::ColumnRangeClosed("family", "c0", "c0"));
  if (not result.first) {
    std::cout << "Cannot find row 'key-0' in the table: " << table_id
              << std::endl;
    return 1;
  }
  auto const& cell = result.second.cells().front();
  std::cout << cell.family_name() << ":" << cell.column_qualifier() << "    @ "
            << cell.timestamp().count() << "us\n"
            << '"' << cell.value() << '"' << std::endl;
  //! [read row]

  // Read multiple rows.
  //! [scan all]
  std::cout << "Scanning all the data from " << table_id << std::endl;
  for (auto& row : read.ReadRows(cbt::RowRange::InfiniteRange(),
                                 cbt::Filter::PassAllFilter())) {
    std::cout << row.row_key() << ":\n";
    for (auto& cell : row.cells()) {
      std::cout << "\t" << cell.family_name() << ":" << cell.column_qualifier()
                << "    @ " << cell.timestamp().count() << "us\n"
                << "\t\"" << cell.value() << '"' << std::endl;
    }
  }
  //! [scan all]

  // Delete the table
  //! [delete table]
  table_admin.DeleteTable(table_id);
  //! [delete table]

  if (not found_app_profile) {
    // Delete the application profile created earlier in the test, ignore
    // warnings because this is just a test.
    instance_admin.DeleteAppProfile(cbt::InstanceId(instance_id),
                                    cbt::AppProfileId(profile_id), true);
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
//! [all code]
