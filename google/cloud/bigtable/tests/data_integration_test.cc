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

#include "google/cloud/bigtable/internal/endian.h"
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/init_google_mock.h"

namespace {
namespace bigtable = google::cloud::bigtable;

class DataIntegrationTest : public bigtable::testing::TableIntegrationTest {
 protected:
  /// Use Table::Apply() to insert a single row.
  void Apply(bigtable::Table& table, std::string row_key,
             std::vector<bigtable::Cell> const& cells);

  /// Use Table::BulkApply() to insert multiple rows.
  void BulkApply(bigtable::Table& table,
                 std::vector<bigtable::Cell> const& cells);

  std::string const family = "family";
  std::string const family1 = "family1";
  std::string const family2 = "family2";
  std::string const family3 = "family3";
  bigtable::TableConfig table_config =
      bigtable::TableConfig({{family, bigtable::GcRule::MaxNumVersions(10)},
                             {family1, bigtable::GcRule::MaxNumVersions(10)},
                             {family2, bigtable::GcRule::MaxNumVersions(10)},
                             {family3, bigtable::GcRule::MaxNumVersions(10)}},
                            {});
};

bool UsingCloudBigtableEmulator() {
  return google::cloud::internal::GetEnv("BIGTABLE_EMULATOR_HOST").has_value();
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];

  (void)::testing::AddGlobalTestEnvironment(
      new ::bigtable::testing::TableTestEnvironment(project_id, instance_id));

  return RUN_ALL_TESTS();
}

namespace {

void DataIntegrationTest::Apply(bigtable::Table& table, std::string row_key,
                                std::vector<bigtable::Cell> const& cells) {
  using namespace std::chrono;
  auto mutation = bigtable::SingleRowMutation(row_key);
  for (auto const& cell : cells) {
    mutation.emplace_back(bigtable::SetCell(
        cell.family_name(), cell.column_qualifier(),
        duration_cast<milliseconds>(microseconds(cell.timestamp())),
        cell.value()));
  }
  table.Apply(std::move(mutation));
}

void DataIntegrationTest::BulkApply(bigtable::Table& table,
                                    std::vector<bigtable::Cell> const& cells) {
  using namespace std::chrono;
  std::map<std::string, bigtable::SingleRowMutation> mutations;
  for (auto const& cell : cells) {
    std::string key = cell.row_key();
    auto inserted = mutations.emplace(key, bigtable::SingleRowMutation(key));
    inserted.first->second.emplace_back(bigtable::SetCell(
        cell.family_name(), cell.column_qualifier(),
        duration_cast<milliseconds>(microseconds(cell.timestamp())),
        cell.value()));
  }
  bigtable::BulkMutation bulk;
  for (auto& kv : mutations) {
    bulk.emplace_back(std::move(kv.second));
  }
  table.BulkApply(std::move(bulk));
}
}  // anonymous namespace

using namespace google::cloud::testing_util::chrono_literals;

