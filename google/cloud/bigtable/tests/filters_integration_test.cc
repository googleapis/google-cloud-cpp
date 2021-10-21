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
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using FilterIntegrationTest =
    ::google::cloud::bigtable::testing::TableIntegrationTest;

/**
 * Create some complex rows in @p table.
 *
 * Create the following rows in @p table, the magic values for the column
 * families are defined above.
 *
 * | Row Key                 | Family  | Column | Contents     |
 * | :---------------------- | :------ | :----- | :----------- |
 * | "{prefix}/one-cell"     | family1 | c      | cell @ 3000  |
 * | "{prefix}/two-cells"    | family1 | c      | cell @ 3000  |
 * | "{prefix}/two-cells"    | family1 | c2     | cell @ 3000  |
 * | "{prefix}/many"         | family1 | c      | cell @ 0     |
 * | "{prefix}/many"         | family1 | c      | cell @ 1000  |
 * | "{prefix}/many"         | family1 | c      | cell @ 2000  |
 * | "{prefix}/many"         | family1 | c      | cell @ 3000  |
 * | "{prefix}/many-columns" | family1 | c0     | cell @ 3000  |
 * | "{prefix}/many-columns" | family1 | c1     | cell @ 3000  |
 * | "{prefix}/many-columns" | family1 | c2     | cell @ 3000  |
 * | "{prefix}/many-columns" | family1 | c3     | cell @ 3000  |
 * | "{prefix}/complex"      | family1 | col0   | cell @ 3000, 6000 |
 * | "{prefix}/complex"      | family1 | col1   | cell @ 3000, 6000 |
 * | "{prefix}/complex"      | family1 | ...    | cell @ 3000, 6000 |
 * | "{prefix}/complex"      | family1 | col9   | cell @ 3000, 6000 |
 * | "{prefix}/complex"      | family2 | col0   | cell @ 3000, 6000 |
 * | "{prefix}/complex"      | family2 | col1   | cell @ 3000, 6000 |
 * | "{prefix}/complex"      | family2 | ...    | cell @ 3000, 6000 |
 * | "{prefix}/complex"      | family2 | col9   | cell @ 3000, 6000 |
 *
 */
void CreateComplexRows(Table& table, std::string const& prefix) {
  namespace bt = bigtable;
  using ::google::cloud::testing_util::chrono_literals::operator"" _ms;

  bt::BulkMutation mutation;
  // Prepare a set of rows, with different numbers of cells, columns, and
  // column families.
  mutation.emplace_back(bt::SingleRowMutation(
      prefix + "/one-cell", {bt::SetCell("family1", "c", 3_ms, "foo")}));
  mutation.emplace_back(bt::SingleRowMutation(
      prefix + "/two-cells", {bt::SetCell("family1", "c", 3_ms, "foo"),
                              bt::SetCell("family1", "c2", 3_ms, "foo")}));
  mutation.emplace_back(bt::SingleRowMutation(
      prefix + "/many", {bt::SetCell("family1", "c", 0_ms, "foo"),
                         bt::SetCell("family1", "c", 1_ms, "foo"),
                         bt::SetCell("family1", "c", 2_ms, "foo"),
                         bt::SetCell("family1", "c", 3_ms, "foo")}));
  mutation.emplace_back(bt::SingleRowMutation(
      prefix + "/many-columns", {bt::SetCell("family1", "c0", 3_ms, "foo"),
                                 bt::SetCell("family1", "c1", 3_ms, "foo"),
                                 bt::SetCell("family1", "c2", 3_ms, "foo"),
                                 bt::SetCell("family1", "c3", 3_ms, "foo")}));
  // This one is complicated: create a mutation with several families and
  // columns.
  bt::SingleRowMutation complex(prefix + "/complex");
  for (int i = 0; i != 4; ++i) {
    for (int j = 0; j != 10; ++j) {
      complex.emplace_back(bt::SetCell("family" + std::to_string(i + 1),
                                       "col" + std::to_string(j), 3_ms, "foo"));
      complex.emplace_back(bt::SetCell("family" + std::to_string(i + 1),
                                       "col" + std::to_string(j), 6_ms, "bar"));
    }
  }
  mutation.emplace_back(std::move(complex));
  auto failures = table.BulkApply(std::move(mutation));
  ASSERT_TRUE(failures.empty());
};

