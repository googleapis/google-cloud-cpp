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

#include "google/cloud/bigtable/grpc_error.h"
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace {
namespace btadmin = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;
using namespace google::cloud::testing_util::chrono_literals;

class SnapshotIntegrationTest : public bigtable::testing::TableIntegrationTest {
 protected:
  std::unique_ptr<bigtable::TableAdmin> table_admin_;

  void SetUp() {
    TableIntegrationTest::SetUp();
    std::shared_ptr<bigtable::AdminClient> admin_client =
        bigtable::CreateDefaultAdminClient(
            bigtable::testing::TableTestEnvironment::project_id(),
            bigtable::ClientOptions());
    table_admin_ = google::cloud::internal::make_unique<bigtable::TableAdmin>(
        admin_client, bigtable::testing::TableTestEnvironment::instance_id());
  }

  void TearDown() {}
};

bool IsSnapshotPresent(
    std::vector<google::bigtable::admin::v2::Snapshot> const& snapshots,
    std::string const& snapshot_name) {
  return snapshots.end() !=
         std::find_if(
             snapshots.begin(), snapshots.end(),
             [&snapshot_name](google::bigtable::admin::v2::Snapshot const& i) {
               return i.name() == snapshot_name;
             });
}

}  // namespace

/// @test Verify that `bigtable::TableAdmin` snapshot operations work as
/// expected.
TEST_F(SnapshotIntegrationTest, SnapshotOperationsTableTest) {
  google::cloud::bigtable::TableId table_id(RandomTableId());
  google::cloud::bigtable::TableId table_id_new(table_id.get() + "-new");
  google::cloud::bigtable::ClusterId cluster_id(
      bigtable::testing::TableTestEnvironment::cluster_id());
  google::cloud::bigtable::SnapshotId snapshot_id(table_id.get() + "-snapshot");

  std::string const column_family1 = "family1";
  std::string const column_family2 = "family2";
  std::string const column_family3 = "family3";
  bigtable::TableConfig table_config = bigtable::TableConfig(
      {{column_family1, bigtable::GcRule::MaxNumVersions(10)},
       {column_family2, bigtable::GcRule::MaxNumVersions(10)},
       {column_family3, bigtable::GcRule::MaxNumVersions(10)}},
      {});
  auto table = CreateTable(table_id.get(), table_config);
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key1 = "row1";
  std::string const row_key2 = "row2";
  std::vector<bigtable::Cell> created_cells{
      {row_key1, column_family1, "column_id1", 1000, "v-c-0-0", {}},
      {row_key1, column_family1, "column_id2", 1000, "v-c-0-1", {}},
      {row_key1, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key2, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key2, column_family3, "column_id3", 3000, "v-c1-0-2", {}},
  };

  // Create records
  CreateCells(*table, created_cells);
  auto snapshot =
      table_admin_->SnapshotTable(cluster_id, snapshot_id, table_id, 36000_s)
          .get();
  auto table_ =
      table_admin_
          ->CreateTableFromSnapshot(cluster_id, snapshot_id, table_id_new.get())
          .get();
  auto actual_cells =
      ReadRows(table_id_new.get(), bigtable::Filter::PassAllFilter());

  CheckEqualUnordered(created_cells, actual_cells);

  EXPECT_TRUE(table_admin_->DeleteSnapshot(cluster_id, snapshot_id).ok());
  EXPECT_TRUE(DeleteTable(table_id.get()).ok());
  EXPECT_TRUE(DeleteTable(table_id_new.get()).ok());
}

/// @test Verify that Snapshot CRUD operations work as expected.
TEST_F(SnapshotIntegrationTest, CreateListGetDeleteSnapshot) {
  google::cloud::bigtable::TableId table_id(RandomTableId());
  google::cloud::bigtable::ClusterId cluster_id(
      bigtable::testing::TableTestEnvironment::cluster_id());
  std::string snapshot_id_str = table_id.get() + "-snapshot";
  google::cloud::bigtable::SnapshotId snapshot_id(snapshot_id_str);

  // create table prerequisites for snapshot operations.
  std::string const column_family1 = "family1";
  std::string const column_family2 = "family2";
  std::string const column_family3 = "family3";
  bigtable::TableConfig table_config = bigtable::TableConfig(
      {{column_family1, bigtable::GcRule::MaxNumVersions(10)},
       {column_family2, bigtable::GcRule::MaxNumVersions(10)},
       {column_family3, bigtable::GcRule::MaxNumVersions(10)}},
      {});
  auto table = CreateTable(table_id.get(), table_config);
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key1 = "row1";
  std::string const row_key2 = "row2";
  std::vector<bigtable::Cell> created_cells{
      {row_key1, column_family1, "column_id1", 1000, "v-c-0-0", {}},
      {row_key1, column_family1, "column_id2", 1000, "v-c-0-1", {}},
      {row_key1, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key2, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key2, column_family3, "column_id3", 3000, "v-c1-0-2", {}},
  };
  // Create records
  CreateCells(*table, created_cells);

  // verify new snapshot id in list of snapshot
  auto snapshots_before = table_admin_->ListSnapshots(cluster_id);
  ASSERT_TRUE(snapshots_before);
  ASSERT_FALSE(IsSnapshotPresent(*snapshots_before, snapshot_id_str))
      << "Snapshot (" << snapshot_id_str << ") already exists."
      << " This is unexpected, as the snapshot ids are"
      << " generated at random.";

  // create snapshot
  auto snapshot =
      table_admin_->SnapshotTable(cluster_id, snapshot_id, table_id, 36000_s)
          .get();
  auto snapshots_current = table_admin_->ListSnapshots(cluster_id);
  ASSERT_TRUE(snapshots_current);
  EXPECT_TRUE(IsSnapshotPresent(*snapshots_current, snapshot.name()));

  // get snapshot
  auto snapshot_check = table_admin_->GetSnapshot(cluster_id, snapshot_id);
  auto const npos = std::string::npos;
  ASSERT_TRUE(snapshot_check);
  EXPECT_NE(npos, snapshot_check->name().find(snapshot_id_str));

  // Delete snapshot
  EXPECT_TRUE(table_admin_->DeleteSnapshot(cluster_id, snapshot_id).ok());
  auto snapshots_after_delete = table_admin_->ListSnapshots(cluster_id);
  ASSERT_TRUE(snapshots_after_delete);
  EXPECT_FALSE(IsSnapshotPresent(*snapshots_after_delete, snapshot.name()));

  // delete table
  EXPECT_TRUE(DeleteTable(table_id.get()).ok());
}

// Test Cases Finished

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Check for arguments validity
  if (argc != 4) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    // Show Usage if invalid no of arguments
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project_id> <instance_id> <cluster_id>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const cluster_id = argv[3];

  (void)::testing::AddGlobalTestEnvironment(
      new bigtable::testing::TableTestEnvironment(project_id, instance_id,
                                                  cluster_id));

  return RUN_ALL_TESTS();
}
