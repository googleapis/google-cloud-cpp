// Copyright 2017 Google Inc.
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
#include "google/cloud/log.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
using ::std::chrono::duration_cast;
using ::std::chrono::microseconds;
using ::std::chrono::milliseconds;
using ::testing::Contains;
using ::testing::HasSubstr;

using DataIntegrationTest =
    ::google::cloud::bigtable::testing::TableIntegrationTest;

/// Use Table::Apply() to insert a single row.
void Apply(Table& table, std::string const& row_key,
           std::vector<Cell> const& cells) {
  auto mutation = SingleRowMutation(row_key);
  for (auto const& cell : cells) {
    mutation.emplace_back(
        SetCell(cell.family_name(), cell.column_qualifier(),
                duration_cast<milliseconds>(microseconds(cell.timestamp())),
                cell.value()));
  }
  auto status = table.Apply(std::move(mutation));
  ASSERT_STATUS_OK(status);
}

/// Use Table::BulkApply() to insert multiple rows.
void BulkApply(Table& table, std::vector<Cell> const& cells) {
  std::map<RowKeyType, SingleRowMutation> mutations;
  for (auto const& cell : cells) {
    auto key = cell.row_key();
    auto inserted = mutations.emplace(key, SingleRowMutation(key));
    inserted.first->second.emplace_back(
        SetCell(cell.family_name(), cell.column_qualifier(),
                duration_cast<milliseconds>(microseconds(cell.timestamp())),
                cell.value()));
  }
  BulkMutation bulk;
  for (auto& kv : mutations) {
    bulk.emplace_back(std::move(kv.second));
  }
  auto failures = table.BulkApply(std::move(bulk));
  ASSERT_TRUE(failures.empty());
}

/// The column families used in this test.
std::string const kFamily1 = "family1";
std::string const kFamily2 = "family2";
std::string const kFamily3 = "family3";
std::string const kFamily4 = "family4";