TEST_F(FilterIntegrationTest, PassAll) {
  auto table = GetTable();
  std::string const row_key = "pass-all-row-key";
  std::vector<Cell> expected{
      {row_key, "family1", "c", 0, "v-c-0-0"},
      {row_key, "family1", "c", 1000, "v-c-0-1"},
      {row_key, "family1", "c", 2000, "v-c-0-2"},
      {row_key, "family2", "c0", 0, "v-c0-0-0"},
      {row_key, "family2", "c1", 1000, "v-c1-0-1"},
      {row_key, "family2", "c1", 2000, "v-c1-0-2"},
  };
  CreateCells(table, expected);

  auto actual = ReadRows(table, Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, BlockAll) {
  auto table = GetTable();
  std::string const row_key = "block-all-row-key";
  std::vector<Cell> created{
      {row_key, "family1", "c", 0, "v-c-0-0"},
      {row_key, "family1", "c", 1000, "v-c-0-1"},
      {row_key, "family1", "c", 2000, "v-c-0-2"},
      {row_key, "family2", "c0", 0, "v-c0-0-0"},
      {row_key, "family2", "c1", 1000, "v-c1-0-1"},
      {row_key, "family2", "c1", 2000, "v-c1-0-2"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{};

  auto actual = ReadRows(table, Filter::BlockAllFilter());
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, Latest) {
  auto table = GetTable();
  std::string const row_key = "latest-row-key";
  std::vector<Cell> created{
      {row_key, "family1", "c", 0, "v-c-0-0"},
      {row_key, "family1", "c", 1000, "v-c-0-1"},
      {row_key, "family1", "c", 2000, "v-c-0-2"},
      {row_key, "family2", "c0", 0, "v-c0-0-0"},
      {row_key, "family2", "c1", 1000, "v-c1-0-1"},
      {row_key, "family2", "c1", 2000, "v-c1-0-2"},
      {row_key, "family2", "c1", 3000, "v-c1-0-3"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {row_key, "family1", "c", 1000, "v-c-0-1"},
      {row_key, "family1", "c", 2000, "v-c-0-2"},
      {row_key, "family2", "c0", 0, "v-c0-0-0"},
      {row_key, "family2", "c1", 2000, "v-c1-0-2"},
      {row_key, "family2", "c1", 3000, "v-c1-0-3"},
  };

  auto actual = ReadRows(table, Filter::Latest(2));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, FamilyRegex) {
  auto table = GetTable();
  std::string const row_key = "family-regex-row-key";
  std::vector<Cell> created{
      {row_key, "family1", "c2", 0, "bar"},
      {row_key, "family1", "c", 0, "bar"},
      {row_key, "family2", "c", 0, "bar"},
      {row_key, "family3", "c", 0, "bar"},
      {row_key, "family3", "c2", 0, "bar"},
      {row_key, "family4", "c2", 0, "bar"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {row_key, "family1", "c2", 0, "bar"},
      {row_key, "family1", "c", 0, "bar"},
      {row_key, "family3", "c", 0, "bar"},
      {row_key, "family3", "c2", 0, "bar"},
  };

  auto actual = ReadRows(table, Filter::FamilyRegex("family[13]"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ColumnRegex) {
  auto table = GetTable();
  std::string const row_key = "column-regex-row-key";
  std::vector<Cell> created{
      {row_key, "family1", "abc", 0, "bar"},
      {row_key, "family2", "bcd", 0, "bar"},
      {row_key, "family3", "abc", 0, "bar"},
      {row_key, "family4", "def", 0, "bar"},
      {row_key, "family1", "fgh", 0, "bar"},
      {row_key, "family2", "hij", 0, "bar"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {row_key, "family1", "abc", 0, "bar"},
      {row_key, "family3", "abc", 0, "bar"},
      {row_key, "family1", "fgh", 0, "bar"},
      {row_key, "family2", "hij", 0, "bar"},
  };

  auto actual = ReadRows(table, Filter::ColumnRegex("(abc|.*h.*)"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ColumnRange) {
  auto table = GetTable();
  std::string const row_key = "column-range-row-key";
  std::vector<Cell> created{
      {row_key, "family1", "a00", 0, "bar"},
      {row_key, "family1", "b00", 0, "bar"},
      {row_key, "family1", "b01", 0, "bar"},
      {row_key, "family1", "b02", 0, "bar"},
      {row_key, "family2", "a00", 0, "bar"},
      {row_key, "family2", "b01", 0, "bar"},
      {row_key, "family2", "b00", 0, "bar"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {row_key, "family1", "b00", 0, "bar"},
      {row_key, "family1", "b01", 0, "bar"},
  };

  auto actual = ReadRows(table, Filter::ColumnRange("family1", "b00", "b02"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, TimestampRange) {
  auto table = GetTable();
  std::string const row_key = "timestamp-range-row-key";
  std::vector<Cell> created{
      {row_key, "family1", "c0", 1000, "v1000"},
      {row_key, "family2", "c1", 2000, "v2000"},
      {row_key, "family3", "c2", 3000, "v3000"},
      {row_key, "family1", "c3", 4000, "v4000"},
      {row_key, "family2", "c4", 4000, "v5000"},
      {row_key, "family3", "c5", 6000, "v6000"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {row_key, "family3", "c2", 3000, "v3000"},
      {row_key, "family1", "c3", 4000, "v4000"},
      {row_key, "family2", "c4", 4000, "v5000"},
  };

  auto actual =
      ReadRows(table, Filter::TimestampRange(std::chrono::milliseconds(3),
                                             std::chrono::milliseconds(6)));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, RowKeysRegex) {
  auto table = GetTable();
  std::string const row_key = "row-key-regex-row-key";
  std::vector<Cell> created{
      {row_key + "/abc0", "family1", "c0", 1000, "v1000"},
      {row_key + "/bcd0", "family2", "c1", 2000, "v2000"},
      {row_key + "/abc1", "family3", "c2", 3000, "v3000"},
      {row_key + "/fgh0", "family1", "c3", 4000, "v4000"},
      {row_key + "/hij0", "family2", "c4", 4000, "v5000"},
      {row_key + "/hij1", "family3", "c5", 6000, "v6000"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {row_key + "/bcd0", "family2", "c1", 2000, "v2000"},
  };

  auto actual = ReadRows(table, Filter::RowKeysRegex(row_key + "/bc.*"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ValueRegex) {
  auto table = GetTable();
  std::string const prefix = "value-regex-prefix";
  std::vector<Cell> created{
      {prefix + "/abc0", "family1", "c0", 1000, "v1000"},
      {prefix + "/bcd0", "family2", "c1", 2000, "v2000"},
      {prefix + "/abc1", "family3", "c2", 3000, "v3000"},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000"},
      {prefix + "/hij1", "family3", "c5", 6000, "v6000"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {prefix + "/abc1", "family3", "c2", 3000, "v3000"},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
  };

  auto actual = ReadRows(table, Filter::ValueRegex("v[34][0-9].*"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ValueRange) {
  auto table = GetTable();
  std::string const prefix = "value-range-prefix";
  std::vector<Cell> created{
      {prefix + "/abc0", "family1", "c0", 1000, "v1000"},
      {prefix + "/bcd0", "family2", "c1", 2000, "v2000"},
      {prefix + "/abc1", "family3", "c2", 3000, "v3000"},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000"},
      {prefix + "/hij1", "family3", "c5", 6000, "v6000"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {prefix + "/bcd0", "family2", "c1", 2000, "v2000"},
      {prefix + "/abc1", "family3", "c2", 3000, "v3000"},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000"},
  };

  auto actual = ReadRows(table, Filter::ValueRange("v2000", "v6000"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, CellsRowLimit) {
  auto table = GetTable();
  std::string const prefix = "cell-row-limit-prefix";
  ASSERT_NO_FATAL_FAILURE(CreateComplexRows(table, prefix));

  auto result = ReadRows(table, Filter::CellsRowLimit(3));

  std::map<RowKeyType, int> actual;
  for (auto const& cell : result) {
    auto inserted = actual.emplace(cell.row_key(), 0);
    inserted.first->second++;
  }
  std::map<RowKeyType, int> expected{{RowKeyType(prefix + "/one-cell"), 1},
                                     {RowKeyType(prefix + "/two-cells"), 2},
                                     {RowKeyType(prefix + "/many"), 3},
                                     {RowKeyType(prefix + "/many-columns"), 3},
                                     {RowKeyType(prefix + "/complex"), 3}};

  EXPECT_THAT(expected, ::testing::ContainerEq(actual));
}

TEST_F(FilterIntegrationTest, CellsRowOffset) {
  auto table = GetTable();
  std::string const prefix = "cell-row-offset-prefix";
  ASSERT_NO_FATAL_FAILURE(CreateComplexRows(table, prefix));

  // Search in the range [row_key_prefix, row_key_prefix + "0"), we used '/' as
  // the separator and the successor of "/" is "0".
  auto result = ReadRows(table, Filter::CellsRowOffset(2));

  std::map<RowKeyType, int> actual;
  for (auto const& cell : result) {
    auto inserted = actual.emplace(cell.row_key(), 0);
    inserted.first->second++;
  }
  std::map<RowKeyType, int> expected{{RowKeyType(prefix + "/many"), 2},
                                     {RowKeyType(prefix + "/many-columns"), 2},
                                     {RowKeyType(prefix + "/complex"), 78}};

  EXPECT_THAT(expected, ::testing::ContainerEq(actual));
}

TEST_F(FilterIntegrationTest, RowSample) {
  using ::google::cloud::testing_util::chrono_literals::operator"" _ms;
  // TODO(#151) - remove workarounds for emulator bug(s).
  if (UsingCloudBigtableEmulator()) {
    return;
  }

  auto table = GetTable();
  std::string const prefix = "row-sample-prefix";

  int constexpr kRowCount = 20000;
  BulkMutation bulk;
  for (int row = 0; row != kRowCount; ++row) {
    std::string row_key = prefix + "/" + std::to_string(row);
    bulk.emplace_back(
        SingleRowMutation(row_key, SetCell("family1", "col", 4_ms, "foo")));
  }
  auto failures = table.BulkApply(std::move(bulk));
  ASSERT_TRUE(failures.empty());

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
  auto const min_count = static_cast<std::size_t>(
      std::floor((kSampleRate - kAllowedError) * kRowCount));
  auto const max_count = static_cast<std::size_t>(
      std::ceil((kSampleRate + kAllowedError) * kRowCount));

  // Search in the range [row_key_prefix, row_key_prefix + "0"), we used '/' as
  // the separator and the successor of "/" is "0".
  auto result = ReadRows(table, Filter::RowSample(kSampleRate));
  EXPECT_LE(min_count, result.size());
  EXPECT_GE(max_count, result.size());
}

TEST_F(FilterIntegrationTest, StripValueTransformer) {
  auto table = GetTable();
  std::string const prefix = "strip-value-transformer-prefix";
  std::vector<Cell> created{
      {prefix + "/abc0", "family1", "c0", 1000, "v1000"},
      {prefix + "/bcd0", "family2", "c1", 2000, "v2000"},
      {prefix + "/abc1", "family3", "c2", 3000, "v3000"},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000"},
      {prefix + "/hij1", "family3", "c5", 6000, "v6000"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {prefix + "/abc0", "family1", "c0", 1000, ""},
      {prefix + "/bcd0", "family2", "c1", 2000, ""},
      {prefix + "/abc1", "family3", "c2", 3000, ""},
      {prefix + "/fgh0", "family1", "c3", 4000, ""},
      {prefix + "/hij0", "family2", "c4", 4000, ""},
      {prefix + "/hij1", "family3", "c5", 6000, ""},
  };

  auto actual = ReadRows(table, Filter::StripValueTransformer());
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ApplyLabelTransformer) {
  // TODO(#151) - remove workarounds for emulator bug(s).
  if (UsingCloudBigtableEmulator()) {
    return;
  }

  auto table = GetTable();
  std::string const prefix = "apply-label-transformer-prefix";
  std::vector<Cell> created{
      {prefix + "/abc0", "family1", "c0", 1000, "v1000"},
      {prefix + "/bcd0", "family2", "c1", 2000, "v2000"},
      {prefix + "/abc1", "family3", "c2", 3000, "v3000"},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000"},
      {prefix + "/hij1", "family3", "c5", 6000, "v6000"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {prefix + "/abc0", "family1", "c0", 1000, "v1000", {"foo"}},
      {prefix + "/bcd0", "family2", "c1", 2000, "v2000", {"foo"}},
      {prefix + "/abc1", "family3", "c2", 3000, "v3000", {"foo"}},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000", {"foo"}},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000", {"foo"}},
      {prefix + "/hij1", "family3", "c5", 6000, "v6000", {"foo"}},
  };

  auto actual = ReadRows(table, Filter::ApplyLabelTransformer("foo"));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, Condition) {
  auto table = GetTable();
  std::string const prefix = "condition-prefix";
  std::vector<Cell> created{
      {prefix + "/abc0", "family1", "c0", 1000, "v1000"},
      {prefix + "/bcd0", "family2", "c1", 2000, "v2000"},
      {prefix + "/abc1", "family3", "c2", 3000, "v3000"},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000"},
      {prefix + "/hij1", "family3", "c5", 6000, "v6000"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {prefix + "/abc0", "family1", "c0", 1000, "v1000"},
      {prefix + "/bcd0", "family2", "c1", 2000, ""},
      {prefix + "/abc1", "family3", "c2", 3000, ""},
      {prefix + "/fgh0", "family1", "c3", 4000, ""},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000"},
  };

  using F = Filter;
  auto actual =
      ReadRows(table, F::Condition(F::ValueRangeClosed("v2000", "v4000"),
                                   F::StripValueTransformer(),
                                   F::FamilyRegex("family[12]")));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, Chain) {
  auto table = GetTable();
  std::string const prefix = "chain-prefix";
  std::vector<Cell> created{
      {prefix + "/abc0", "family1", "c0", 1000, "v1000"},
      {prefix + "/bcd0", "family2", "c1", 2000, "v2000"},
      {prefix + "/abc1", "family3", "c2", 3000, "v3000"},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000"},
      {prefix + "/hij1", "family3", "c5", 6000, "v6000"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {prefix + "/fgh0", "family1", "c3", 4000, ""},
  };

  using F = Filter;
  auto actual =
      ReadRows(table, F::Chain(F::ValueRangeClosed("v2000", "v5000"),
                               F::StripValueTransformer(),
                               F::ColumnRangeClosed("family1", "c2", "c3")));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, ChainFromRange) {
  auto table = GetTable();
  std::string const prefix = "chain-prefix";
  std::vector<Cell> created{
      {prefix + "/abc0", "family1", "c0", 1000, "v1000"},
      {prefix + "/bcd0", "family2", "c1", 2000, "v2000"},
      {prefix + "/abc1", "family3", "c2", 3000, "v3000"},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000"},
      {prefix + "/hij1", "family3", "c5", 6000, "v6000"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {prefix + "/fgh0", "family1", "c3", 4000, ""},
  };

  using F = Filter;
  auto range = {F::ValueRangeClosed("v2000", "v5000"),
                F::StripValueTransformer(),
                F::ColumnRangeClosed("family1", "c2", "c3")};
  auto actual = ReadRows(table, F::ChainFromRange(range.begin(), range.end()));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, Interleave) {
  auto table = GetTable();
  std::string const prefix = "interleave-prefix";
  std::vector<Cell> created{
      {prefix + "/abc0", "family1", "c0", 1000, "v1000"},
      {prefix + "/bcd0", "family2", "c1", 2000, "v2000"},
      {prefix + "/abc1", "family3", "c2", 3000, "v3000"},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000"},
      {prefix + "/hij1", "family3", "c5", 6000, "v6000"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {prefix + "/bcd0", "family2", "c1", 2000, ""},
      {prefix + "/abc1", "family3", "c2", 3000, ""},
      {prefix + "/fgh0", "family1", "c3", 4000, ""},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, ""},
  };

  using F = Filter;
  auto actual = ReadRows(
      table, F::Interleave(F::Chain(F::ValueRangeClosed("v2000", "v5000"),
                                    F::StripValueTransformer()),
                           F::ColumnRangeClosed("family1", "c2", "c3")));
  CheckEqualUnordered(expected, actual);
}

TEST_F(FilterIntegrationTest, InterleaveFromRange) {
  auto table = GetTable();
  std::string const prefix = "interleave-prefix";
  std::vector<Cell> created{
      {prefix + "/abc0", "family1", "c0", 1000, "v1000"},
      {prefix + "/bcd0", "family2", "c1", 2000, "v2000"},
      {prefix + "/abc1", "family3", "c2", 3000, "v3000"},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, "v5000"},
      {prefix + "/hij1", "family3", "c5", 6000, "v6000"},
  };
  CreateCells(table, created);
  std::vector<Cell> expected{
      {prefix + "/bcd0", "family2", "c1", 2000, ""},
      {prefix + "/abc1", "family3", "c2", 3000, ""},
      {prefix + "/fgh0", "family1", "c3", 4000, ""},
      {prefix + "/fgh0", "family1", "c3", 4000, "v4000"},
      {prefix + "/hij0", "family2", "c4", 4000, ""},
  };

  using F = Filter;
  std::vector<F> filter_collection{
      F::Chain(F::ValueRangeClosed("v2000", "v5000"),
               F::StripValueTransformer()),
      F::ColumnRangeClosed("family1", "c2", "c3")};
  auto actual =
      ReadRows(table, F::InterleaveFromRange(filter_collection.begin(),
                                             filter_collection.end()));
  CheckEqualUnordered(expected, actual);
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
