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

#include "bigtable/client/filters.h"

#include <cmath>
#include <cstdlib>
#include <sstream>

#include <google/protobuf/text_format.h>

#include <absl/memory/memory.h>
#include <absl/strings/str_join.h>

#include <gmock/gmock.h>

#include "bigtable/admin/admin_client.h"
#include "bigtable/admin/table_admin.h"
#include "bigtable/client/cell.h"
#include "bigtable/client/data_client.h"
#include "bigtable/client/table.h"

namespace btproto = ::google::bigtable::v2;
namespace admin_proto = ::google::bigtable::admin::v2;

namespace {
/// Store the project and instance captured from the command-line arguments.
class FilterTestEnvironment : public ::testing::Environment {
 public:
  FilterTestEnvironment(std::string project, std::string instance) {
    project_id_ = std::move(project);
    instance_id_ = std::move(instance);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& instance_id() { return instance_id_; }

 private:
  static std::string project_id_;
  static std::string instance_id_;
};

class FilterIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override;

  std::unique_ptr<bigtable::Table> CreateTable(std::string const& table_id);

  /**
   * Return all the cells included in @p request.
   *
   * TODO(#32) remove this when Table::ReadRows() is a thing.
   */
  std::vector<bigtable::Cell> ReadRows(bigtable::Table& table,
                                       btproto::ReadRowsRequest request);

  /**
   * Return all the cells in the row pointed by @p rowkey, applying @p filter.
   *
   * TODO(#29) remove this when Table::ReadRow() is a thing.
   */
  std::vector<bigtable::Cell> ReadRow(bigtable::Table& table,
                                      std::string rowkey,
                                      bigtable::Filter filter);

  /**
   * Return all the cells in @p table that pass @p filter.
   */
  std::vector<bigtable::Cell> ReadRows(bigtable::Table& table,
                                       bigtable::Filter filter);

  void CreateCells(bigtable::Table& table,
                   std::vector<bigtable::Cell> const& cells);

  /**
   * Create some complex rows in @p table.
   *
   * Create the following rows in @p table, the magic values for the column
   * families are defined above.
   *
   * | Row Key                 | Family | Column | Contents      |
   * | :---------------------- | :----- | :----- | :------------ |
   * | "{prefix}/one-cell"     | fam0   | c      | cell @ 3000 |
   * | "{prefix}/two-cells"    | fam0   | c      | cell @ 3000 |
   * | "{prefix}/two-cells"    | fam0   | c2     | cell @ 3000 |
   * | "{prefix}/many"         | fam0   | c      | cells @ 0, 1000, 2000, 3000 |
   * | "{prefix}/many-columns" | fam0   | c0     | cell @ 3000 |
   * | "{prefix}/many-columns" | fam0   | c1     | cell @ 3000 |
   * | "{prefix}/many-columns" | fam0   | c2     | cell @ 3000 |
   * | "{prefix}/many-columns" | fam0   | c3     | cell @ 3000 |
   * | "{prefix}/complex"      | fam0   | col0   | cell @ 3000, 6000 |
   * | "{prefix}/complex"      | fam0   | col1   | cell @ 3000, 6000 |
   * | "{prefix}/complex"      | fam0   | ...    | cell @ 3000, 6000 |
   * | "{prefix}/complex"      | fam0   | col9   | cell @ 3000, 6000 |
   * | "{prefix}/complex"      | fam1   | col0   | cell @ 3000, 6000 |
   * | "{prefix}/complex"      | fam1   | col1   | cell @ 3000, 6000 |
   * | "{prefix}/complex"      | fam1   | ...    | cell @ 3000, 6000 |
   * | "{prefix}/complex"      | fam1   | col9   | cell @ 3000, 6000 |
   *
   */
  void CreateComplexRows(bigtable::Table& table, std::string const& prefix);

  std::shared_ptr<bigtable::AdminClient> admin_client_;
  std::unique_ptr<bigtable::TableAdmin> table_admin_;
  std::shared_ptr<bigtable::DataClient> data_client_;
  std::string fam0 = "fam0";
  std::string fam1 = "fam1";
  std::string fam2 = "fam2";
  std::string fam3 = "fam3";
};
}  // anonymous namespace

