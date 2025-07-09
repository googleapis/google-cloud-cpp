// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/emulator/table.h"
#include "google/cloud/bigtable/emulator/column_family.h"
#include "google/cloud/bigtable/emulator/filter.h"
#include "google/cloud/bigtable/emulator/range_set.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <gtest/gtest.h>
#include <re2/re2.h>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace {

std::string DumpStream(AbstractCellStreamImpl& stream,
                       NextMode next_mode = NextMode::kCell) {
  std::stringstream ss;
  for (; stream.HasValue(); stream.Next(next_mode)) {
    auto const& cell = stream.Value();
    ss << cell.row_key() << " " << cell.column_family() << ":"
       << cell.column_qualifier() << " @" << cell.timestamp().count()
       << "ms: " << cell.value() << std::endl;
  }
  return ss.str();
}

TEST(FilteredTableStream, Empty) {
  FilteredTableStream stream({});
  EXPECT_EQ("", DumpStream(stream));
}

TEST(FilteredTableStream, EmptyColumnFamilies) {
  ColumnFamily fam1;
  ColumnFamily fam2;
  auto ffam1 = std::make_unique<FilteredColumnFamilyStream>(
      fam1, "fam1", std::make_unique<StringRangeSet>(StringRangeSet::All()));
  auto ffam2 = std::make_unique<FilteredColumnFamilyStream>(
      fam2, "fam2", std::make_unique<StringRangeSet>(StringRangeSet::All()));
  std::vector<std::unique_ptr<FilteredColumnFamilyStream>> fams;
  fams.emplace_back(std::move(ffam1));
  fams.emplace_back(std::move(ffam2));
  FilteredTableStream stream(std::move(fams));
  EXPECT_EQ("", DumpStream(stream));
}

TEST(FilteredTableStream, ColumnFamiliesAreFiltered) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnFamily fam1;
  ColumnFamily fam2;
  fam1.SetCell("row0", "col0", 10_ms, "foo");
  fam2.SetCell("row0", "col0", 10_ms, "foo");
  auto ffam1 = std::make_unique<FilteredColumnFamilyStream>(
      fam1, "fam1", std::make_unique<StringRangeSet>(StringRangeSet::All()));
  auto ffam2 = std::make_unique<FilteredColumnFamilyStream>(
      fam2, "fam2", std::make_unique<StringRangeSet>(StringRangeSet::All()));
  std::vector<std::unique_ptr<FilteredColumnFamilyStream>> fams;
  fams.emplace_back(std::move(ffam1));
  fams.emplace_back(std::move(ffam2));
  FilteredTableStream stream(std::move(fams));
  auto family_pattern = std::make_shared<re2::RE2>("fam1");
  ASSERT_TRUE(family_pattern->ok());
  stream.ApplyFilter(FamilyNameRegex{family_pattern});
  EXPECT_EQ("row0 fam1:col0 @10ms: foo\n", DumpStream(stream));
}

TEST(FilteredTableStream, OnlyRightFamilyColumnsAreFiltered) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnFamily fam1;
  ColumnFamily fam2;
  fam1.SetCell("row0", "col0", 10_ms, "foo");
  fam2.SetCell("row0", "col0", 10_ms, "foo");
  auto ffam1 = std::make_unique<FilteredColumnFamilyStream>(
      fam1, "fam1", std::make_unique<StringRangeSet>(StringRangeSet::All()));
  auto ffam2 = std::make_unique<FilteredColumnFamilyStream>(
      fam2, "fam2", std::make_unique<StringRangeSet>(StringRangeSet::All()));
  std::vector<std::unique_ptr<FilteredColumnFamilyStream>> fams;
  fams.emplace_back(std::move(ffam1));
  fams.emplace_back(std::move(ffam2));
  FilteredTableStream stream(std::move(fams));

  stream.ApplyFilter(
      ColumnRange{"fam2", StringRangeSet::Range("col0", false, "col1", true)});
  EXPECT_EQ("row0 fam2:col0 @10ms: foo\n", DumpStream(stream));
}

TEST(FilteredTableStream, OtherFiltersArePropagated) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnFamily fam1;
  ColumnFamily fam2;
  fam1.SetCell("row1", "col1", 10_ms, "foo");
  fam1.SetCell("row0", "col1", 10_ms, "foo");    // row key regex
  fam2.SetCell("row1", "col1", 10_ms, "foo");    // column family regex
  fam1.SetCell("row1", "col2", 10_ms, "foo");    // column qualifier regex
  fam1.SetCell("row1", "a1", 10_ms, "foo");      // column range
  fam1.SetCell("row1", "col1", 1000_ms, "foo");  // timestamp range
  auto ffam1 = std::make_unique<FilteredColumnFamilyStream>(
      fam1, "fam1", std::make_unique<StringRangeSet>(StringRangeSet::All()));
  auto ffam2 = std::make_unique<FilteredColumnFamilyStream>(
      fam2, "fam2", std::make_unique<StringRangeSet>(StringRangeSet::All()));
  std::vector<std::unique_ptr<FilteredColumnFamilyStream>> fams;
  fams.emplace_back(std::move(ffam1));
  fams.emplace_back(std::move(ffam2));
  FilteredTableStream stream(std::move(fams));

  auto row_key_pattern = std::make_shared<re2::RE2>("row1");
  ASSERT_TRUE(row_key_pattern->ok());
  EXPECT_TRUE(stream.ApplyFilter(RowKeyRegex{row_key_pattern}));

  auto family_pattern = std::make_shared<re2::RE2>("fam1");
  ASSERT_TRUE(family_pattern->ok());
  EXPECT_TRUE(stream.ApplyFilter(FamilyNameRegex{family_pattern}));

  auto qualifier_pattern = std::make_shared<re2::RE2>("1$");
  ASSERT_TRUE(qualifier_pattern->ok());
  EXPECT_TRUE(stream.ApplyFilter(ColumnRegex{qualifier_pattern}));

  EXPECT_TRUE(stream.ApplyFilter(
      ColumnRange{"fam1", StringRangeSet::Range("co", false, "com", false)}));

  EXPECT_TRUE(stream.ApplyFilter(
      TimestampRange{TimestampRangeSet::Range(0_ms, 300_ms)}));

  EXPECT_EQ("row1 fam1:col1 @10ms: foo\n", DumpStream(stream));
}

}  // anonymous namespace
}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
