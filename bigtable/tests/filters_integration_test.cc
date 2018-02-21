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
#include <sstream>

#include <gmock/gmock.h>

#include "bigtable/admin/admin_client.h"
#include "bigtable/admin/table_admin.h"
#include "bigtable/client/cell.h"
#include "bigtable/client/data_client.h"
#include "bigtable/client/internal/throw_delegate.h"
#include "bigtable/client/table.h"
#include "bigtable/client/testing/table_integration_test.h"

namespace btproto = ::google::bigtable::v2;
namespace admin_proto = ::google::bigtable::admin::v2;

namespace bigtable {
namespace testing {

class FilterIntegrationTest : public TableIntegrationTest {
 protected:
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

  std::string fam0 = "fam0";
  std::string fam1 = "fam1";
  std::string fam2 = "fam2";
  std::string fam3 = "fam3";

  bigtable::TableConfig table_config =
      bigtable::TableConfig({{fam0, bigtable::GcRule::MaxNumVersions(10)},
                             {fam1, bigtable::GcRule::MaxNumVersions(10)},
                             {fam2, bigtable::GcRule::MaxNumVersions(10)},
                             {fam3, bigtable::GcRule::MaxNumVersions(10)}},
                            {});
};

}  // namespace testing
}  // namespace bigtable

namespace {
/// Return true if connected to the Cloud Bigtable Emulator.
bool UsingCloudBigtableEmulator();

}  // anonymous namespace

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
    std::cerr << "Expected empty instance at the beginning of integration "
              << "test";
    return 1;
  }

  (void)::testing::AddGlobalTestEnvironment(
      new ::bigtable::testing::TableTestEnvironment(project_id, instance_id));

  return RUN_ALL_TESTS();
}