TEST_F(DataIntegrationTest, TableApply) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);

  std::string const row_key = "row-key-1";
  std::vector<bigtable::Cell> created{
      {row_key, family, "c0", 1000, "v1000", {}},
      {row_key, family, "c1", 2000, "v2000", {}}};
  Apply(*table, row_key, created);
  std::vector<bigtable::Cell> expected{
      {row_key, family, "c0", 1000, "v1000", {}},
      {row_key, family, "c1", 2000, "v2000", {}}};

  auto actual = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_id);
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableBulkApply) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);

  std::vector<bigtable::Cell> created{
      {"row-key-1", family, "c0", 1000, "v1000", {}},
      {"row-key-1", family, "c1", 2000, "v2000", {}},
      {"row-key-2", family, "c0", 1000, "v1000", {}},
      {"row-key-2", family, "c1", 2000, "v2000", {}},
      {"row-key-3", family, "c0", 1000, "v1000", {}},
      {"row-key-3", family, "c1", 2000, "v2000", {}},
      {"row-key-4", family, "c0", 1000, "v1000", {}},
      {"row-key-4", family, "c1", 2000, "v2000", {}}};
  BulkApply(*table, created);
  std::vector<bigtable::Cell> expected{
      {"row-key-1", family, "c0", 1000, "v1000", {}},
      {"row-key-1", family, "c1", 2000, "v2000", {}},
      {"row-key-2", family, "c0", 1000, "v1000", {}},
      {"row-key-2", family, "c1", 2000, "v2000", {}},
      {"row-key-3", family, "c0", 1000, "v1000", {}},
      {"row-key-3", family, "c1", 2000, "v2000", {}},
      {"row-key-4", family, "c0", 1000, "v1000", {}},
      {"row-key-4", family, "c1", 2000, "v2000", {}}};

  auto actual = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_id);
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableSingleRow) {
  std::string const table_id = RandomTableId();
  std::string const row_key = "row-key-1";
  auto table = CreateTable(table_id, table_config);

  auto mutation = bigtable::SingleRowMutation(
      row_key, bigtable::SetCell(family, "c1", 1_ms, "V1000"),
      bigtable::SetCell(family, "c2", 2_ms, "V2000"),
      bigtable::SetCell(family, "c3", 3_ms, "V3000"));
  table->Apply(std::move(mutation));
  std::vector<bigtable::Cell> expected{
      {row_key, family, "c1", 1000, "V1000", {}},
      {row_key, family, "c2", 2000, "V2000", {}},
      {row_key, family, "c3", 3000, "V3000", {}}};

  auto actual = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_id);
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableReadRowTest) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";

  std::vector<bigtable::Cell> created{
      {row_key1, family, "c1", 1000, "v1000", {}},
      {row_key2, family, "c2", 2000, "v2000", {}}};
  std::vector<bigtable::Cell> expected{
      {row_key1, family, "c1", 1000, "v1000", {}}};

  CreateCells(*table, created);
  auto row_cell = table->ReadRow(row_key1, bigtable::Filter::PassAllFilter());
  std::vector<bigtable::Cell> actual;
  actual.emplace_back(row_cell.second.cells().at(0));
  DeleteTable(table_id);
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableReadRowNotExistTest) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";

  std::vector<bigtable::Cell> created{
      {row_key1, family, "c1", 1000, "v1000", {}}};

  CreateCells(*table, created);
  auto row_cell = table->ReadRow(row_key2, bigtable::Filter::PassAllFilter());
  DeleteTable(table_id);
  EXPECT_FALSE(row_cell.first);
}

