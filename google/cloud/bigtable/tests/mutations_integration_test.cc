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

#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/chrono_literals.h"

namespace {

namespace bigtable = google::cloud::bigtable;
using ::google::cloud::testing_util::chrono_literals::operator"" _us;

using MutationIntegrationTest = bigtable::testing::TableIntegrationTest;
/**
 * This function creates Cell by ignoring the timestamp.
 * In this case Cloud Bigtable will insert the default server
 * side timestamp for the cells.
 */
void CreateCellsIgnoringTimestamp(bigtable::Table& table,
                                  std::vector<bigtable::Cell> const& cells) {
  std::map<bigtable::RowKeyType, bigtable::SingleRowMutation> mutations;
  for (auto const& cell : cells) {
    auto key = cell.row_key();
    auto inserted = mutations.emplace(key, bigtable::SingleRowMutation(key));
    inserted.first->second.emplace_back(bigtable::SetCell(
        cell.family_name(), cell.column_qualifier(), cell.value()));
  }

  bigtable::BulkMutation bulk;
  for (auto& kv : mutations) {
    bulk.emplace_back(std::move(kv.second));
  }
  ASSERT_NE(0, bulk.size());
  auto failures = table.BulkApply(std::move(bulk));
  ASSERT_TRUE(failures.empty());
}

std::string const kColumnFamily1 = "family1";
std::string const kColumnFamily2 = "family2";
std::string const kColumnFamily3 = "family3";

}  // namespace

/**
 * Check if the values inserted by SetCell are correctly inserted into
 * Cloud Bigtable
 */
TEST_F(MutationIntegrationTest, SetCellTest) {
  auto table = GetTable();

  // Create a vector of cells which will be inserted into bigtable
  std::string const row_key = "SetCellRowKey";
  std::vector<bigtable::Cell> created_cells{
      {row_key, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key, kColumnFamily1, "column_id1", 1000, "v-c-0-1"},
      {row_key, kColumnFamily1, "column_id1", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 0, "v-c0-0-0"},
      {row_key, kColumnFamily2, "column_id3", 1000, "v-c1-0-1"},
      {row_key, kColumnFamily3, "column_id1", 2000, "v-c1-0-2"},
  };

  CreateCells(table, created_cells);
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

  CheckEqualUnordered(created_cells, actual_cells);
}

/**
 * Check if the numeric and string values inserted by SetCell are
 * correctly inserted into Cloud Bigtable
 */
TEST_F(MutationIntegrationTest, SetCellNumericValueTest) {
  auto table = GetTable();

  // Create a vector of cells which will be inserted into bigtable
  std::string const row_key = "SetCellNumRowKey";
  std::vector<bigtable::Cell> created_cells{
      {row_key, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key, kColumnFamily1, "column_id1", 1000, 2000},
      {row_key, kColumnFamily1, "column_id1", 2000, 3000},
      {row_key, kColumnFamily2, "column_id2", 0, "v-c0-0-0"},
      {row_key, kColumnFamily2, "column_id3", 1000, 5000},
      {row_key, kColumnFamily3, "column_id1", 2000, "v-c1-0-2"}};

  CreateCells(table, created_cells);
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

  CheckEqualUnordered(created_cells, actual_cells);
}

/**
 * Check if an error is returned when a string value was set and a numeric
 * value was retrieved. NOTE: This error happens only when/because the length
 * of the string != sizeof(int64_t).
 */
TEST_F(MutationIntegrationTest, SetCellNumericValueErrorTest) {
  std::string const table_id = RandomTableId();
  bigtable::Cell new_cell(
      "row-key", "column_family", "column_id", 1000,
      "some string value that is longer than sizeof(int64_t)");
  auto decoded = new_cell.decode_big_endian_integer<std::int64_t>();
  EXPECT_FALSE(decoded);

  // To be explicit, setting a string value that happens to be 8-bytes long
  // *will* be decodeable to in int64_t. I don't know what value it will be,
  // but it's decodeable.
  new_cell =
      bigtable::Cell("row-key", "column_family", "column_id", 1000, "12345678");
  decoded = new_cell.decode_big_endian_integer<std::int64_t>();
  EXPECT_STATUS_OK(decoded);
}

