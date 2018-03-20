// Copyright 2018 Google Inc.
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

#include "bigtable/client/grpc_error.h"
#include "bigtable/client/testing/table_integration_test.h"

namespace {

namespace admin_proto = ::google::bigtable::admin::v2;

class MutationIntegrationTest : public bigtable::testing::TableIntegrationTest {
 protected:
  std::string const column_family1 = "fam1";
  std::string const column_family2 = "fam2";
  std::string const column_family3 = "fam3";

  bigtable::TableConfig table_config = bigtable::TableConfig(
      {{column_family1, bigtable::GcRule::MaxNumVersions(10)},
       {column_family2, bigtable::GcRule::MaxNumVersions(10)},
       {column_family3, bigtable::GcRule::MaxNumVersions(10)}},
      {});

  /**
   * This function creates Cell by ignoring the timestamp.
   * In this case Cloud Bigtable will insert the default server
   * side timestamp for the cells.
   */
  void CreateCellsIgnoringTimestamp(bigtable::Table& table,
                                    std::vector<bigtable::Cell> const& cells) {
    std::map<std::string, bigtable::SingleRowMutation> mutations;
    for (auto const& cell : cells) {
      std::string key = cell.row_key();
      auto inserted = mutations.emplace(key, bigtable::SingleRowMutation(key));
      inserted.first->second.emplace_back(bigtable::SetCell(
          cell.family_name(), cell.column_qualifier(), cell.value()));
    }

    bigtable::BulkMutation bulk;
    for (auto& kv : mutations) {
      bulk.emplace_back(std::move(kv.second));
    }
    table.BulkApply(std::move(bulk));
  }
};

bool UsingCloudBigtableEmulator() {
  return std::getenv("BIGTABLE_EMULATOR_HOST") != nullptr;
}
}  // namespace

/**
 * Check if the values inserted by SetCell are correctly inserted into
 * Cloud Bigtable
 */
TEST_F(MutationIntegrationTest, SetCellTest) {
  std::string const table_name = "table-setcell";

  auto table = CreateTable(table_name, table_config);
  // Create a vector of cells which will be inserted into bigtable
  std::string const row_key = "SetCellRowKey";
  std::vector<bigtable::Cell> created_cells{
      {row_key, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key, column_family1, "column_id1", 1000, "v-c-0-1", {}},
      {row_key, column_family1, "column_id1", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 0, "v-c0-0-0", {}},
      {row_key, column_family2, "column_id3", 1000, "v-c1-0-1", {}},
      {row_key, column_family3, "column_id1", 2000, "v-c1-0-2", {}},
  };

  CreateCells(*table, created_cells);
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_name);

  CheckEqualUnordered(created_cells, actual_cells);
}

/**
 * Verify that the values inserted by SetCell with server-side timestamp are
 * correctly inserted into Cloud Bigtable.
 */
TEST_F(MutationIntegrationTest, SetCellIgnoreTimestampTest) {
  std::string const table_name = "table-setcell-ignore-timestamp";

  auto table = CreateTable(table_name, table_config);
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key = "SetCellRowKey";
  std::vector<bigtable::Cell> created_cells{
      {row_key, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key, column_family1, "column_id2", 1000, "v-c-0-1", {}},
      {row_key, column_family1, "column_id3", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 0, "v-c0-0-0", {}},
      {row_key, column_family2, "column_id3", 1000, "v-c1-0-1", {}},
      {row_key, column_family3, "column_id1", 2000, "v-c1-0-2", {}},
  };
  std::int64_t server_timestamp = -1;
  std::vector<bigtable::Cell> expected_cells{
      {row_key, column_family1, "column_id1", server_timestamp, "v-c-0-0", {}},
      {row_key, column_family1, "column_id2", server_timestamp, "v-c-0-1", {}},
      {row_key, column_family1, "column_id3", server_timestamp, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", server_timestamp, "v-c0-0-0", {}},
      {row_key, column_family2, "column_id3", server_timestamp, "v-c1-0-1", {}},
      {row_key, column_family3, "column_id1", server_timestamp, "v-c1-0-2", {}},
  };

  CreateCellsIgnoringTimestamp(*table, created_cells);
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_name);

  // Create the expected_cells and actual_cells with same timestamp
  auto expected_cells_ignore_time = GetCellsIgnoringTimestamp(expected_cells);
  auto actual_cells_ignore_time = GetCellsIgnoringTimestamp(actual_cells);

  CheckEqualUnordered(expected_cells_ignore_time, actual_cells_ignore_time);
}