TEST_F(DataIntegrationTest, TableReadRowsAllRows) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::string const row_key3(1024, '3');    // a long key
  std::string const long_value(1024, 'v');  // a long value

  std::vector<bigtable::Cell> created{
      {row_key1, family, "c1", 1000, "data1", {}},
      {row_key1, family, "c2", 1000, "data2", {}},
      {row_key2, family, "c1", 1000, "", {}},
      {row_key3, family, "c1", 1000, long_value, {}}};

  CreateCells(*table, created);

  // Some equivalent ways to read the three rows
  auto read1 =
      table->ReadRows(bigtable::RowSet(bigtable::RowRange::InfiniteRange()),
                      bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(created, MoveCellsFromReader(read1));

  auto read2 =
      table->ReadRows(bigtable::RowSet(bigtable::RowRange::InfiniteRange()), 3,
                      bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(created, MoveCellsFromReader(read2));

  auto read3 = table->ReadRows(
      bigtable::RowSet(bigtable::RowRange::InfiniteRange()),
      bigtable::RowReader::NO_ROWS_LIMIT, bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(created, MoveCellsFromReader(read3));

  if (!UsingCloudBigtableEmulator()) {
    // TODO(#151) - remove workarounds for emulator bug(s).
    auto read4 =
        table->ReadRows(bigtable::RowSet(), bigtable::Filter::PassAllFilter());
    CheckEqualUnordered(created, MoveCellsFromReader(read4));
  }
  DeleteTable(table_id);
}

TEST_F(DataIntegrationTest, TableReadRowsPartialRows) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::string const row_key3 = "row-key-3";

  std::vector<bigtable::Cell> created{
      {row_key1, family, "c1", 1000, "data1", {}},
      {row_key1, family, "c2", 1000, "data2", {}},
      {row_key2, family, "c1", 1000, "data3", {}},
      {row_key3, family, "c1", 1000, "data4", {}}};

  CreateCells(*table, created);

  std::vector<bigtable::Cell> expected{
      {row_key1, family, "c1", 1000, "data1", {}},
      {row_key1, family, "c2", 1000, "data2", {}},
      {row_key2, family, "c1", 1000, "data3", {}}};

  // Some equivalent ways of reading just the first two rows
  auto read1 =
      table->ReadRows(bigtable::RowSet(bigtable::RowRange::InfiniteRange()), 2,
                      bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected, MoveCellsFromReader(read1));

  bigtable::RowSet rs2;
  rs2.Append(row_key1);
  rs2.Append(row_key2);
  auto read2 =
      table->ReadRows(std::move(rs2), bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected, MoveCellsFromReader(read2));

  bigtable::RowSet rs3(bigtable::RowRange::Closed(row_key1, row_key2));
  auto read3 =
      table->ReadRows(std::move(rs3), bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected, MoveCellsFromReader(read3));

  DeleteTable(table_id);
}

TEST_F(DataIntegrationTest, TableReadRowsNoRows) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::string const row_key3 = "row-key-3";

  std::vector<bigtable::Cell> created{
      {row_key1, family, "c1", 1000, "data1", {}},
      {row_key3, family, "c1", 1000, "data2", {}}};

  CreateCells(*table, created);

  std::vector<bigtable::Cell> expected;  // empty

  // read nonexistent rows
  auto read1 = table->ReadRows(bigtable::RowSet(row_key2),
                               bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected, MoveCellsFromReader(read1));

  auto read2 =
      table->ReadRows(bigtable::RowSet(bigtable::RowRange::Prefix(row_key2)),
                      bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected, MoveCellsFromReader(read2));

  auto read3 = table->ReadRows(bigtable::RowSet(bigtable::RowRange::Empty()),
                               bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected, MoveCellsFromReader(read3));

  DeleteTable(table_id);
}

TEST_F(DataIntegrationTest, TableReadRowsWrongTable) {
  std::string const table_id = RandomTableId();

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  bigtable::Table table(data_client_, table_id);
#else
  bigtable::noex::Table table(data_client_, table_id);
#endif

  auto read1 =
      table.ReadRows(bigtable::RowSet(bigtable::RowRange::InfiniteRange()),
                     bigtable::Filter::PassAllFilter());

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(read1.begin(), std::runtime_error);
#else
  EXPECT_EQ(read1.begin(), read1.end());
  grpc::Status status1 = read1.Finish();
  EXPECT_FALSE(status1.ok());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(DataIntegrationTest, TableCheckAndMutateRowPass) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);
  std::string const key = "row-key";

  std::vector<bigtable::Cell> created{{key, family, "c1", 0, "v1000", {}}};
  CreateCells(*table, created);
  auto result = table->CheckAndMutateRow(
      key, bigtable::Filter::ValueRegex("v1000"),
      {bigtable::SetCell(family, "c2", 0_ms, "v2000")},
      {bigtable::SetCell(family, "c3", 0_ms, "v3000")});
  EXPECT_TRUE(result);
  std::vector<bigtable::Cell> expected{{key, family, "c1", 0, "v1000", {}},
                                       {key, family, "c2", 0, "v2000", {}}};
  auto actual = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_id);
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableCheckAndMutateRowFail) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);
  std::string const key = "row-key";

  std::vector<bigtable::Cell> created{{key, family, "c1", 0, "v1000", {}}};
  CreateCells(*table, created);
  auto result = table->CheckAndMutateRow(
      key, bigtable::Filter::ValueRegex("not-there"),
      {bigtable::SetCell(family, "c2", 0_ms, "v2000")},
      {bigtable::SetCell(family, "c3", 0_ms, "v3000")});
  EXPECT_FALSE(result);
  std::vector<bigtable::Cell> expected{{key, family, "c1", 0, "v1000", {}},
                                       {key, family, "c3", 0, "v3000", {}}};
  auto actual = ReadRows(*table, bigtable::Filter::PassAllFilter());
  DeleteTable(table_id);
  CheckEqualUnordered(expected, actual);
}