/**
 * Verify that the values inserted by SetCell with server-side timestamp are
 * correctly inserted into Cloud Bigtable.
 */
TEST_F(MutationIntegrationTest, SetCellIgnoreTimestampTest) {
  auto table = GetTable();

  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key = "SetCellRowKey";
  std::vector<bigtable::Cell> created_cells{
      {row_key, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key, kColumnFamily1, "column_id2", 1000, "v-c-0-1"},
      {row_key, kColumnFamily1, "column_id3", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 0, "v-c0-0-0"},
      {row_key, kColumnFamily2, "column_id3", 1000, "v-c1-0-1"},
      {row_key, kColumnFamily3, "column_id1", 2000, "v-c1-0-2"},
  };
  std::int64_t server_timestamp = -1;
  std::vector<bigtable::Cell> expected_cells{
      {row_key, kColumnFamily1, "column_id1", server_timestamp, "v-c-0-0"},
      {row_key, kColumnFamily1, "column_id2", server_timestamp, "v-c-0-1"},
      {row_key, kColumnFamily1, "column_id3", server_timestamp, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", server_timestamp, "v-c0-0-0"},
      {row_key, kColumnFamily2, "column_id3", server_timestamp, "v-c1-0-1"},
      {row_key, kColumnFamily3, "column_id1", server_timestamp, "v-c1-0-2"},
  };

  CreateCellsIgnoringTimestamp(table, created_cells);
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

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
  auto table = GetTable();
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key = "DeleteColumn-Key";
  std::vector<bigtable::Cell> created_cells{
      {row_key, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key, kColumnFamily1, "column_id2", 1000, "v-c-0-1"},
      {row_key, kColumnFamily1, "column_id3", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 2000, "v-c0-0-0"},
      {row_key, kColumnFamily2, "column_id2", 1000, "v-c0-0-1"},
      {row_key, kColumnFamily2, "column_id2", 3000, "v-c0-0-2"},
      {row_key, kColumnFamily2, "column_id2", 4000, "v-c0-0-3"},
      {row_key, kColumnFamily2, "column_id3", 1000, "v-c1-0-1"},
      {row_key, kColumnFamily2, "column_id2", 2000, "v-c1-0-2"},
      {row_key, kColumnFamily3, "column_id1", 2000, "v-c1-0-2"},
  };

  std::vector<bigtable::Cell> expected_cells{
      {row_key, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key, kColumnFamily1, "column_id2", 1000, "v-c-0-1"},
      {row_key, kColumnFamily1, "column_id3", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 1000, "v-c0-0-1"},
      {row_key, kColumnFamily2, "column_id2", 4000, "v-c0-0-3"},
      {row_key, kColumnFamily2, "column_id3", 1000, "v-c1-0-1"},
      {row_key, kColumnFamily3, "column_id1", 2000, "v-c1-0-2"},
  };

  // Create records
  CreateCells(table, created_cells);
  // Delete the columns with column identifier as column_id2
  auto status = table.Apply(bigtable::SingleRowMutation(
      row_key, bigtable::DeleteFromColumn(kColumnFamily2, "column_id2", 2000_us,
                                          4000_us)));
  ASSERT_STATUS_OK(status);
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

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
  auto table = GetTable();
  // Create a vector of cell which will be inserted into bigtable
  std::string const key = "row";
  std::vector<bigtable::Cell> created_cells{
      {key, kColumnFamily1, "c1", 1000, "v1"},
      {key, kColumnFamily1, "c2", 1000, "v2"},
      {key, kColumnFamily1, "c3", 2000, "v3"},
      {key, kColumnFamily2, "c2", 1000, "v4"},
      {key, kColumnFamily2, "c2", 3000, "v5"},
      {key, kColumnFamily2, "c2", 4000, "v6"},
      {key, kColumnFamily2, "c3", 1000, "v7"},
      {key, kColumnFamily2, "c2", 2000, "v8"},
      {key, kColumnFamily3, "c1", 2000, "v9"},
  };

  CreateCells(table, created_cells);

  // Try to delete the columns with an invalid range:
  auto status = table.Apply(bigtable::SingleRowMutation(
      key, bigtable::DeleteFromColumn(kColumnFamily2, "c2", 4000_us, 2000_us)));
  EXPECT_FALSE(status.ok());
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

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
  auto table = GetTable();
  // Create a vector of cell which will be inserted into bigtable
  std::string const key = "row";
  std::vector<bigtable::Cell> created_cells{
      {key, kColumnFamily1, "c3", 2000, "v3"},
      {key, kColumnFamily2, "c2", 2000, "v2"},
      {key, kColumnFamily3, "c1", 2000, "v1"},
  };

  CreateCells(table, created_cells);

  auto status = table.Apply(bigtable::SingleRowMutation(
      key, bigtable::DeleteFromColumn(kColumnFamily2, "c2", 2000_us, 2000_us)));
  EXPECT_FALSE(status.ok());
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

  CheckEqualUnordered(created_cells, actual_cells);
}

/**
 * Verify that DeleteFromColumn operation for specific column_identifier
 * is deleting all records only for that column_identifier.
 */
TEST_F(MutationIntegrationTest, DeleteFromColumnForAllTest) {
  auto table = GetTable();
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key = "DeleteColumnForAll-Key";
  std::vector<bigtable::Cell> created_cells{
      {row_key, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key, kColumnFamily1, "column_id3", 1000, "v-c-0-1"},
      {row_key, kColumnFamily2, "column_id3", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 2000, "v-c0-0-0"},
      {row_key, kColumnFamily1, "column_id3", 3000, "v-c1-0-2"},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key, kColumnFamily2, "column_id3", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 2000, "v-c0-0-0"},
  };

  // Create records
  CreateCells(table, created_cells);
  // Delete the columns with column identifier column_id3
  auto status = table.Apply(bigtable::SingleRowMutation(
      row_key, bigtable::DeleteFromColumn(kColumnFamily1, "column_id3")));
  ASSERT_STATUS_OK(status);
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

  CheckEqualUnordered(expected_cells, actual_cells);
}