/**
 * Verify that the deletion of records for specific row_key, column_family,
 * column_identifier and within the time range are deleted from Cloud
 * Bigtable.
 */
TEST_F(MutationIntegrationTest, DeleteFromColumnForTimestampRangeTest) {
  std::string const table_name = "table-delete-for-column-time-range";

  auto table = CreateTable(table_name, table_config);
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key = "DeleteColumn-Key";
  std::vector<bigtable::Cell> created_cells{
      {row_key, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key, column_family1, "column_id2", 1000, "v-c-0-1", {}},
      {row_key, column_family1, "column_id3", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key, column_family2, "column_id2", 1000, "v-c0-0-1", {}},
      {row_key, column_family2, "column_id2", 3000, "v-c0-0-2", {}},
      {row_key, column_family2, "column_id2", 4000, "v-c0-0-3", {}},
      {row_key, column_family2, "column_id3", 1000, "v-c1-0-1", {}},
      {row_key, column_family2, "column_id2", 2000, "v-c1-0-2", {}},
      {row_key, column_family3, "column_id1", 2000, "v-c1-0-2", {}},
  };
  std::int64_t timestamp_begin = 2000;
  std::int64_t timestamp_end = 4000;  // timestamp_end is not inclusive.
  std::vector<bigtable::Cell> expected_cells{
      {row_key, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key, column_family1, "column_id2", 1000, "v-c-0-1", {}},
      {row_key, column_family1, "column_id3", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 1000, "v-c0-0-1", {}},
      {row_key, column_family2, "column_id2", 4000, "v-c0-0-3", {}},
      {row_key, column_family2, "column_id3", 1000, "v-c1-0-1", {}},
      {row_key, column_family3, "column_id1", 2000, "v-c1-0-2", {}},
  };

  // Create records
  CreateCells(*table, created_cells);
  // Delete the columns with column identifier as column_id2
  table->Apply(bigtable::SingleRowMutation(
      row_key, bigtable::DeleteFromColumn(column_family2, "column_id2",
                                          timestamp_begin, timestamp_end)));
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_name);

  CheckEqualUnordered(expected_cells, actual_cells);
}

/**
 * Verify DeleteFromColumn() with invalid ranges works.
 *
 * We expect the server (and not the client library) to reject invalid ranges.
 */
TEST_F(MutationIntegrationTest, DeleteFromColumnForReversedTimestampRangeTest) {
  // TODO(#151) - remove workarounds for emulator bug(s).
  if (UsingCloudBigtableEmulator()) {
    return;
  }
  std::string const table_name = "table-delete-for-column-time-range-reversed";

  auto table = CreateTable(table_name, table_config);
  // Create a vector of cell which will be inserted into bigtable
  std::string const key = "row";
  std::vector<bigtable::Cell> created_cells{
      {key, column_family1, "c1", 1000, "v1", {}},
      {key, column_family1, "c2", 1000, "v2", {}},
      {key, column_family1, "c3", 2000, "v3", {}},
      {key, column_family2, "c2", 1000, "v4", {}},
      {key, column_family2, "c2", 3000, "v5", {}},
      {key, column_family2, "c2", 4000, "v6", {}},
      {key, column_family2, "c3", 1000, "v7", {}},
      {key, column_family2, "c2", 2000, "v8", {}},
      {key, column_family3, "c1", 2000, "v9", {}},
  };

  CreateCells(*table, created_cells);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // Try to delete the columns with an invalid range:
  EXPECT_THROW(
      table->Apply(bigtable::SingleRowMutation(
          key, bigtable::DeleteFromColumn(column_family2, "c2", 4000, 2000))),
      bigtable::PermanentMutationFailure);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      table->Apply(bigtable::SingleRowMutation(
          key, bigtable::DeleteFromColumn(column_family2, "column_id2", 4000,
                                          2000))),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_name);

  CheckEqualUnordered(created_cells, actual_cells);
}