TEST_F(DataIntegrationTest, TableReadModifyWriteAppendValueTest) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);
  std::string const row_key1 = "row-key-1";
  std::string const row_key2 = "row-key-2";
  std::string const add_suffix1 = "-suffix";
  std::string const add_suffix2 = "-next";
  std::string const add_suffix3 = "-newrecord";

  std::vector<bigtable::Cell> created{
      {row_key1, family1, "column-id1", 1000, "v1000", {}},
      {row_key1, family2, "column-id2", 2000, "v2000", {}},
      {row_key1, family3, "column-id1", 2000, "v3000", {}},
      {row_key1, family1, "column-id3", 2000, "v5000", {}}};

  std::vector<bigtable::Cell> expected{
      {row_key1, family1, "column-id1", 1000, "v1000" + add_suffix1, {}},
      {row_key1, family2, "column-id2", 2000, "v2000" + add_suffix2, {}},
      {row_key1, family3, "column-id3", 2000, add_suffix3, {}}};

  CreateCells(*table, created);
  auto result_row =
      table->ReadModifyWriteRow(row_key1,
                                bigtable::ReadModifyWriteRule::AppendValue(
                                    family1, "column-id1", add_suffix1),
                                bigtable::ReadModifyWriteRule::AppendValue(
                                    family2, "column-id2", add_suffix2),
                                bigtable::ReadModifyWriteRule::AppendValue(
                                    family3, "column-id3", add_suffix3));

  // Returned cells contains timestamp in microseconds which is
  // not matching with the timestamp in expected cells, So creating
  // cells by ignoring timestamp
  auto expected_cells_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_cells_ignore_timestamp =
      GetCellsIgnoringTimestamp(result_row.cells());

  DeleteTable(table_id);
  CheckEqualUnordered(expected_cells_ignore_timestamp,
                      actual_cells_ignore_timestamp);
}

TEST_F(DataIntegrationTest, TableReadModifyWriteRowIncrementAmountTest) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);
  std::string const key = "row-key";

  // An initial; BigEndian int64 number with value 0.
  std::string v1("\x00\x00\x00\x00\x00\x00\x00\x00", 8);
  std::vector<bigtable::Cell> created{{key, family1, "c1", 0, v1, {}}};

  // The expected values as buffers containing BigEndian int64 numbers.
  std::string e1("\x00\x00\x00\x00\x00\x00\x00\x2A", 8);
  std::string e2("\x00\x00\x00\x00\x00\x00\x00\x07", 8);
  std::vector<bigtable::Cell> expected{{key, family1, "c1", 0, e1, {}},
                                       {key, family1, "c2", 0, e2, {}}};

  CreateCells(*table, created);
  auto row = table->ReadModifyWriteRow(
      key, bigtable::ReadModifyWriteRule::IncrementAmount(family1, "c1", 42),
      bigtable::ReadModifyWriteRule::IncrementAmount(family1, "c2", 7));

  // Ignore the server set timestamp on the returned cells because it is not
  // predictable.
  auto expected_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_ignore_timestamp = GetCellsIgnoringTimestamp(row.cells());

  DeleteTable(table_id);
  CheckEqualUnordered(expected_ignore_timestamp, actual_ignore_timestamp);
}