/**
 * Verify that DeleteFromColumn operation for specific column_identifier
 * and starting from specific timestamp is deleting all records after that
 * timestamp only.
 */
TEST_F(MutationIntegrationTest, DeleteFromColumnStartingFromTest) {
  auto table = GetTable();
  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key = "DeleteColumnStartingFrom-Key";
  std::vector<bigtable::Cell> created_cells{
      {row_key, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key, kColumnFamily1, "column_id1", 1000, "v-c-0-1"},
      {row_key, kColumnFamily1, "column_id1", 2000, "v-c-0-1"},
      {row_key, kColumnFamily2, "column_id3", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 2000, "v-c0-0-0"},
      {row_key, kColumnFamily1, "column_id3", 3000, "v-c1-0-2"},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key, kColumnFamily2, "column_id3", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 2000, "v-c0-0-0"},
      {row_key, kColumnFamily1, "column_id3", 3000, "v-c1-0-2"},
  };

  // Create records
  CreateCells(table, created_cells);
  // Delete the columns with column identifier column_id1
  auto status = table.Apply(bigtable::SingleRowMutation(
      row_key, bigtable::DeleteFromColumnStartingFrom(kColumnFamily1,
                                                      "column_id1", 1000_us)));
  ASSERT_STATUS_OK(status);
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

  CheckEqualUnordered(expected_cells, actual_cells);
}

/**
 * Verify that DeleteFromColumn operation for specific column_identifier
 * and ending at specific timestamp is deleting all records before that
 * timestamp only. end_timestamp is not inclusive.
 */
