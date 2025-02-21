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

#include "google/cloud/bigtable/emulator/row_iterators.h"
#include "google/cloud/bigtable/row_range.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace {

std::string DumpColumnRow(ColumnRow const& col_row,
                          std::string const& prefix = "") {
  std::stringstream ss;
  for (auto const& cell : col_row) {
    ss << prefix << "@" << cell.first.count() << "ms: " << cell.second
       << std::endl;
  }
  return ss.str();
}

std::string DumpColumnFamilyRow(ColumnFamilyRow const& fam_row,
                                std::string const& prefix = "") {
  std::stringstream ss;
  for (auto const& col_row : fam_row) {
    ss << DumpColumnRow(col_row.second, prefix + col_row.first + " ");
  }
  return ss.str();
}

std::string DumpColumnFamily(ColumnFamily const& fam,
                             std::string const& cf_name = "") {
  std::stringstream ss;
  for (auto const& fam_row : fam) {
    ss << DumpColumnFamilyRow(fam_row.second,
                              fam_row.first + " " + cf_name + ":");
  }
  return ss.str();
}

TEST(ColumnRow, Trivial) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnRow col_row;
  EXPECT_FALSE(col_row.HasCells());
  col_row.SetCell(10_ms, "foo");
  EXPECT_TRUE(col_row.HasCells());
  col_row.SetCell(10_ms, "bar");
  EXPECT_EQ(std::next(col_row.begin()), col_row.end());
  EXPECT_EQ("bar", col_row.begin()->second);

  col_row.SetCell(0_ms, "baz");
  col_row.SetCell(20_ms, "qux");
  EXPECT_EQ("bar", col_row.lower_bound(10_ms)->second);
  EXPECT_EQ("qux", col_row.upper_bound(10_ms)->second);
}

TEST(ColumnRow, DeleteTimeRangeFinite) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnRow col_row;
  col_row.SetCell(10_ms, "foo");
  col_row.SetCell(20_ms, "bar");
  col_row.SetCell(30_ms, "baz");
  col_row.SetCell(40_ms, "qux");
  google::bigtable::v2::TimestampRange range;
  range.set_start_timestamp_micros(5000);
  range.set_end_timestamp_micros(40000);
  col_row.DeleteTimeRange(range);

  EXPECT_EQ("@40ms: qux\n", DumpColumnRow(col_row));
}

TEST(ColumnRow, DeleteTimeRangeInfinite) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnRow col_row;
  col_row.SetCell(10_ms, "foo");
  col_row.SetCell(20_ms, "bar");
  col_row.SetCell(30_ms, "baz");
  col_row.SetCell(40_ms, "qux");
  google::bigtable::v2::TimestampRange range;
  range.set_start_timestamp_micros(20000);
  col_row.DeleteTimeRange(range);

  EXPECT_EQ("@10ms: foo\n", DumpColumnRow(col_row));
}

TEST(ColumnFamilyRow, Trivial) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnFamilyRow fam_row;
  EXPECT_FALSE(fam_row.HasColumns());
  fam_row.SetCell("col1", 10_ms, "foo");
  EXPECT_TRUE(fam_row.HasColumns());
  fam_row.SetCell("col1", 10_ms, "bar");
  EXPECT_EQ(std::next(fam_row.begin()), fam_row.end());
  EXPECT_EQ("bar", fam_row.begin()->second.begin()->second);

  fam_row.SetCell("col0", 10_ms, "baz");
  fam_row.SetCell("col2", 10_ms, "qux");

  EXPECT_EQ(R"""(
col0 @10ms: baz
col1 @10ms: bar
col2 @10ms: qux
)""",
            "\n" + DumpColumnFamilyRow(fam_row));

  EXPECT_EQ("bar", fam_row.lower_bound("col1")->second.begin()->second);
  EXPECT_EQ("qux", fam_row.upper_bound("col1")->second.begin()->second);

  EXPECT_EQ(1, fam_row.DeleteColumn("col1",
                                    ::google::bigtable::v2::TimestampRange{}));

  // Verify that there is no empty column.
  EXPECT_EQ(2, std::distance(fam_row.begin(), fam_row.end()));

  google::bigtable::v2::TimestampRange not_matching_range;
  not_matching_range.set_start_timestamp_micros(10);
  not_matching_range.set_end_timestamp_micros(20);
  EXPECT_EQ(0, fam_row.DeleteColumn("col2", not_matching_range));

  EXPECT_EQ(R"""(
col0 @10ms: baz
col2 @10ms: qux
)""",
            "\n" + DumpColumnFamilyRow(fam_row));
}