TEST_F(DataIntegrationTest, TableReadModifyWriteRowMultipleTest) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);
  std::string const key = "row-key";

  std::string v1("\x00\x00\x00\x00\x00\x00\x00\x00", 8);
  std::vector<bigtable::Cell> created{{key, family1, "c1", 0, v1, {}},
                                      {key, family1, "c3", 0, "start;", {}},
                                      {key, family2, "d1", 0, v1, {}},
                                      {key, family2, "d3", 0, "start;", {}}};

  // The expected values as buffers containing BigEndian int64 numbers.
  std::string e1("\x00\x00\x00\x00\x00\x00\x00\x2A", 8);
  std::string e2("\x00\x00\x00\x00\x00\x00\x00\x07", 8);
  std::string e3("\x00\x00\x00\x00\x00\x00\x07\xD0", 8);
  std::string e4("\x00\x00\x00\x00\x00\x00\x0B\xB8", 8);
  std::vector<bigtable::Cell> expected{
      {key, family1, "c1", 0, e1, {}},
      {key, family1, "c2", 0, e2, {}},
      {key, family1, "c3", 0, "start;suffix", {}},
      {key, family1, "c4", 0, "suffix", {}},
      {key, family2, "d1", 0, e3, {}},
      {key, family2, "d2", 0, e4, {}},
      {key, family2, "d3", 0, "start;suffix", {}},
      {key, family2, "d4", 0, "suffix", {}}};

  CreateCells(*table, created);
  using R = bigtable::ReadModifyWriteRule;
  auto row =
      table->ReadModifyWriteRow(key, R::IncrementAmount(family1, "c1", 42),
                                R::IncrementAmount(family1, "c2", 7),
                                R::IncrementAmount(family2, "d1", 2000),
                                R::IncrementAmount(family2, "d2", 3000),
                                R::AppendValue(family1, "c3", "suffix"),
                                R::AppendValue(family1, "c4", "suffix"),
                                R::AppendValue(family2, "d3", "suffix"),
                                R::AppendValue(family2, "d4", "suffix"));

  // Ignore the server set timestamp on the returned cells because it is not
  // predictable.
  auto expected_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_ignore_timestamp = GetCellsIgnoringTimestamp(row.cells());

  DeleteTable(table_id);
  CheckEqualUnordered(expected_ignore_timestamp, actual_ignore_timestamp);
}

TEST_F(DataIntegrationTest, TableCellValueInt64Test) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);
  std::string const key = "row-key";

  std::vector<bigtable::Cell> created{
      {key, family1, "c1", 0, bigtable::bigendian64_t(42), {}},
      {key, family1, "c3", 0, "start;", {}},
      {key, family2, "d1", 0, bigtable::bigendian64_t(2), {}},
      {key, family2, "d2", 0, bigtable::bigendian64_t(5012), {}},
      {key, family2, "d3", 0, "start;", {}}};

  std::vector<bigtable::Cell> expected{
      {key, family1, "c1", 0, bigtable::bigendian64_t(40), {}},
      {key, family1, "c2", 0, bigtable::bigendian64_t(7), {}},
      {key, family1, "c3", 0, "start;suffix", {}},
      {key, family2, "d1", 0, bigtable::bigendian64_t(2002), {}},
      {key, family2, "d2", 0, bigtable::bigendian64_t(9999998012), {}},
      {key, family2, "d3", 0, "start;suffix", {}}};

  CreateCells(*table, created);
  using R = bigtable::ReadModifyWriteRule;
  auto row =
      table->ReadModifyWriteRow(key, R::IncrementAmount(family1, "c1", -2),
                                R::IncrementAmount(family1, "c2", 7),
                                R::IncrementAmount(family2, "d1", 2000),
                                R::IncrementAmount(family2, "d2", 9999993000),
                                R::AppendValue(family1, "c3", "suffix"),
                                R::AppendValue(family2, "d3", "suffix"));

  // Ignore the server set timestamp on the returned cells because it is not
  // predictable.
  auto expected_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_ignore_timestamp = GetCellsIgnoringTimestamp(row.cells());

  DeleteTable(table_id);
  CheckEqualUnordered(expected_ignore_timestamp, actual_ignore_timestamp);
}