TEST_F(DataIntegrationTest, TableApply) {
  auto table = GetTable();

  std::string const row_key = "row-key-1";
  std::vector<Cell> created{{row_key, kFamily4, "c0", 1000, "v1000"},
                            {row_key, kFamily4, "c1", 2000, "v2000"}};
  Apply(table, row_key, created);
  std::vector<Cell> expected{{row_key, kFamily4, "c0", 1000, "v1000"},
                             {row_key, kFamily4, "c1", 2000, "v2000"}};

  auto actual = ReadRows(table, Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableBulkApply) {
  auto table = GetTable();

  std::vector<Cell> created{{"row-key-1", kFamily4, "c0", 1000, "v1000"},
                            {"row-key-1", kFamily4, "c1", 2000, "v2000"},
                            {"row-key-2", kFamily4, "c0", 1000, "v1000"},
                            {"row-key-2", kFamily4, "c1", 2000, "v2000"},
                            {"row-key-3", kFamily4, "c0", 1000, "v1000"},
                            {"row-key-3", kFamily4, "c1", 2000, "v2000"},
                            {"row-key-4", kFamily4, "c0", 1000, "v1000"},
                            {"row-key-4", kFamily4, "c1", 2000, "v2000"}};
  BulkApply(table, created);
  std::vector<Cell> expected{{"row-key-1", kFamily4, "c0", 1000, "v1000"},
                             {"row-key-1", kFamily4, "c1", 2000, "v2000"},
                             {"row-key-2", kFamily4, "c0", 1000, "v1000"},
                             {"row-key-2", kFamily4, "c1", 2000, "v2000"},
                             {"row-key-3", kFamily4, "c0", 1000, "v1000"},
                             {"row-key-3", kFamily4, "c1", 2000, "v2000"},
                             {"row-key-4", kFamily4, "c0", 1000, "v1000"},
                             {"row-key-4", kFamily4, "c1", 2000, "v2000"}};

  auto actual = ReadRows(table, Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableSingleRow) {
  std::string const row_key = "row-key-1";
  auto table = GetTable();

  auto mutation =
      SingleRowMutation(row_key, SetCell(kFamily4, "c1", 1_ms, "V1000"),
                        SetCell(kFamily4, "c2", 2_ms, "V2000"),
                        SetCell(kFamily4, "c3", 3_ms, "V3000"));
  ASSERT_STATUS_OK(table.Apply(std::move(mutation)));
  std::vector<Cell> expected{{row_key, kFamily4, "c1", 1000, "V1000"},
                             {row_key, kFamily4, "c2", 2000, "V2000"},
                             {row_key, kFamily4, "c3", 3000, "V3000"}};

  auto actual = ReadRows(table, Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableReadRowTest) {
  auto table = GetTable();
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";

  std::vector<Cell> created{{row_key1, kFamily4, "c1", 1000, "v1000"},
                            {row_key2, kFamily4, "c2", 2000, "v2000"}};
  std::vector<Cell> expected{{row_key1, kFamily4, "c1", 1000, "v1000"}};

  CreateCells(table, created);
  auto row_cell = table.ReadRow(row_key1, Filter::PassAllFilter());
  ASSERT_STATUS_OK(row_cell);
  std::vector<Cell> actual;
  actual.emplace_back(row_cell->second.cells().at(0));
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableReadRowNotExistTest) {
  auto table = GetTable();
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";

  std::vector<Cell> created{{row_key1, kFamily4, "c1", 1000, "v1000"}};

  CreateCells(table, created);
  auto row_cell = table.ReadRow(row_key2, Filter::PassAllFilter());
  ASSERT_STATUS_OK(row_cell);
  EXPECT_FALSE(row_cell->first);
}

TEST_F(DataIntegrationTest, TableReadRowsAllRows) {
  auto table = GetTable();
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::string const row_key3(1024, '3');    // a long key
  std::string const long_value(1024, 'v');  // a long value

  std::vector<Cell> created{{row_key1, kFamily4, "c1", 1000, "data1"},
                            {row_key1, kFamily4, "c2", 1000, "data2"},
                            {row_key2, kFamily4, "c1", 1000, ""},
                            {row_key3, kFamily4, "c1", 1000, long_value}};

  CreateCells(table, created);

  // Some equivalent ways to read the three rows
  auto read1 = table.ReadRows(RowSet(RowRange::InfiniteRange()),
                              Filter::PassAllFilter());
  CheckEqualUnordered(created, MoveCellsFromReader(read1));

  auto read2 = table.ReadRows(RowSet(RowRange::InfiniteRange()), 3,
                              Filter::PassAllFilter());
  CheckEqualUnordered(created, MoveCellsFromReader(read2));

  auto read3 =
      table.ReadRows(RowSet(RowRange::InfiniteRange()),
                     RowReader::NO_ROWS_LIMIT, Filter::PassAllFilter());
  CheckEqualUnordered(created, MoveCellsFromReader(read3));

  if (!UsingCloudBigtableEmulator()) {
    // TODO(#151) - remove workarounds for emulator bug(s).
    auto read4 = table.ReadRows(RowSet(), Filter::PassAllFilter());
    CheckEqualUnordered(created, MoveCellsFromReader(read4));
  }
}

TEST_F(DataIntegrationTest, TableReadRowsPartialRows) {
  auto table = GetTable();
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::string const row_key3 = "row-key-3";

  std::vector<Cell> created{{row_key1, kFamily4, "c1", 1000, "data1"},
                            {row_key1, kFamily4, "c2", 1000, "data2"},
                            {row_key2, kFamily4, "c1", 1000, "data3"},
                            {row_key3, kFamily4, "c1", 1000, "data4"}};

  CreateCells(table, created);

  std::vector<Cell> expected{{row_key1, kFamily4, "c1", 1000, "data1"},
                             {row_key1, kFamily4, "c2", 1000, "data2"},
                             {row_key2, kFamily4, "c1", 1000, "data3"}};

  // Some equivalent ways of reading just the first two rows
  {
    SCOPED_TRACE(table.table_name() + " ReadRows(key1, key2)");
    RowSet rows;
    rows.Append(row_key1);
    rows.Append(row_key2);
    auto reader = table.ReadRows(std::move(rows), Filter::PassAllFilter());
    CheckEqualUnordered(expected, MoveCellsFromReader(reader));
  }

  {
    SCOPED_TRACE(table.table_name() + " ReadRows(, limit = 2, )");
    auto reader = table.ReadRows(RowSet(RowRange::InfiniteRange()), 2,
                                 Filter::PassAllFilter());
    CheckEqualUnordered(expected, MoveCellsFromReader(reader));
  }

  {
    SCOPED_TRACE(table.table_name() + " ReadRows([key1, key2], ...)");
    RowSet rows(RowRange::Closed(row_key1, row_key2));
    auto reader = table.ReadRows(std::move(rows), Filter::PassAllFilter());
    CheckEqualUnordered(expected, MoveCellsFromReader(reader));
  }
}

TEST_F(DataIntegrationTest, TableReadRowsNoRows) {
  auto table = GetTable();
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::string const row_key3 = "row-key-3";

  std::vector<Cell> created{{row_key1, kFamily4, "c1", 1000, "data1"},
                            {row_key3, kFamily4, "c1", 1000, "data2"}};

  CreateCells(table, created);

  std::vector<Cell> expected;  // empty

  // read nonexistent rows
  auto read1 = table.ReadRows(RowSet(row_key2), Filter::PassAllFilter());
  CheckEqualUnordered(expected, MoveCellsFromReader(read1));

  auto read2 = table.ReadRows(RowSet(RowRange::Prefix(row_key2)),
                              Filter::PassAllFilter());
  CheckEqualUnordered(expected, MoveCellsFromReader(read2));

  auto read3 =
      table.ReadRows(RowSet(RowRange::Empty()), Filter::PassAllFilter());
  CheckEqualUnordered(expected, MoveCellsFromReader(read3));
}

TEST_F(DataIntegrationTest, TableReadRowsWrongTable) {
  std::string const table_id = RandomTableId();

  Table table(data_client_, table_id);

  auto read1 = table.ReadRows(RowSet(RowRange::InfiniteRange()),
                              Filter::PassAllFilter());

  auto it = read1.begin();
  ASSERT_NE(read1.end(), it);
  EXPECT_FALSE(*it);
  ++it;
  EXPECT_EQ(read1.end(), it);
}

TEST_F(DataIntegrationTest, TableCheckAndMutateRowPass) {
  auto table = GetTable();
  std::string const key = "row-key";

  std::vector<Cell> created{{key, kFamily4, "c1", 0, "v1000"}};
  CreateCells(table, created);
  auto result =
      table.CheckAndMutateRow(key, Filter::ValueRegex("v1000"),
                              {SetCell(kFamily4, "c2", 0_ms, "v2000")},
                              {SetCell(kFamily4, "c3", 0_ms, "v3000")});
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(MutationBranch::kPredicateMatched, *result);
  std::vector<Cell> expected{{key, kFamily4, "c1", 0, "v1000"},
                             {key, kFamily4, "c2", 0, "v2000"}};
  auto actual = ReadRows(table, Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableCheckAndMutateRowFail) {
  auto table = GetTable();
  std::string const key = "row-key";

  std::vector<Cell> created{{key, kFamily4, "c1", 0, "v1000"}};
  CreateCells(table, created);
  auto result =
      table.CheckAndMutateRow(key, Filter::ValueRegex("not-there"),
                              {SetCell(kFamily4, "c2", 0_ms, "v2000")},
                              {SetCell(kFamily4, "c3", 0_ms, "v3000")});
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(MutationBranch::kPredicateNotMatched, *result);
  std::vector<Cell> expected{{key, kFamily4, "c1", 0, "v1000"},
                             {key, kFamily4, "c3", 0, "v3000"}};
  auto actual = ReadRows(table, Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableReadModifyWriteAppendValueTest) {
  auto table = GetTable();
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::string const add_suffix1 = "-suffix";
  std::string const add_suffix2 = "-next";
  std::string const add_suffix3 = "-newrecord";

  std::vector<Cell> created{{row_key1, kFamily1, "column-id1", 1000, "v1000"},
                            {row_key1, kFamily2, "column-id2", 2000, "v2000"},
                            {row_key1, kFamily3, "column-id1", 2000, "v3000"},
                            {row_key1, kFamily1, "column-id3", 2000, "v5000"}};

  std::vector<Cell> expected{
      {row_key1, kFamily1, "column-id1", 1000, "v1000" + add_suffix1},
      {row_key1, kFamily2, "column-id2", 2000, "v2000" + add_suffix2},
      {row_key1, kFamily3, "column-id3", 2000, add_suffix3}};

  CreateCells(table, created);
  auto result_row = table.ReadModifyWriteRow(
      row_key1,
      ReadModifyWriteRule::AppendValue(kFamily1, "column-id1", add_suffix1),
      ReadModifyWriteRule::AppendValue(kFamily2, "column-id2", add_suffix2),
      ReadModifyWriteRule::AppendValue(kFamily3, "column-id3", add_suffix3));
  ASSERT_STATUS_OK(result_row);
  // Returned cells contains timestamp in microseconds which is
  // not matching with the timestamp in expected cells, So creating
  // cells by ignoring timestamp
  auto expected_cells_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_cells_ignore_timestamp =
      GetCellsIgnoringTimestamp(result_row->cells());

  CheckEqualUnordered(expected_cells_ignore_timestamp,
                      actual_cells_ignore_timestamp);
}

TEST_F(DataIntegrationTest, TableReadModifyWriteRowIncrementAmountTest) {
  auto table = GetTable();
  std::string const key = "row-key";

  // An initial; big-endian int64 number with value 0.
  std::string v1("\x00\x00\x00\x00\x00\x00\x00\x00", 8);
  std::vector<Cell> created{{key, kFamily1, "c1", 0, v1}};

  // The expected values as buffers containing big-endian int64 numbers.
  std::string e1("\x00\x00\x00\x00\x00\x00\x00\x2A", 8);
  std::string e2("\x00\x00\x00\x00\x00\x00\x00\x07", 8);
  std::vector<Cell> expected{{key, kFamily1, "c1", 0, e1},
                             {key, kFamily1, "c2", 0, e2}};

  CreateCells(table, created);
  auto row = table.ReadModifyWriteRow(
      key, ReadModifyWriteRule::IncrementAmount(kFamily1, "c1", 42),
      ReadModifyWriteRule::IncrementAmount(kFamily1, "c2", 7));
  ASSERT_STATUS_OK(row);
  // Ignore the server set timestamp on the returned cells because it is not
  // predictable.
  auto expected_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_ignore_timestamp = GetCellsIgnoringTimestamp(row->cells());

  CheckEqualUnordered(expected_ignore_timestamp, actual_ignore_timestamp);
}

TEST_F(DataIntegrationTest, TableReadModifyWriteRowMultipleTest) {
  auto table = GetTable();
  std::string const key = "row-key";

  std::string v1("\x00\x00\x00\x00\x00\x00\x00\x00", 8);
  std::vector<Cell> created{{key, kFamily1, "c1", 0, v1},
                            {key, kFamily1, "c3", 0, "start;"},
                            {key, kFamily2, "d1", 0, v1},
                            {key, kFamily2, "d3", 0, "start;"}};

  // The expected values as buffers containing big-endian int64 numbers.
  std::string e1("\x00\x00\x00\x00\x00\x00\x00\x2A", 8);
  std::string e2("\x00\x00\x00\x00\x00\x00\x00\x07", 8);
  std::string e3("\x00\x00\x00\x00\x00\x00\x07\xD0", 8);
  std::string e4("\x00\x00\x00\x00\x00\x00\x0B\xB8", 8);
  std::vector<Cell> expected{{key, kFamily1, "c1", 0, e1},
                             {key, kFamily1, "c2", 0, e2},
                             {key, kFamily1, "c3", 0, "start;suffix"},
                             {key, kFamily1, "c4", 0, "suffix"},
                             {key, kFamily2, "d1", 0, e3},
                             {key, kFamily2, "d2", 0, e4},
                             {key, kFamily2, "d3", 0, "start;suffix"},
                             {key, kFamily2, "d4", 0, "suffix"}};

  CreateCells(table, created);
  using R = ReadModifyWriteRule;
  auto row =
      table.ReadModifyWriteRow(key, R::IncrementAmount(kFamily1, "c1", 42),
                               R::IncrementAmount(kFamily1, "c2", 7),
                               R::IncrementAmount(kFamily2, "d1", 2000),
                               R::IncrementAmount(kFamily2, "d2", 3000),
                               R::AppendValue(kFamily1, "c3", "suffix"),
                               R::AppendValue(kFamily1, "c4", "suffix"),
                               R::AppendValue(kFamily2, "d3", "suffix"),
                               R::AppendValue(kFamily2, "d4", "suffix"));
  ASSERT_STATUS_OK(row);
  // Ignore the server set timestamp on the returned cells because it is not
  // predictable.
  auto expected_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_ignore_timestamp = GetCellsIgnoringTimestamp(row->cells());

  CheckEqualUnordered(expected_ignore_timestamp, actual_ignore_timestamp);
}

TEST_F(DataIntegrationTest, TableCellValueInt64Test) {
  auto table = GetTable();
  std::string const key = "row-key";

  std::vector<Cell> created{{key, kFamily1, "c1", 0, 42},
                            {key, kFamily1, "c3", 0, "start;"},
                            {key, kFamily2, "d1", 0, 2},
                            {key, kFamily2, "d2", 0, 5012},
                            {key, kFamily2, "d3", 0, "start;"}};

  std::vector<Cell> expected{{key, kFamily1, "c1", 0, 40},
                             {key, kFamily1, "c2", 0, 7},
                             {key, kFamily1, "c3", 0, "start;suffix"},
                             {key, kFamily2, "d1", 0, 2002},
                             {key, kFamily2, "d2", 0, 9999998012},
                             {key, kFamily2, "d3", 0, "start;suffix"}};

  CreateCells(table, created);
  using R = ReadModifyWriteRule;
  auto row =
      table.ReadModifyWriteRow(key, R::IncrementAmount(kFamily1, "c1", -2),
                               R::IncrementAmount(kFamily1, "c2", 7),
                               R::IncrementAmount(kFamily2, "d1", 2000),
                               R::IncrementAmount(kFamily2, "d2", 9999993000),
                               R::AppendValue(kFamily1, "c3", "suffix"),
                               R::AppendValue(kFamily2, "d3", "suffix"));
  ASSERT_STATUS_OK(row);
  // Ignore the server set timestamp on the returned cells because it is not
  // predictable.
  auto expected_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_ignore_timestamp = GetCellsIgnoringTimestamp(row->cells());

  CheckEqualUnordered(expected_ignore_timestamp, actual_ignore_timestamp);
}

TEST_F(DataIntegrationTest, TableReadMultipleCellsBigValue) {
  if (UsingCloudBigtableEmulator()) {
    // TODO(#151) - remove workarounds for emulator bug(s).
    return;
  }

  auto table = GetTable();

  std::string const row_key = "row-key-1";
  // cell vector contains 4 cells of 32 MiB each, or 128 MiB (without
  // considering any overhead). That is much larger that the default gRPC
  // message size (~4 MiB), and yet much smaller than the configured message
  // size (~256MiB). Therefore, the row would not fit in a message if we failed
  // to change the default configuration, but it is not so large that it will
  // fail to work if we miss the overhead estimation.
  auto constexpr kMib = 1024 * 1024UL;
  auto constexpr kCellSize = 32 * kMib;
  auto constexpr kCellCount = 4;
  // Smaller rows than this size are not a good test, they would pass with the
  // default setting, so only accept rows that are at least 10x the default
  // setting of 4 MiB.
  auto const min_row_size = 10 * 4 * kMib;
  // Larger rows than this size are not a good test, they would fail even if the
  // setting was working.
  auto const max_row_size = 256 * kMib;

  std::string value(kCellSize, 'a');
  std::vector<Cell> created;
  std::vector<Cell> expected;

  for (int i = 0; i < kCellCount; i++) {
    auto col_qualifier = "c" + std::to_string(i);
    created.emplace_back(row_key, kFamily4, col_qualifier, 0, value);
    expected.emplace_back(row_key, kFamily4, col_qualifier, 0, value);
  }

  CreateCells(table, created);

  auto result = table.ReadRow(row_key, Filter::PassAllFilter());
  ASSERT_STATUS_OK(result);
  EXPECT_TRUE(result->first);

  std::size_t total_row_size = 0;
  for (auto const& cell : result->second.cells()) {
    total_row_size += cell.value().size();
  }
  EXPECT_LT(total_row_size, max_row_size);
  EXPECT_GT(total_row_size, min_row_size);

  // Ignore the server set timestamp on the returned cells because it is not
  // predictable.
  auto expected_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_ignore_timestamp =
      GetCellsIgnoringTimestamp(result->second.cells());
  CheckEqualUnordered(expected_ignore_timestamp, actual_ignore_timestamp);
}

TEST_F(DataIntegrationTest, TableApplyWithLogging) {
  testing_util::ScopedLog log;
  std::string const table_id = RandomTableId();

  auto constexpr kTestMaxVersions = 10;
  auto const test_gc_rule = bigtable::GcRule::MaxNumVersions(kTestMaxVersions);
  bigtable::TableConfig table_config = bigtable::TableConfig(
      {
          {kFamily1, test_gc_rule},
          {kFamily2, test_gc_rule},
          {kFamily3, test_gc_rule},
          {kFamily4, test_gc_rule},
      },
      {});
  ASSERT_STATUS_OK(table_admin_->CreateTable(table_id, table_config));

  std::shared_ptr<bigtable::DataClient> data_client =
      bigtable::CreateDefaultDataClient(project_id(), instance_id(),
                                        ClientOptions().enable_tracing("rpc"));

  Table table(data_client, table_id);

  std::string const row_key = "row-key-1";
  std::vector<Cell> created{{row_key, kFamily4, "c0", 1000, "v1000"},
                            {row_key, kFamily4, "c1", 2000, "v2000"}};
  Apply(table, row_key, created);
  std::vector<Cell> expected{{row_key, kFamily4, "c0", 1000, "v1000"},
                             {row_key, kFamily4, "c1", 2000, "v2000"}};

  auto actual = ReadRows(table, Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);
  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("MutateRow")));

  ASSERT_STATUS_OK(table_admin_->DeleteTable(table_id));
}

TEST(ConnectionRefresh, Disabled) {
  auto client_options = bigtable::ClientOptions().set_max_conn_refresh_period(
      std::chrono::seconds(0));
  auto data_client = bigtable::CreateDefaultDataClient(
      testing::TableTestEnvironment::project_id(),
      testing::TableTestEnvironment::instance_id(), client_options);
  // In general, it is hard to show that something has *not* happened, at best
  // we can show that its side-effects have not happened. In this case we want
  // to show that the channels have not been refreshed. A side-effect of
  // refreshing a channel is that it enters the `READY` state, so we check that
  // this has not taken place and take that as evidence that no refresh has
  // taken place.
  //
  // After the `CompletionQueue` argument is removed from the `Bigtable` API, we
  // will have an option to provide a mock `CompletionQueue` to the `DataClient`
  // for test purposes and verify that no timers are created, which will be a
  // superior way to write this test.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  for (std::size_t i = 0; i < client_options.connection_pool_size(); ++i) {
    auto channel = data_client->Channel();
    EXPECT_EQ(GRPC_CHANNEL_IDLE, channel->GetState(false));
  }
  // Make sure things still work.
  bigtable::Table table(data_client, testing::TableTestEnvironment::table_id());
  std::string const row_key = "row-key-1";
  std::vector<Cell> created{{row_key, kFamily4, "c0", 1000, "v1000"},
                            {row_key, kFamily4, "c1", 2000, "v2000"}};
  Apply(table, row_key, created);
  // After performing some operations, some of the channels should be in ready
  // state.
  auto check_if_some_channel_is_ready = [&] {
    for (std::size_t i = 0; i < client_options.connection_pool_size(); ++i) {
      if (data_client->Channel()->GetState(false) == GRPC_CHANNEL_READY) {
        return true;
      }
    }
    return false;
  };
  EXPECT_TRUE(check_if_some_channel_is_ready());
}

TEST(ConnectionRefresh, Frequent) {
  auto data_client = bigtable::CreateDefaultDataClient(
      testing::TableTestEnvironment::project_id(),
      testing::TableTestEnvironment::instance_id(),
      bigtable::ClientOptions().set_max_conn_refresh_period(
          std::chrono::milliseconds(100)));

  for (;;) {
    if (data_client->Channel()->GetState(false) == GRPC_CHANNEL_READY) {
      // We've found a channel which changed its state from IDLE to READY,
      // which means that our refreshing mechanism works.
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Make sure things still work.
  bigtable::Table table(data_client, testing::TableTestEnvironment::table_id());
  std::string const row_key = "row-key-1";
  std::vector<Cell> created{{row_key, kFamily4, "c0", 1000, "v1000"},
                            {row_key, kFamily4, "c1", 2000, "v2000"}};
  Apply(table, row_key, created);
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new ::google::cloud::bigtable::testing::TableTestEnvironment);

  return RUN_ALL_TESTS();
}