/**
 * Verify DeleteFromColumn() with empty ranges works.
 *
 * We expect the server (and not the client library) to reject invalid ranges.
 */
TEST_F(MutationIntegrationTest, DeleteFromColumnForEmptyTimestampRangeTest) {
  // TODO(#151) - remove workarounds for emulator bug(s).
  if (UsingCloudBigtableEmulator()) {
    return;
  }
  std::string const table_name = "table-delete-for-column-time-range-empty";

  auto table = CreateTable(table_name, table_config);
  // Create a vector of cell which will be inserted into bigtable
  std::string const key = "row";
  std::vector<bigtable::Cell> created_cells{
      {key, column_family1, "c3", 2000, "v3", {}},
      {key, column_family2, "c2", 2000, "v2", {}},
      {key, column_family3, "c1", 2000, "v1", {}},
  };

  CreateCells(*table, created_cells);
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  // Try to delete the columns with an invalid range:
  // TODO(#119) - change the expected exception to the wrapper.
  EXPECT_THROW(
      table->Apply(bigtable::SingleRowMutation(
          key, bigtable::DeleteFromColumn(column_family2, "c2", 2000, 2000))),
      bigtable::PermanentMutationFailure);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      table->Apply(bigtable::SingleRowMutation(
          key, bigtable::DeleteFromColumn(column_family2, "column_id2", 2000,
                                          2000))),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_name);

  CheckEqualUnordered(created_cells, actual_cells);
}

/**
 * Verify that DeleteFromColumn operation for specific column_identifier
 * is deleting all records only for that column_identifier.
 */
TEST_F(MutationIntegrationTest, DeleteFromColumnForAllTest) {
  std::string const table_name = "table-delete-for-column";

  auto table = CreateTable(table_name, table_config);
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key = "DeleteColumnForAll-Key";
  std::vector<bigtable::Cell> created_cells{
      {row_key, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key, column_family1, "column_id3", 1000, "v-c-0-1", {}},
      {row_key, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key, column_family1, "column_id3", 3000, "v-c1-0-2", {}},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
  };

  // Create records
  CreateCells(*table, created_cells);
  // Delete the columns with column identifier column_id3
  table->Apply(bigtable::SingleRowMutation(
      row_key, bigtable::DeleteFromColumn(column_family1, "column_id3")));
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_name);

  CheckEqualUnordered(expected_cells, actual_cells);
}

/**
 * Verify that DeleteFromColumn operation for specific column_identifier
 * and starting from specific timestamp is deleting all records after that
 * timestamp only.
 */
TEST_F(MutationIntegrationTest, DeleteFromColumnStartingFromTest) {
  std::string const table_name = "table-delete-for-column-starting-from";

  auto table = CreateTable(table_name, table_config);
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key = "DeleteColumnStartingFrom-Key";
  std::vector<bigtable::Cell> created_cells{
      {row_key, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key, column_family1, "column_id1", 1000, "v-c-0-1", {}},
      {row_key, column_family1, "column_id1", 2000, "v-c-0-1", {}},
      {row_key, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key, column_family1, "column_id3", 3000, "v-c1-0-2", {}},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key, column_family1, "column_id3", 3000, "v-c1-0-2", {}},
  };

  // Create records
  CreateCells(*table, created_cells);
  std::int64_t time_begin = 1000;
  // Delete the columns with column identifier column_id1
  table->Apply(bigtable::SingleRowMutation(
      row_key, bigtable::DeleteFromColumnStartingFrom(
                   column_family1, "column_id1", time_begin)));
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_name);

  CheckEqualUnordered(expected_cells, actual_cells);
}

/**
 * Verify that DeleteFromColumn operation for specific column_identifier
 * and ending at specific timestamp is deleting all records before that
 * timestamp only. end_timestamp is not inclusive.
 */