TEST(ColumnFamily, Trivial) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnFamily fam;
  fam.SetCell("row1", "col0", 10_ms, "foo");
  fam.SetCell("row1", "col0", 10_ms, "bar");
  EXPECT_EQ("row1 :col0 @10ms: bar\n", DumpColumnFamily(fam));

  fam.SetCell("row0", "col0", 10_ms, "baz");
  fam.SetCell("row2", "col0", 10_ms, "qux");

  EXPECT_EQ(R"""(
row0 :col0 @10ms: baz
row1 :col0 @10ms: bar
row2 :col0 @10ms: qux
)""",
            "\n" + DumpColumnFamily(fam));

  EXPECT_EQ("col0 @10ms: bar\n",
            DumpColumnFamilyRow(fam.lower_bound("row1")->second));
  EXPECT_EQ("col0 @10ms: qux\n",
            DumpColumnFamilyRow(fam.upper_bound("row1")->second));

  EXPECT_EQ(1, fam.DeleteColumn("row1", "col0",
                                ::google::bigtable::v2::TimestampRange{}));

  // Verify that there is no empty row
  EXPECT_EQ(2, std::distance(fam.begin(), fam.end()));

  EXPECT_EQ(R"""(
row0 :col0 @10ms: baz
row2 :col0 @10ms: qux
)""",
            "\n" + DumpColumnFamily(fam));

  EXPECT_TRUE(fam.DeleteRow("row2"));
  EXPECT_FALSE(fam.DeleteRow("row_nonexistent"));

  EXPECT_EQ("row0 :col0 @10ms: baz\n", DumpColumnFamily(fam));
}

std::string DumpFilteredColumnFamilyStream(
    FilteredColumnFamilyStream& stream, NextMode next_mode = NextMode::kCell) {
  std::stringstream ss;
  for (; stream.HasValue(); stream.Next(next_mode)) {
    auto const& cell = stream.Value();
    ss << cell.row_key() << " " << cell.column_family() << ":"
       << cell.column_qualifier() << " @" << cell.timestamp().count()
       << "ms: " << cell.value() << std::endl;
  }
  return ss.str();
}

TEST(FilteredColumnFamilyStream, Empty) {
  ColumnFamily fam;
  auto included_rows = std::make_shared<StringRangeSet>(StringRangeSet::All());
  FilteredColumnFamilyStream filtered_stream(fam, "cf1", included_rows);
  EXPECT_EQ("", DumpFilteredColumnFamilyStream(filtered_stream));
}

TEST(FilteredColumnFamilyStream, Unfiltered) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnFamily fam;
  fam.SetCell("row0", "col0", 10_ms, "foo");
  fam.SetCell("row0", "col1", 20_ms, "bar");
  fam.SetCell("row0", "col1", 30_ms, "baz");
  fam.SetCell("row1", "col0", 10_ms, "foo");
  fam.SetCell("row1", "col1", 20_ms, "foo");
  fam.SetCell("row1", "col1", 30_ms, "foo");
  fam.SetCell("row2", "col0", 10_ms, "qux");
  fam.SetCell("row2", "col2", 40_ms, "qux");
  fam.SetCell("row2", "col2", 50_ms, "qux");
  auto included_rows = std::make_shared<StringRangeSet>(StringRangeSet::All());
  FilteredColumnFamilyStream filtered_stream(fam, "cf1", included_rows);
  EXPECT_EQ(R"""(
row0 cf1:col0 @10ms: foo
row0 cf1:col1 @20ms: bar
row0 cf1:col1 @30ms: baz
row1 cf1:col0 @10ms: foo
row1 cf1:col1 @20ms: foo
row1 cf1:col1 @30ms: foo
row2 cf1:col0 @10ms: qux
row2 cf1:col2 @40ms: qux
row2 cf1:col2 @50ms: qux
)""", "\n" + DumpFilteredColumnFamilyStream(filtered_stream));
}