TEST_F(MutationIntegrationTest, DeleteFromColumnEndingAtTest) {
  auto table = GetTable();
  // Create a vector of cell which will be inserted into bigtable cloud
  std::string const row_key = "DeleteColumnEndingAt-Key";
  std::vector<bigtable::Cell> created_cells{
      {row_key, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key, kColumnFamily1, "column_id1", 1000, "v-c-0-1"},
      {row_key, kColumnFamily1, "column_id1", 2000, "v-c-0-1"},
      {row_key, kColumnFamily2, "column_id3", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 2000, "v-c0-0-0"},
      {row_key, kColumnFamily1, "column_id3", 3000, "v-c1-0-2"},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key, kColumnFamily1, "column_id1", 2000, "v-c-0-1"},
      {row_key, kColumnFamily2, "column_id3", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 2000, "v-c0-0-0"},
      {row_key, kColumnFamily1, "column_id3", 3000, "v-c1-0-2"},
  };

  // Create records
  CreateCells(table, created_cells);
  // end_time is not inclusive, only records with timestamp < time_end
  // will be deleted
  // Delete the columns with column identifier column_id1
  auto status = table.Apply(bigtable::SingleRowMutation(
      row_key, bigtable::DeleteFromColumnEndingAt(kColumnFamily1, "column_id1",
                                                  2000_us)));
  ASSERT_STATUS_OK(status);
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

  CheckEqualUnordered(expected_cells, actual_cells);
}

/**
 * Verify that records deleted for a specific family will delete correct
 * records for that family only
 */
TEST_F(MutationIntegrationTest, DeleteFromFamilyTest) {
  auto table = GetTable();

  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key = "DeleteFamily-Key";
  std::vector<bigtable::Cell> created_cells{
      {row_key, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key, kColumnFamily1, "column_id1", 1000, "v-c-0-1"},
      {row_key, kColumnFamily2, "column_id3", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 2000, "v-c0-0-0"},
      {row_key, kColumnFamily1, "column_id3", 3000, "v-c1-0-2"},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key, kColumnFamily2, "column_id3", 2000, "v-c-0-2"},
      {row_key, kColumnFamily2, "column_id2", 2000, "v-c0-0-0"},
  };

  // Create records
  CreateCells(table, created_cells);
  // Delete all the records for family
  auto status = table.Apply(bigtable::SingleRowMutation(
      row_key, bigtable::DeleteFromFamily(kColumnFamily1)));
  ASSERT_STATUS_OK(status);
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

  CheckEqualUnordered(expected_cells, actual_cells);
}

/**
 * Verify that records deleted for a specific row will delete correct
 * records for that row only
 */
TEST_F(MutationIntegrationTest, DeleteFromRowTest) {
  auto table = GetTable();

  // Create a vector of cell which will be inserted into bigtable
  std::string const row_key1 = "DeleteRowKey1";
  std::string const row_key2 = "DeleteRowKey2";
  std::vector<bigtable::Cell> created_cells{
      {row_key1, kColumnFamily1, "column_id1", 0, "v-c-0-0"},
      {row_key1, kColumnFamily1, "column_id1", 1000, "v-c-0-1"},
      {row_key1, kColumnFamily2, "column_id3", 2000, "v-c-0-2"},
      {row_key2, kColumnFamily2, "column_id2", 2000, "v-c0-0-0"},
      {row_key2, kColumnFamily3, "column_id3", 3000, "v-c1-0-2"},
  };
  std::vector<bigtable::Cell> expected_cells{
      {row_key2, kColumnFamily2, "column_id2", 2000, "v-c0-0-0"},
      {row_key2, kColumnFamily3, "column_id3", 3000, "v-c1-0-2"},
  };

  // Create records
  CreateCells(table, created_cells);
  // Delete all the records for a row
  auto status = table.Apply(
      bigtable::SingleRowMutation(row_key1, bigtable::DeleteFromRow()));
  ASSERT_STATUS_OK(status);
  auto actual_cells = ReadRows(table, bigtable::Filter::PassAllFilter());

  CheckEqualUnordered(expected_cells, actual_cells);
}
// Test Cases Finished

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new ::bigtable::testing::TableTestEnvironment);

  return RUN_ALL_TESTS();
}