TEST_F(MutationIntegrationTest, DeleteFromColumnEndingAtTest) {
  std::string const table_name = "table-delete-for-column-ending-at";

  auto table = CreateTable(table_name, table_config);
  // Create a vector of cell which will be inserted into bigtable cloud
  std::string const row_key = "DeleteColumnEndingAt-Key";
  std::vector<bigtable::Cell> created_cells{
      {row_key, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key, column_family1, "column_id1", 1000, "v-c-0-1", {}},
      {row_key, column_family1, "column_id1", 2000, "v-c-0-1", {}},
      {row_key, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key, column_family1, "column_id3", 3000, "v-c1-0-2", {}},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key, column_family1, "column_id1", 2000, "v-c-0-1", {}},
      {row_key, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key, column_family1, "column_id3", 3000, "v-c1-0-2", {}},
  };

  // Create records
  CreateCells(*table, created_cells);
  // end_time is not inclusive, only records with timestamp < time_end
  // will be deleted
  std::int64_t time_end = 2000;
  // Delete the columns with column identifier column_id1
  table->Apply(bigtable::SingleRowMutation(
      row_key, bigtable::DeleteFromColumnEndingAt(column_family1, "column_id1",
                                                  time_end)));
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_name);

  CheckEqualUnordered(expected_cells, actual_cells);
}

/**
 * Verify that records deleted for a specific family will delete correct
 * records for that family only
 */
TEST_F(MutationIntegrationTest, DeleteFromFamilyTest) {
  std::string const table_name = "table-delete-for-family";

  auto table = CreateTable(table_name, table_config);
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key = "DeleteFamily-Key";
  std::vector<bigtable::Cell> created_cells{
      {row_key, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key, column_family1, "column_id1", 1000, "v-c-0-1", {}},
      {row_key, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key, column_family1, "column_id3", 3000, "v-c1-0-2", {}},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
  };

  // Create records
  CreateCells(*table, created_cells);
  // Delete all the records for family
  table->Apply(bigtable::SingleRowMutation(
      row_key, bigtable::DeleteFromFamily(column_family1)));
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_name);

  CheckEqualUnordered(expected_cells, actual_cells);
}

/**
 * Verify that records deleted for a specific row will delete correct
 * records for that row only
 */
TEST_F(MutationIntegrationTest, DeleteFromRowTest) {
  std::string const table_name = "table-delete-for-row";

  auto table = CreateTable(table_name, table_config);
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key1 = "DeleteRowKey1";
  std::string const row_key2 = "DeleteRowKey2";
  std::vector<bigtable::Cell> created_cells{
      {row_key1, column_family1, "column_id1", 0, "v-c-0-0", {}},
      {row_key1, column_family1, "column_id1", 1000, "v-c-0-1", {}},
      {row_key1, column_family2, "column_id3", 2000, "v-c-0-2", {}},
      {row_key2, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key2, column_family3, "column_id3", 3000, "v-c1-0-2", {}},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key2, column_family2, "column_id2", 2000, "v-c0-0-0", {}},
      {row_key2, column_family3, "column_id3", 3000, "v-c1-0-2", {}},
  };

  // Create records
  CreateCells(*table, created_cells);
  // Delete all the records for a row
  table->Apply(
      bigtable::SingleRowMutation(row_key1, bigtable::DeleteFromRow()));
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_name);

  CheckEqualUnordered(expected_cells, actual_cells);
}
// Test Cases Finished

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of("/");
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance>" << std::endl;
    return 1;
  }
  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];

  auto admin_client =
      bigtable::CreateDefaultAdminClient(project_id, bigtable::ClientOptions());
  bigtable::TableAdmin admin(admin_client, instance_id);

  auto table_list = admin.ListTables(admin_proto::Table::NAME_ONLY);
  if (not table_list.empty()) {
    std::cerr << "Expected empty instance at the beginning of integration test"
              << std::endl;
    return 1;
  }

  (void)::testing::AddGlobalTestEnvironment(
      new ::bigtable::testing::TableTestEnvironment(project_id, instance_id));

  return RUN_ALL_TESTS();
}