TEST(FilteredColumnFamilyStream, FilterByTimestampRange) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnFamily fam;
  fam.SetCell("row0", "col0", 10_ms, "foo");
  fam.SetCell("row0", "col0", 20_ms, "bar");  // Filter out
  fam.SetCell("row0", "col0", 30_ms, "baz");
  fam.SetCell("row0", "col1", 100_ms, "foo");  // Filter out
  fam.SetCell("row0", "col1", 150_ms, "foo");  // Filter out
  fam.SetCell("row0", "col1", 190_ms, "foo");  // Filter out
  fam.SetCell("row0", "col2", 200_ms, "foo");
  fam.SetCell("row0", "col2", 220_ms, "foo");
  fam.SetCell("row0", "col2", 240_ms, "foo");
  fam.SetCell("row0", "col3", 300_ms, "foo");  // Filter out
  fam.SetCell("row0", "col3", 300_ms, "foo");  // Filter out
  fam.SetCell("row0", "col3", 300_ms, "foo");  // Filter out
  fam.SetCell("row1", "col0", 10_ms, "foo");
  fam.SetCell("row1", "col0", 20_ms, "bar");  // Filter out
  fam.SetCell("row1", "col0", 30_ms, "baz");
  fam.SetCell("row1", "col1", 100_ms, "foo");  // Filter out
  fam.SetCell("row1", "col1", 150_ms, "foo");  // Filter out
  fam.SetCell("row1", "col1", 190_ms, "foo");  // Filter out
  fam.SetCell("row1", "col2", 200_ms, "foo");
  fam.SetCell("row1", "col2", 220_ms, "foo");
  fam.SetCell("row1", "col2", 240_ms, "foo");
  fam.SetCell("row1", "col3", 300_ms, "foo");  // Filter out
  fam.SetCell("row1", "col3", 300_ms, "foo");  // Filter out
  fam.SetCell("row1", "col3", 300_ms, "foo");  // Filter out
  auto included_rows = std::make_shared<StringRangeSet>(StringRangeSet::All());
  FilteredColumnFamilyStream filtered_stream(fam, "cf1", included_rows);
  filtered_stream.ApplyFilter(
      TimestampRange{TimestampRangeSet::Range(0_ms, 20_ms)});
  filtered_stream.ApplyFilter(
      TimestampRange{TimestampRangeSet::Range(30_ms, 100_ms)});
  filtered_stream.ApplyFilter(
      TimestampRange{TimestampRangeSet::Range(200_ms, 300_ms)});
  EXPECT_EQ(R"""(
row0 cf1:col0 @10ms: foo
row0 cf1:col0 @30ms: baz
row0 cf1:col2 @200ms: foo
row0 cf1:col2 @220ms: foo
row0 cf1:col2 @240ms: foo
row1 cf1:col0 @10ms: foo
row1 cf1:col0 @30ms: baz
row1 cf1:col2 @200ms: foo
row1 cf1:col2 @220ms: foo
row1 cf1:col2 @240ms: foo
)""", "\n" + DumpFilteredColumnFamilyStream(filtered_stream));
}

TEST(FilteredColumnFamilyStream, FilterByColumnRange) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnFamily fam;
  fam.SetCell("row0", "col0", 10_ms, "foo");
  fam.SetCell("row0", "col1", 100_ms, "foo");  // Filter out
  fam.SetCell("row0", "col2", 200_ms, "foo");
  fam.SetCell("row0", "col3", 300_ms, "foo");  // Filter out
  fam.SetCell("row0", "col3", 300_ms, "foo");  // Filter out
  fam.SetCell("row1", "col2", 300_ms, "foo");
  fam.SetCell("row2", "col0", 300_ms, "foo");  
  auto included_rows = std::make_shared<StringRangeSet>(StringRangeSet::All());
  FilteredColumnFamilyStream filtered_stream(fam, "cf1", included_rows);
  filtered_stream.ApplyFilter(
      ColumnRange{StringRangeSet::Range("col0", false, "col0", false)});
  filtered_stream.ApplyFilter(
      ColumnRange{StringRangeSet::Range("col2", false, "col2", false)});
  EXPECT_EQ(R"""(
row0 cf1:col0 @10ms: foo
row0 cf1:col2 @200ms: foo
row1 cf1:col2 @300ms: foo
row2 cf1:col0 @300ms: foo
)""", "\n" + DumpFilteredColumnFamilyStream(filtered_stream));
}

