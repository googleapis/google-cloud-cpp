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

  /**
   * Delete the records from table for the specified family and column
   * identifier.
   */
  void DeleteFromColumn(bigtable::Table& table, std::string row_key,
                        std::string column_family, std::string column_id) {
    bigtable::SingleRowMutation mutation(row_key);
    mutation.emplace_back(bigtable::DeleteFromColumn(column_family, column_id));

    bigtable::BulkMutation bulk;
    bulk.emplace_back(std::move(mutation));
    table.BulkApply(std::move(bulk));
  }

  /**
   * Delete the records from table for the specified family and column
   * with timestamp starting from time_begin.
   *
   * @param table represents bigtable table
   * @param row_key represents the row_key from which we have to delete
   *     the records.
   * @param column_family represents column family from which we have to
   *     delete the records.
   * @param column_id represents the column identifier from which we have
   *     to delete the records.
   * @param time_begin represents the starting timestamp from which we
   *     have to delete the records.
   *     This parameter is inclusive, i.e. timestamp >= time_begin
   */
  void DeleteFromColumnStartingFrom(bigtable::Table& table, std::string row_key,
                                    std::string column_family,
                                    std::string column_id,
                                    std::int64_t time_begin) {
    bigtable::SingleRowMutation mutation(row_key);
    mutation.emplace_back(bigtable::DeleteFromColumnStartingFrom(
        column_family, column_id, time_begin));

    bigtable::BulkMutation bulk;
    bulk.emplace_back(std::move(mutation));
    table.BulkApply(std::move(bulk));
  }

  /**
   * Delete the records from table for the specified family and column
   * identifier with timestamp ending at time_end.
   *
   * @param table represents bigtable table
   * @param row_key represents the row_key from which we have to delete
   *     the records.
   * @param column_family represents column family from which we have
   *     to delete the records.
   * @param column_id represents the column identifier from which we
   *     have to delete the records.
   * @param time_end represents the ending timestamp upto which we have
   *     to delete the records.
   *     This parameter is not inclusive. i.e. timestamp < time_end
   */
  void DeleteFromColumnEndingAt(bigtable::Table& table, std::string row_key,
                                std::string column_family,
                                std::string column_id, std::int64_t time_end) {
    bigtable::SingleRowMutation mutation(row_key);
    mutation.emplace_back(
        bigtable::DeleteFromColumnEndingAt(column_family, column_id, time_end));

    bigtable::BulkMutation bulk;
    bulk.emplace_back(std::move(mutation));
    table.BulkApply(std::move(bulk));
  }

  /**
   * Delete the records from table for the specified family and column
   * identifier with timestamp starting from time_begin and ending at
   * time_end.
   *
   * @param table represents bigtable table
   * @param row_key represents the row_key from which we have to delete
   *     the records.
   * @param column_family represents column family from which we have
   *     to delete the records.
   * @param column_id represents the column identifier from which we
   *     have to delete the records.
   * @param time_begin represents the starting timestamp from which
   *     we have to delete the records.
   *     This parameter is inclusive i.e. timestamp >= time_begin
   * @param time_end represents the ending timestamp upto which we have
   *     to delete the records.
   *     This parameter is not inclusive. i.e. timestamp < time_end
   */
  void DeleteFromColumn(bigtable::Table& table, std::string row_key,
                        std::string column_family, std::string column_id,
                        std::int64_t time_begin, std::int64_t time_end) {
    bigtable::SingleRowMutation mutation(row_key);
    mutation.emplace_back(bigtable::DeleteFromColumn(column_family, column_id,
                                                     time_begin, time_end));

    bigtable::BulkMutation bulk;
    bulk.emplace_back(std::move(mutation));
    table.BulkApply(std::move(bulk));
  }

  /**
   * Delete records from a specific row_key and specific column_family
   */
  void DeleteFromFamily(bigtable::Table& table, std::string row_key,
                        std::string column_family) {
    bigtable::SingleRowMutation mutation(row_key);
    mutation.emplace_back(bigtable::DeleteFromFamily(column_family));

    bigtable::BulkMutation bulk;
    bulk.emplace_back(std::move(mutation));
    table.BulkApply(std::move(bulk));
  }

  /**
   * Delete records from a specific row key
   */
  void DeleteFromRow(bigtable::Table& table, std::string row_key) {
    bigtable::SingleRowMutation mutation(row_key);
    mutation.emplace_back(bigtable::DeleteFromRow());

    bigtable::BulkMutation bulk;
    bulk.emplace_back(std::move(mutation));
    table.BulkApply(std::move(bulk));
  }
};
}  // namespace anonymous

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
  std::vector<bigtable::Cell> expected_cells_ignore_time;
  std::vector<bigtable::Cell> actual_cells_ignore_time;
  for (auto& cell : expected_cells) {
    bigtable::Cell newCell(cell.row_key(), cell.family_name(),
                           cell.column_qualifier(), 1000, cell.value(),
                           cell.labels());
    expected_cells_ignore_time.emplace_back(std::move(newCell));
  }
  for (auto& cell : actual_cells) {
    bigtable::Cell newCell(cell.row_key(), cell.family_name(),
                           cell.column_qualifier(), 1000, cell.value(),
                           cell.labels());
    actual_cells_ignore_time.emplace_back(std::move(newCell));
  }

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
  DeleteFromColumn(*table, row_key, column_family2, "column_id2",
                   timestamp_begin, timestamp_end);
  auto actual_cells = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_name);

  CheckEqualUnordered(expected_cells, actual_cells);
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
  DeleteFromColumn(*table, row_key, column_family1, "column_id3");
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
  DeleteFromColumnStartingFrom(*table, row_key, column_family1, "column_id1",
                               time_begin);
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
  DeleteFromColumnEndingAt(*table, row_key, column_family1, "column_id1",
                           time_end);
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
  DeleteFromFamily(*table, row_key, column_family1);
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
  DeleteFromRow(*table, row_key1);
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