namespace bigtable {
namespace testing {

TEST_F(FilterIntegrationTest, PassAll) {
  auto table = CreateTable("pass-all-filter-table", table_config);
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

  auto actual = ReadRows(*table, bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, BlockAll) {
  // TODO(#151) - remove workarounds for emulator bug(s).
  if (UsingCloudBigtableEmulator()) {
    return;
  }
  auto table = CreateTable("block-all-filter-table", table_config);
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
  auto actual = ReadRows(*table, bigtable::Filter::BlockAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, Latest) {
  auto table = CreateTable("latest-filter-table", table_config);
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
  auto actual = ReadRows(*table, bigtable::Filter::Latest(2));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, FamilyRegex) {
  auto table = CreateTable("family-regex-filter-table", table_config);
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
  auto actual = ReadRows(*table, bigtable::Filter::FamilyRegex("fam[02]"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ColumnRegex) {
  auto table = CreateTable("column-regex-filter-table", table_config);
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
  auto actual = ReadRows(*table, bigtable::Filter::ColumnRegex("(abc|.*h.*)"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ColumnRange) {
  auto table = CreateTable("column-range-filter-table", table_config);
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
  auto actual =
      ReadRows(*table, bigtable::Filter::ColumnRange("fam0", "b00", "b02"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, TimestampRange) {
  auto table = CreateTable("timestamp-range-filter-table", table_config);
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
      {row_key, "fam2", "c2", 3000, "v3000", {}},
      {row_key, "fam0", "c3", 4000, "v4000", {}},
      {row_key, "fam1", "c4", 4000, "v5000", {}},
  };
  using std::chrono::milliseconds;
  auto actual = ReadRows(*table, bigtable::Filter::TimestampRange(
                                     milliseconds(3), milliseconds(6)));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, RowKeysRegex) {
  auto table = CreateTable("row-key-regex-filter-table", table_config);
  std::string const row_key = "row-key-regex-row-key";
  std::vector<bigtable::Cell> created{
      {row_key + "/abc0", "fam0", "c0", 1000, "v1000", {}},
      {row_key + "/bcd0", "fam1", "c1", 2000, "v2000", {}},
      {row_key + "/abc1", "fam2", "c2", 3000, "v3000", {}},
      {row_key + "/fgh0", "fam0", "c3", 4000, "v4000", {}},
      {row_key + "/hij0", "fam1", "c4", 4000, "v5000", {}},
      {row_key + "/hij1", "fam2", "c5", 6000, "v6000", {}},
  };
  CreateCells(*table, created);

  std::vector<bigtable::Cell> expected{
      {row_key + "/bcd0", "fam1", "c1", 2000, "v2000", {}},
  };
  auto actual =
      ReadRows(*table, bigtable::Filter::RowKeysRegex(row_key + "/bc.*"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ValueRegex) {
  auto table = CreateTable("value-regex-filter-table", table_config);
  std::string const prefix = "value-regex-prefix";
  std::vector<bigtable::Cell> created{
      {prefix + "/abc0", "fam0", "c0", 1000, "v1000", {}},
      {prefix + "/bcd0", "fam1", "c1", 2000, "v2000", {}},
      {prefix + "/abc1", "fam2", "c2", 3000, "v3000", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "v4000", {}},
      {prefix + "/hij0", "fam1", "c4", 4000, "v5000", {}},
      {prefix + "/hij1", "fam2", "c5", 6000, "v6000", {}},
  };
  CreateCells(*table, created);
  std::vector<bigtable::Cell> expected{
      {prefix + "/abc1", "fam2", "c2", 3000, "v3000", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "v4000", {}},
  };
  auto actual = ReadRows(*table, bigtable::Filter::ValueRegex("v[34][0-9].*"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ValueRange) {
  auto table = CreateTable("value-range-filter-table", table_config);
  std::string const prefix = "value-range-prefix";
  std::vector<bigtable::Cell> created{
      {prefix + "/abc0", "fam0", "c0", 1000, "v1000", {}},
      {prefix + "/bcd0", "fam1", "c1", 2000, "v2000", {}},
      {prefix + "/abc1", "fam2", "c2", 3000, "v3000", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "v4000", {}},
      {prefix + "/hij0", "fam1", "c4", 4000, "v5000", {}},
      {prefix + "/hij1", "fam2", "c5", 6000, "v6000", {}},
  };
  CreateCells(*table, created);
  std::vector<bigtable::Cell> expected{
      {prefix + "/bcd0", "fam1", "c1", 2000, "v2000", {}},
      {prefix + "/abc1", "fam2", "c2", 3000, "v3000", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "v4000", {}},
      {prefix + "/hij0", "fam1", "c4", 4000, "v5000", {}},
  };
  auto actual =
      ReadRows(*table, bigtable::Filter::ValueRange("v2000", "v6000"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, CellsRowLimit) {
  auto table = CreateTable("cells-row-limit-filter-table", table_config);
  std::string const prefix = "cell-row-limit-prefix";
  CreateComplexRows(*table, prefix);

  auto result = ReadRows(*table, bigtable::Filter::CellsRowLimit(3));

  std::map<std::string, int> actual;
  for (auto const& c : result) {
    auto ins = actual.emplace(c.row_key(), 0);
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
  auto table = CreateTable("cells-row-offset-filter-table", table_config);
  std::string const prefix = "cell-row-offset-prefix";
  CreateComplexRows(*table, prefix);

  // Search in the range [row_key_prefix, row_key_prefix + "0"), we used '/' as
  // the separator and the successor of "/" is "0".
  auto result = ReadRows(*table, bigtable::Filter::CellsRowOffset(2));

  std::map<std::string, int> actual;
  for (auto const& c : result) {
    auto ins = actual.emplace(c.row_key(), 0);
    ins.first->second++;
  }
  std::map<std::string, int> expected{{prefix + "/many", 2},
                                      {prefix + "/many-columns", 2},
                                      {prefix + "/complex", 78}};

  EXPECT_THAT(expected, ::testing::ContainerEq(actual));
}

TEST_F(FilterIntegrationTest, RowSample) {
  auto table = CreateTable("row-sample-filter-table", table_config);
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

TEST_F(FilterIntegrationTest, StripValueTransformer) {
  auto table =
      CreateTable("strip-value-transformer-filter-table", table_config);
  std::string const prefix = "strip-value-transformer-prefix";
  std::vector<bigtable::Cell> created{
      {prefix + "/abc0", "fam0", "c0", 1000, "v1000", {}},
      {prefix + "/bcd0", "fam1", "c1", 2000, "v2000", {}},
      {prefix + "/abc1", "fam2", "c2", 3000, "v3000", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "v4000", {}},
      {prefix + "/hij0", "fam1", "c4", 4000, "v5000", {}},
      {prefix + "/hij1", "fam2", "c5", 6000, "v6000", {}},
  };
  CreateCells(*table, created);
  std::vector<bigtable::Cell> expected{
      {prefix + "/abc0", "fam0", "c0", 1000, "", {}},
      {prefix + "/bcd0", "fam1", "c1", 2000, "", {}},
      {prefix + "/abc1", "fam2", "c2", 3000, "", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "", {}},
      {prefix + "/hij0", "fam1", "c4", 4000, "", {}},
      {prefix + "/hij1", "fam2", "c5", 6000, "", {}},
  };
  auto actual = ReadRows(*table, bigtable::Filter::StripValueTransformer());
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ApplyLabelTransformer) {
  // TODO(#151) - remove workarounds for emulator bug(s).
  if (UsingCloudBigtableEmulator()) {
    return;
  }
  auto table =
      CreateTable("apply-label-transformer-filter-table", table_config);
  std::string const prefix = "apply-label-transformer-prefix";
  std::vector<bigtable::Cell> created{
      {prefix + "/abc0", "fam0", "c0", 1000, "v1000", {}},
      {prefix + "/bcd0", "fam1", "c1", 2000, "v2000", {}},
      {prefix + "/abc1", "fam2", "c2", 3000, "v3000", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "v4000", {}},
      {prefix + "/hij0", "fam1", "c4", 4000, "v5000", {}},
      {prefix + "/hij1", "fam2", "c5", 6000, "v6000", {}},
  };
  CreateCells(*table, created);
  std::vector<bigtable::Cell> expected{
      {prefix + "/abc0", "fam0", "c0", 1000, "v1000", {"foo"}},
      {prefix + "/bcd0", "fam1", "c1", 2000, "v2000", {"foo"}},
      {prefix + "/abc1", "fam2", "c2", 3000, "v3000", {"foo"}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "v4000", {"foo"}},
      {prefix + "/hij0", "fam1", "c4", 4000, "v5000", {"foo"}},
      {prefix + "/hij1", "fam2", "c5", 6000, "v6000", {"foo"}},
  };
  auto actual =
      ReadRows(*table, bigtable::Filter::ApplyLabelTransformer("foo"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, Condition) {
  auto table = CreateTable("condition-filter-table", table_config);
  std::string const prefix = "condition-prefix";
  std::vector<bigtable::Cell> created{
      {prefix + "/abc0", "fam0", "c0", 1000, "v1000", {}},
      {prefix + "/bcd0", "fam1", "c1", 2000, "v2000", {}},
      {prefix + "/abc1", "fam2", "c2", 3000, "v3000", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "v4000", {}},
      {prefix + "/hij0", "fam1", "c4", 4000, "v5000", {}},
      {prefix + "/hij1", "fam2", "c5", 6000, "v6000", {}},
  };
  CreateCells(*table, created);
  std::vector<bigtable::Cell> expected{
      {prefix + "/abc0", "fam0", "c0", 1000, "v1000", {}},
      {prefix + "/bcd0", "fam1", "c1", 2000, "", {}},
      {prefix + "/abc1", "fam2", "c2", 3000, "", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "", {}},
      {prefix + "/hij0", "fam1", "c4", 4000, "v5000", {}},
  };
  using F = bigtable::Filter;
  auto actual =
      ReadRows(*table, F::Condition(F::ValueRangeClosed("v2000", "v4000"),
                                    F::StripValueTransformer(),
                                    F::FamilyRegex("fam[01]")));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, Chain) {
  auto table = CreateTable("chain-filter-table", table_config);
  std::string const prefix = "chain-prefix";
  std::vector<bigtable::Cell> created{
      {prefix + "/abc0", "fam0", "c0", 1000, "v1000", {}},
      {prefix + "/bcd0", "fam1", "c1", 2000, "v2000", {}},
      {prefix + "/abc1", "fam2", "c2", 3000, "v3000", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "v4000", {}},
      {prefix + "/hij0", "fam1", "c4", 4000, "v5000", {}},
      {prefix + "/hij1", "fam2", "c5", 6000, "v6000", {}},
  };
  CreateCells(*table, created);
  std::vector<bigtable::Cell> expected{
      {prefix + "/fgh0", "fam0", "c3", 4000, "", {}},
  };
  using F = bigtable::Filter;
  auto actual =
      ReadRows(*table, F::Chain(F::ValueRangeClosed("v2000", "v5000"),
                                F::StripValueTransformer(),
                                F::ColumnRangeClosed("fam0", "c2", "c3")));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, Interleave) {
  auto table = CreateTable("interleave-filter-table", table_config);
  std::string const prefix = "interleave-prefix";
  std::vector<bigtable::Cell> created{
      {prefix + "/abc0", "fam0", "c0", 1000, "v1000", {}},
      {prefix + "/bcd0", "fam1", "c1", 2000, "v2000", {}},
      {prefix + "/abc1", "fam2", "c2", 3000, "v3000", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "v4000", {}},
      {prefix + "/hij0", "fam1", "c4", 4000, "v5000", {}},
      {prefix + "/hij1", "fam2", "c5", 6000, "v6000", {}},
  };
  CreateCells(*table, created);
  std::vector<bigtable::Cell> expected{
      {prefix + "/bcd0", "fam1", "c1", 2000, "", {}},
      {prefix + "/abc1", "fam2", "c2", 3000, "", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "", {}},
      {prefix + "/fgh0", "fam0", "c3", 4000, "v4000", {}},
      {prefix + "/hij0", "fam1", "c4", 4000, "", {}},
  };
  using F = bigtable::Filter;
  auto actual = ReadRows(
      *table, F::Interleave(F::Chain(F::ValueRangeClosed("v2000", "v5000"),
                                     F::StripValueTransformer()),
                            F::ColumnRangeClosed("fam0", "c2", "c3")));
  CheckEqualUnordered(expected, actual);
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

}  // namespace testing
}  // namespace bigtable

namespace {
bool UsingCloudBigtableEmulator() {
  return std::getenv("BIGTABLE_EMULATOR_HOST") != nullptr;
}
}  // namespace