TEST(FilteredColumnFamilyStream, FilterByColumnRegex) {
  using testing_util::chrono_literals::operator""_ms;
  auto pattern1 = std::make_shared<re2::RE2>("col");
  ASSERT_TRUE(pattern1->ok());
  auto pattern2 = std::make_shared<re2::RE2>("[02]");
  ASSERT_TRUE(pattern2->ok());

  ColumnFamily fam;
  fam.SetCell("row0", "col0", 10_ms, "foo");
  fam.SetCell("row0", "col1", 100_ms, "foo");  // Filter out
  fam.SetCell("row0", "col2", 200_ms, "foo");
  fam.SetCell("row0", "col3", 300_ms, "foo");  // Filter out
  fam.SetCell("row0", "col3", 300_ms, "foo");  // Filter out
  fam.SetCell("row1", "col2", 300_ms, "foo");
  fam.SetCell("row2", "col0", 300_ms, "foo");  
  auto included_rows = std::make_shared<StringRangeSet>(StringRangeSet::All());
  FilteredColumnFamilyStream filtered_stream(fam, "cf1", included_rows);
  filtered_stream.ApplyFilter(ColumnRegex{pattern1});
  filtered_stream.ApplyFilter(ColumnRegex{pattern2});
  EXPECT_EQ(R"""(
row0 cf1:col0 @10ms: foo
row0 cf1:col2 @200ms: foo
row1 cf1:col2 @300ms: foo
row2 cf1:col0 @300ms: foo
)""", "\n" + DumpFilteredColumnFamilyStream(filtered_stream));
}

TEST(FilteredColumnFamilyStream, FilterRowKeyRegex) {
  using testing_util::chrono_literals::operator""_ms;
  auto pattern1 = std::make_shared<re2::RE2>("row");
  ASSERT_TRUE(pattern1->ok());
  auto pattern2 = std::make_shared<re2::RE2>("[02]");
  ASSERT_TRUE(pattern2->ok());

  ColumnFamily fam;
  fam.SetCell("row0", "col0", 10_ms, "foo");
  fam.SetCell("row1", "col1", 100_ms, "foo");  // Filter out
  fam.SetCell("row2", "col2", 200_ms, "foo");
  fam.SetCell("row3", "col3", 300_ms, "foo");  // Filter out
  auto included_rows = std::make_shared<StringRangeSet>(StringRangeSet::All());
  FilteredColumnFamilyStream filtered_stream(fam, "cf1", included_rows);
  filtered_stream.ApplyFilter(RowKeyRegex{pattern1});
  filtered_stream.ApplyFilter(RowKeyRegex{pattern2});
  EXPECT_EQ(R"""(
row0 cf1:col0 @10ms: foo
row2 cf1:col2 @200ms: foo
)""", "\n" + DumpFilteredColumnFamilyStream(filtered_stream));
}

TEST(FilteredColumnFamilyStream, FilterRowSet) {
  using testing_util::chrono_literals::operator""_ms;

  ColumnFamily fam;
  fam.SetCell("row0", "col0", 10_ms, "foo");
  fam.SetCell("row1", "col1", 100_ms, "foo");  // Filter out
  fam.SetCell("row2", "col2", 200_ms, "foo");
  fam.SetCell("row3", "col3", 300_ms, "foo");  // Filter out
  auto included_rows =
      std::make_shared<StringRangeSet>(StringRangeSet::Empty());
  included_rows->Sum(StringRangeSet::Range("row0", false, "row2", true));
  included_rows->Sum(StringRangeSet::Range(
      "row3", false, StringRangeSet::Range::Infinity{}, false));
  FilteredColumnFamilyStream filtered_stream(fam, "cf1", included_rows);
  EXPECT_EQ(R"""(
row0 cf1:col0 @10ms: foo
row1 cf1:col1 @100ms: foo
row3 cf1:col3 @300ms: foo
)""", "\n" + DumpFilteredColumnFamilyStream(filtered_stream));
}

}  // anonymous namespace
}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google