TEST_F(DataIntegrationTest, TableSampleRowKeysTest) {
  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);

  // Create BATCH_SIZE * BATCH_COUNT rows.
  constexpr int BATCH_COUNT = 10;
  constexpr int BATCH_SIZE = 5000;
  constexpr int COLUMN_COUNT = 10;
  int rowid = 0;
  for (int batch = 0; batch != BATCH_COUNT; ++batch) {
    bigtable::BulkMutation bulk;
    for (int row = 0; row != BATCH_SIZE; ++row) {
      std::ostringstream os;
      os << "row:" << std::setw(9) << std::setfill('0') << rowid;

      // Build a mutation that creates 10 columns.
      bigtable::SingleRowMutation mutation(os.str());
      for (int col = 0; col != COLUMN_COUNT; ++col) {
        std::string colid = "c" + std::to_string(col);
        std::string value = colid + "#" + os.str();
        mutation.emplace_back(
            bigtable::SetCell(family1, std::move(colid), std::move(value)));
      }
      bulk.emplace_back(std::move(mutation));
      ++rowid;
    }
    table->BulkApply(std::move(bulk));
  }
  auto samples = table->SampleRows<std::vector>();
  DeleteTable(table_id);

  // It is somewhat hard to verify that the values returned here are correct.
  // We cannot check the specific values, not even the format, of the row keys
  // because Cloud Bigtable might return an empty row key (for "end of table"),
  // and it might return row keys that have never been written to.
  // All we can check is that this is not empty, and that the offsets are in
  // ascending order.
  EXPECT_FALSE(samples.empty());
  std::int64_t previous = 0;
  for (auto const& s : samples) {
    EXPECT_LE(previous, s.offset_bytes);
    previous = s.offset_bytes;
  }
  // At least one of the samples should have non-zero offset:
  auto last = samples.back();
  EXPECT_LT(0, last.offset_bytes);
}

TEST_F(DataIntegrationTest, TableReadMultipleCellsBigValue) {
  if (UsingCloudBigtableEmulator()) {
    // TODO(#151) - remove workarounds for emulator bug(s).
    return;
  }

  std::string const table_id = RandomTableId();
  auto table = CreateTable(table_id, table_config);

  std::string const row_key = "row-key-1";
  // cell vector contains 10 cells of 25 MiB
  auto const MiB = 1024L * 1024L;

  std::string value((25 * MiB), 'a');
  std::vector<bigtable::Cell> created;
  std::vector<bigtable::Cell> expected;

  std::string col_qualifier;
  for (int i = 0; i < 10; i++) {
    col_qualifier = "c" + std::to_string(i);
    created.push_back(
        bigtable::Cell(row_key, family, col_qualifier, 0, value, {}));
    expected.push_back(
        bigtable::Cell(row_key, family, col_qualifier, 0, value, {}));
  }

  CreateCells(*table, created);
  auto result = table->ReadRow(row_key, bigtable::Filter::PassAllFilter());
  EXPECT_TRUE(result.first);

  int totalrowsize = 0;
  for (auto const& cell : result.second.cells()) {
    totalrowsize += cell.value().size();
  }
  EXPECT_LE(totalrowsize, (256 * MiB));

  // Ignore the server set timestamp on the returned cells because it is not
  // predictable.
  auto expected_ignore_timestamp = GetCellsIgnoringTimestamp(expected);
  auto actual_ignore_timestamp =
      GetCellsIgnoringTimestamp(result.second.cells());
  DeleteTable(table_id);
  CheckEqualUnordered(expected_ignore_timestamp, actual_ignore_timestamp);
}