int main(int argc, char* argv[]) try {
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
      bigtable::CreateAdminClient(project_id, bigtable::ClientOptions());
  bigtable::TableAdmin admin(admin_client, instance_id);

  auto table_list = admin.ListTables(admin_proto::Table::NAME_ONLY);
  if (not table_list.empty()) {
    std::ostringstream os;
    os << "Expected empty instance at the beginning of integration test";
    throw std::runtime_error(os.str());
  }

  (void)::testing::AddGlobalTestEnvironment(
      new FilterTestEnvironment(project_id, instance_id));

  return RUN_ALL_TESTS();
} catch (bigtable::PermanentMutationFailure const& ex) {
  std::cerr << "bigtable::PermanentMutationFailure raised: " << ex.what()
            << " - " << ex.status().error_message() << " ["
            << ex.status().error_code()
            << "], details=" << ex.status().error_details() << std::endl;
  int count = 0;
  for (auto const& failure : ex.failures()) {
    std::cerr << "failure[" << count++
              << "] {key=" << failure.mutation().row_key() << "}" << std::endl;
  }
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
} catch (...) {
  std::cerr << "Unknown exception raised." << std::endl;
  return 1;
}

namespace {
/**
 * Compare two cells, think about the spaceship operator.
 *
 * @return `< 0` if lhs < rhs, `0` if lhs == rhs, and `> 0` otherwise.
 */
int CellCompare(bigtable::Cell const& lhs, bigtable::Cell const& rhs);

/**
 * Compare two sets of cells.
 *
 * Unordered because ReadRows does not guarantee a particular order.
 */
void CheckEqualUnordered(std::vector<bigtable::Cell> expected,
                         std::vector<bigtable::Cell> actual);

/// Return true if connected to the Cloud Bigtable Emulator.
bool UsingCloudBigtableEmulator();
}  // anonymous namespace

namespace bigtable {
//@{
/// @name Helpers for GTest.
bool operator==(Cell const& lhs, Cell const& rhs) {
  return CellCompare(lhs, rhs) == 0;
}

bool operator<(Cell const& lhs, Cell const& rhs) {
  return CellCompare(lhs, rhs) < 0;
}

void PrintTo(bigtable::Cell const& cell, std::ostream* os) {
  *os << "  row_key=" << cell.row_key() << ", family=" << cell.family_name()
      << ", column=" << cell.column_qualifier()
      << ", timestamp=" << cell.timestamp() << ", value=" << cell.value()
      << ", labels={" << absl::StrJoin(cell.labels(), ",") << "}";
}
//@}
}  // namespace bigtable

TEST_F(FilterIntegrationTest, PassAll) {
  auto table = CreateTable("pass-all-filter-table");
  std::string const row_key = "pass-all-row-key";
  std::vector<bigtable::Cell> expected{
      {row_key, "fam0", "c", 0, "v-c-0-0", {}},
      {row_key, "fam0", "c", 1000, "v-c-0-1", {}},
      {row_key, "fam0", "c", 2000, "v-c-0-2", {}},
      {row_key, "fam1", "c0", 0, "v-c0-0-0", {}},
      {row_key, "fam1", "c1", 1000, "v-c1-0-1", {}},
      {row_key, "fam1", "c1", 2000, "v-c1-0-2", {}},
  };
  CreateCells(*table, expected);

  auto actual = ReadRow(*table, row_key, bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, BlockAll) {
  // TODO(#151) - remove workarounds for emulator bug(s).
  if (UsingCloudBigtableEmulator()) {
    return;
  }
  auto table = CreateTable("block-all-filter-table");
  std::string const row_key = "block-all-row-key";
  std::vector<bigtable::Cell> created{
      {row_key, "fam0", "c", 0, "v-c-0-0", {}},
      {row_key, "fam0", "c", 1000, "v-c-0-1", {}},
      {row_key, "fam0", "c", 2000, "v-c-0-2", {}},
      {row_key, "fam1", "c0", 0, "v-c0-0-0", {}},
      {row_key, "fam1", "c1", 1000, "v-c1-0-1", {}},
      {row_key, "fam1", "c1", 2000, "v-c1-0-2", {}},
  };
  CreateCells(*table, created);

  std::vector<bigtable::Cell> expected{};
  auto actual = ReadRow(*table, row_key, bigtable::Filter::BlockAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, Latest) {
  auto table = CreateTable("latest-filter-table");
  std::string const row_key = "latest-row-key";
  std::vector<bigtable::Cell> created{
      {row_key, "fam0", "c", 0, "v-c-0-0", {}},
      {row_key, "fam0", "c", 1000, "v-c-0-1", {}},
      {row_key, "fam0", "c", 2000, "v-c-0-2", {}},
      {row_key, "fam1", "c0", 0, "v-c0-0-0", {}},
      {row_key, "fam1", "c1", 1000, "v-c1-0-1", {}},
      {row_key, "fam1", "c1", 2000, "v-c1-0-2", {}},
      {row_key, "fam1", "c1", 3000, "v-c1-0-3", {}},
  };
  CreateCells(*table, created);

  std::vector<bigtable::Cell> expected{
      {row_key, "fam0", "c", 1000, "v-c-0-1", {}},
      {row_key, "fam0", "c", 2000, "v-c-0-2", {}},
      {row_key, "fam1", "c0", 0, "v-c0-0-0", {}},
      {row_key, "fam1", "c1", 2000, "v-c1-0-2", {}},
      {row_key, "fam1", "c1", 3000, "v-c1-0-3", {}},
  };
  auto actual = ReadRow(*table, row_key, bigtable::Filter::Latest(2));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, FamilyRegex) {
  auto table = CreateTable("family-regex-filter-table");
  std::string const row_key = "family-regex-row-key";
  std::vector<bigtable::Cell> created{
      {row_key, "fam0", "c2", 0, "bar", {}},
      {row_key, "fam0", "c", 0, "bar", {}},
      {row_key, "fam1", "c", 0, "bar", {}},
      {row_key, "fam2", "c", 0, "bar", {}},
      {row_key, "fam2", "c2", 0, "bar", {}},
      {row_key, "fam3", "c2", 0, "bar", {}},
  };
  CreateCells(*table, created);

  std::vector<bigtable::Cell> expected{
      {row_key, "fam0", "c2", 0, "bar", {}},
      {row_key, "fam0", "c", 0, "bar", {}},
      {row_key, "fam2", "c", 0, "bar", {}},
      {row_key, "fam2", "c2", 0, "bar", {}},
  };
  auto actual =
      ReadRow(*table, row_key, bigtable::Filter::FamilyRegex("fam[02]"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ColumnRegex) {
  auto table = CreateTable("column-regex-filter-table");
  std::string const row_key = "column-regex-row-key";
  std::vector<bigtable::Cell> created{
      {row_key, "fam0", "abc", 0, "bar", {}},
      {row_key, "fam1", "bcd", 0, "bar", {}},
      {row_key, "fam2", "abc", 0, "bar", {}},
      {row_key, "fam3", "def", 0, "bar", {}},
      {row_key, "fam0", "fgh", 0, "bar", {}},
      {row_key, "fam1", "hij", 0, "bar", {}},
  };
  CreateCells(*table, created);

  std::vector<bigtable::Cell> expected{
      {row_key, "fam0", "abc", 0, "bar", {}},
      {row_key, "fam2", "abc", 0, "bar", {}},
      {row_key, "fam0", "fgh", 0, "bar", {}},
      {row_key, "fam1", "hij", 0, "bar", {}},
  };
  auto actual =
      ReadRow(*table, row_key, bigtable::Filter::ColumnRegex("(abc|.*h.*)"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ColumnRange) {
  auto table = CreateTable("column-range-filter-table");
  std::string const row_key = "column-range-row-key";
  std::vector<bigtable::Cell> created{
      {row_key, "fam0", "a00", 0, "bar", {}},
      {row_key, "fam0", "b00", 0, "bar", {}},
      {row_key, "fam0", "b01", 0, "bar", {}},
      {row_key, "fam0", "b02", 0, "bar", {}},
      {row_key, "fam1", "a00", 0, "bar", {}},
      {row_key, "fam1", "b01", 0, "bar", {}},
      {row_key, "fam1", "b00", 0, "bar", {}},
  };
  CreateCells(*table, created);

  std::vector<bigtable::Cell> expected{
      {row_key, "fam0", "b00", 0, "bar", {}},
      {row_key, "fam0", "b01", 0, "bar", {}},
  };
  auto actual = ReadRow(*table, row_key,
                        bigtable::Filter::ColumnRange("fam0", "b00", "b02"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, TimestampRange) {
  auto table = CreateTable("timestamp-range-filter-table");
  std::string const row_key = "timestamp-range-row-key";
  std::vector<bigtable::Cell> created{
      {row_key, "fam0", "c0", 1000, "v1000", {}},
      {row_key, "fam1", "c1", 2000, "v2000", {}},
      {row_key, "fam2", "c2", 3000, "v3000", {}},
      {row_key, "fam0", "c3", 4000, "v4000", {}},
      {row_key, "fam1", "c4", 4000, "v5000", {}},
      {row_key, "fam2", "c5", 6000, "v6000", {}},
  };
  CreateCells(*table, created);

  std::vector<bigtable::Cell> expected{
      {row_key, "fam2", "c2", 1000, "v3000", {}},
      {row_key, "fam0", "c3", 1000, "v4000", {}},
      {row_key, "fam1", "c4", 1000, "v5000", {}},
  };
  using std::chrono::milliseconds;
  auto actual = ReadRow(*table, row_key, bigtable::Filter::TimestampRange(
                                             milliseconds(3), milliseconds(6)));
  CheckEqualUnordered(expected, actual);
}

// TODO(#152) - implement the following integration test.
TEST_F(FilterIntegrationTest, RowKeysRegex) {}

// TODO(#152) - implement the following integration test.
TEST_F(FilterIntegrationTest, ValueRegex) {}

// TODO(#152) - implement the following integration test.
TEST_F(FilterIntegrationTest, ValueRange) {}

TEST_F(FilterIntegrationTest, CellsRowLimit) {
  auto table = CreateTable("cells-row-limit-filter-table");
  std::string const prefix = "cell-row-limit-prefix";
  CreateComplexRows(*table, prefix);

  auto result = ReadRows(*table, bigtable::Filter::CellsRowLimit(3));

  std::map<std::string, int> actual;
  for (auto const& c : result) {
    auto ins = actual.emplace(static_cast<std::string>(c.row_key()), 0);
    ins.first->second++;
  }
  std::map<std::string, int> expected{{prefix + "/one-cell", 1},
                                      {prefix + "/two-cells", 2},
                                      {prefix + "/many", 3},
                                      {prefix + "/many-columns", 3},
                                      {prefix + "/complex", 3}};

  EXPECT_THAT(expected, ::testing::ContainerEq(actual));
}

TEST_F(FilterIntegrationTest, CellsRowOffset) {
  auto table = CreateTable("cells-row-offset-filter-table");
  std::string const prefix = "cell-row-offset-prefix";
  CreateComplexRows(*table, prefix);

  // Search in the range [row_key_prefix, row_key_prefix + "0"), we used '/' as
  // the separator and the successor of "/" is "0".
  auto result = ReadRows(*table, bigtable::Filter::CellsRowOffset(2));

  std::map<std::string, int> actual;
  for (auto const& c : result) {
    auto ins = actual.emplace(static_cast<std::string>(c.row_key()), 0);
    ins.first->second++;
  }
  std::map<std::string, int> expected{{prefix + "/many", 2},
                                      {prefix + "/many-columns", 2},
                                      {prefix + "/complex", 78}};

  EXPECT_THAT(expected, ::testing::ContainerEq(actual));
}

TEST_F(FilterIntegrationTest, RowSample) {
  auto table = CreateTable("row-sample-filter-table");
  std::string const prefix = "row-sample-prefix";
  // TODO(#151) - remove workarounds for emulator bug(s).
  if (UsingCloudBigtableEmulator()) {
    return;
  }

  constexpr int row_count = 20000;
  bigtable::BulkMutation bulk;
  for (int row = 0; row != row_count; ++row) {
    std::string row_key = prefix + "/" + std::to_string(row);
    bulk.emplace_back(bigtable::SingleRowMutation(
        row_key, {bigtable::SetCell("fam0", "col", 4000, "foo")}));
  }
  table->BulkApply(std::move(bulk));

  // We want to check that the sampling rate was "more or less" the prescribed
  // value.  We use 5% as the allowed error, this is arbitrary.  If we wanted to
  // get serious about testing the sampling rate, we would do some statistics.
  // We do not really need to, because we are testing the library, not the
  // server. But for what it's worth, the outline would be:
  //
  //   - Model sampling as a binomial process.
  //   - Perform power analysis to decide the size of the sample.
  //   - Perform hypothesis testing: is the actual sampling rate != that the
  //     prescribed rate (and sufficiently different, i.e., the effect is large
  //     enough).
  //
  // For what is worth, the sample size is large enough to detect effects of 2%
  // at the conventional significance and power levels.  In R:
  //
  // ```R
  // require(pwr)
  // pwr.p.test(h = ES.h(p1 = 0.63, p2 = 0.65), sig.level = 0.05,
  //            power=0.80, alternative="two.sided")
  // ```
  //
  // h = 0.04167045
  // n = 4520.123
  // sig.level = 0.05
  // power = 0.8
  // alternative = two.sided
  //
  constexpr double kSampleRate = 0.75;
  constexpr double kAllowedError = 0.05;
  const std::size_t kMinCount = static_cast<std::size_t>(
      std::floor((kSampleRate - kAllowedError) * row_count));
  const std::size_t kMaxCount = static_cast<std::size_t>(
      std::ceil((kSampleRate + kAllowedError) * row_count));

  // Search in the range [row_key_prefix, row_key_prefix + "0"), we used '/' as
  // the separator and the successor of "/" is "0".
  auto result = ReadRows(*table, bigtable::Filter::RowSample(kSampleRate));

  EXPECT_LE(kMinCount, result.size());
  EXPECT_GE(kMaxCount, result.size());
}

// TODO(#152) - implement the following integration test.
TEST_F(FilterIntegrationTest, StripValueTransformer) {}

// TODO(#152) - implement the following integration test.
TEST_F(FilterIntegrationTest, Condition) {}

// TODO(#152) - implement the following integration test.
TEST_F(FilterIntegrationTest, Chain) {}

// TODO(#152) - implement the following integration test.
TEST_F(FilterIntegrationTest, Interleave) {}

namespace {
std::string FilterTestEnvironment::project_id_;
std::string FilterTestEnvironment::instance_id_;

void FilterIntegrationTest::SetUp() {
  admin_client_ = bigtable::CreateAdminClient(
      FilterTestEnvironment::project_id(), bigtable::ClientOptions());
  table_admin_ = absl::make_unique<bigtable::TableAdmin>(
      admin_client_, FilterTestEnvironment::instance_id());
  data_client_ = bigtable::CreateDefaultClient(
      FilterTestEnvironment::project_id(), FilterTestEnvironment::instance_id(),
      bigtable::ClientOptions());
}

std::unique_ptr<bigtable::Table> FilterIntegrationTest::CreateTable(
    std::string const& table_id) {
  table_admin_->CreateTable(
      table_id,
      bigtable::TableConfig({{fam0, bigtable::GcRule::MaxNumVersions(10)},
                             {fam1, bigtable::GcRule::MaxNumVersions(10)},
                             {fam2, bigtable::GcRule::MaxNumVersions(10)},
                             {fam3, bigtable::GcRule::MaxNumVersions(10)}},
                            {}));
  return absl::make_unique<bigtable::Table>(data_client_, table_id);
}

std::vector<bigtable::Cell> FilterIntegrationTest::ReadRows(
    bigtable::Table& table, btproto::ReadRowsRequest request) {
  std::vector<bigtable::Cell> result;
  grpc::ClientContext client_context;
  auto stream = data_client_->Stub()->ReadRows(&client_context, request);
  btproto::ReadRowsResponse response;

  std::string current_row_key;
  std::string current_family;
  std::string current_column;
  std::string current_value;
  std::vector<std::string> current_labels;
  while (stream->Read(&response)) {
    for (btproto::ReadRowsResponse::CellChunk const& chunk :
         response.chunks()) {
      if (not chunk.row_key().empty()) {
        current_row_key = chunk.row_key();
      }
      if (chunk.has_family_name()) {
        current_family = chunk.family_name().value();
      }
      if (chunk.has_qualifier()) {
        current_column = chunk.qualifier().value();
      }
      // Most of the time `chunk.labels()` is empty, but the following copy is
      // fast in that case.
      std::copy(chunk.labels().begin(), chunk.labels().end(),
                std::back_inserter(current_labels));
      if (chunk.value_size() > 0) {
        current_value.reserve(chunk.value_size());
      }
      current_value.append(chunk.value());
      if (chunk.value_size() == 0 or chunk.commit_row()) {
        // This was the last chunk.
        result.emplace_back(current_row_key, current_family, current_column,
                            chunk.timestamp_micros(), std::move(current_value),
                            std::move(current_labels));
      }
    }
  }
  auto status = stream->Finish();
  if (not status.ok()) {
    std::ostringstream os;
    os << "gRPC error in ReadRow() - " << status.error_message() << " ["
       << status.error_code() << "] details=" << status.error_details();
    throw std::runtime_error(os.str());
  }
  return result;
}

std::vector<bigtable::Cell> FilterIntegrationTest::ReadRow(
    bigtable::Table& table, std::string key, bigtable::Filter filter) {
  btproto::ReadRowsRequest request;
  request.set_table_name(table.table_name());
  request.set_rows_limit(1);
  auto& row = *request.mutable_rows();
  *row.add_row_keys() = std::move(key);
  *request.mutable_filter() = filter.as_proto_move();

  return ReadRows(table, std::move(request));
}

std::vector<bigtable::Cell> FilterIntegrationTest::ReadRows(
    bigtable::Table& table, bigtable::Filter filter) {
  btproto::ReadRowsRequest request;
  request.set_table_name(table.table_name());
  *request.mutable_filter() = filter.as_proto_move();

  return ReadRows(table, std::move(request));
}

/// A helper function to create a list of cells.
void FilterIntegrationTest::CreateCells(
    bigtable::Table& table, std::vector<bigtable::Cell> const& cells) {
  std::map<std::string, bigtable::SingleRowMutation> mutations;
  for (auto const& cell : cells) {
    std::string key = static_cast<std::string>(cell.row_key());
    auto inserted = mutations.emplace(key, bigtable::SingleRowMutation(key));
    inserted.first->second.emplace_back(bigtable::SetCell(
        static_cast<std::string>(cell.family_name()),
        static_cast<std::string>(cell.column_qualifier()), cell.timestamp(),
        static_cast<std::string>(cell.value())));
  }
  bigtable::BulkMutation bulk;
  for (auto& kv : mutations) {
    bulk.emplace_back(std::move(kv.second));
  }
  table.BulkApply(std::move(bulk));
}

void FilterIntegrationTest::CreateComplexRows(bigtable::Table& table,
                                              std::string const& prefix) {
  namespace bt = bigtable;
  bt::BulkMutation mutation;
  // Prepare a set of rows, with different numbers of cells, columns, and
  // column families.
  mutation.emplace_back(bt::SingleRowMutation(
      prefix + "/one-cell", {bt::SetCell("fam0", "c", 3000, "foo")}));
  mutation.emplace_back(
      bt::SingleRowMutation(prefix + "/two-cells",
                            {bt::SetCell("fam0", "c", 3000, "foo"),
                             bt::SetCell("fam0", "c2", 3000, "foo")}));
  mutation.emplace_back(
      bt::SingleRowMutation(prefix + "/many",
                            {bt::SetCell("fam0", "c", 0, "foo"),
                             bt::SetCell("fam0", "c", 1000, "foo"),
                             bt::SetCell("fam0", "c", 2000, "foo"),
                             bt::SetCell("fam0", "c", 3000, "foo")}));
  mutation.emplace_back(
      bt::SingleRowMutation(prefix + "/many-columns",
                            {bt::SetCell("fam0", "c0", 3000, "foo"),
                             bt::SetCell("fam0", "c1", 3000, "foo"),
                             bt::SetCell("fam0", "c2", 3000, "foo"),
                             bt::SetCell("fam0", "c3", 3000, "foo")}));
  // This one is complicated: create a mutation with several families and
  // columns.
  bt::SingleRowMutation complex(prefix + "/complex");
  for (int i = 0; i != 4; ++i) {
    for (int j = 0; j != 10; ++j) {
      complex.emplace_back(bt::SetCell("fam" + std::to_string(i),
                                       "col" + std::to_string(j), 3000, "foo"));
      complex.emplace_back(bt::SetCell("fam" + std::to_string(i),
                                       "col" + std::to_string(j), 6000, "bar"));
    }
  }
  mutation.emplace_back(std::move(complex));
  table.BulkApply(std::move(mutation));
}

int CellCompare(bigtable::Cell const& lhs, bigtable::Cell const& rhs) {
  auto compare_row_key = lhs.row_key().compare(lhs.row_key());
  if (compare_row_key != 0) {
    return compare_row_key;
  }
  auto compare_family_name = lhs.family_name().compare(rhs.family_name());
  if (compare_family_name != 0) {
    return compare_family_name;
  }
  auto compare_column_qualifier =
      lhs.column_qualifier().compare(rhs.column_qualifier());
  if (compare_column_qualifier != 0) {
    return compare_column_qualifier;
  }
  if (lhs.timestamp() < lhs.timestamp()) {
    return -1;
  }
  if (lhs.timestamp() > lhs.timestamp()) {
    return 1;
  }
  return lhs.value().compare(rhs.value());
}

void CheckEqualUnordered(std::vector<bigtable::Cell> expected,
                         std::vector<bigtable::Cell> actual) {
  std::sort(expected.begin(), expected.end());
  std::sort(actual.begin(), actual.end());
  EXPECT_THAT(expected, ::testing::ContainerEq(actual));
}

bool UsingCloudBigtableEmulator() {
  return std::getenv("BIGTABLE_EMULATOR_HOST") != nullptr;
}

}  // anonymous namespace
