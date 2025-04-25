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

#include "google/cloud/bigtable/emulator/range_set.h"
#include "google/cloud/bigtable/row_range.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace {

bool const kOpen = true;
bool const kClosed = false;
bool const kWhatever = true;  // to indicate it's unimportant in the test

TEST(StringRangeValueOrder, Simple) {
  EXPECT_EQ(-1, detail::CompareRangeValues("A", "B"));
  EXPECT_EQ(0, detail::CompareRangeValues("A", "A"));
  EXPECT_EQ(1, detail::CompareRangeValues("B", "A"));
}

TEST(StringRangeValueOrder, Empty) {
  EXPECT_EQ(-1, detail::CompareRangeValues("", "A"));
  EXPECT_EQ(0, detail::CompareRangeValues("", ""));
  EXPECT_EQ(1, detail::CompareRangeValues("A", ""));
}

TEST(StringRangeValueOrder, Infinite) {
  EXPECT_EQ(-1,
            detail::CompareRangeValues("A", StringRangeSet::Range::Infinity{}));
  EXPECT_EQ(0, detail::CompareRangeValues(StringRangeSet::Range::Infinity{},
                                          StringRangeSet::Range::Infinity{}));
  EXPECT_EQ(1,
            detail::CompareRangeValues(StringRangeSet::Range::Infinity{}, "A"));

  EXPECT_EQ(-1,
            detail::CompareRangeValues("", StringRangeSet::Range::Infinity{}));
  EXPECT_EQ(1,
            detail::CompareRangeValues(StringRangeSet::Range::Infinity{}, ""));
}

// FIXME add invalid data tests
TEST(StringRangeSet, FromRowRangeClosed) {
  auto closed = StringRangeSet::Range::FromRowRange(
      RowRange::Closed("A", "B").as_proto());
  EXPECT_STATUS_OK(closed);
  EXPECT_EQ("A", closed->start());
  EXPECT_EQ("B", closed->end());
  EXPECT_TRUE(closed->start_closed());
  EXPECT_TRUE(closed->end_closed());
  EXPECT_FALSE(closed->start_open());
  EXPECT_FALSE(closed->end_open());
}

TEST(StringRangeSet, FromRowRangeOpen) {
  auto open =
      StringRangeSet::Range::FromRowRange(RowRange::Open("A", "B").as_proto());
  EXPECT_STATUS_OK(open);
  EXPECT_EQ("A", open->start());
  EXPECT_EQ("B", open->end());
  EXPECT_FALSE(open->start_closed());
  EXPECT_FALSE(open->end_closed());
  EXPECT_TRUE(open->start_open());
  EXPECT_TRUE(open->end_open());
}

TEST(StringRangeSet, FromRowRangeImplicitlyInfinite) {
  auto range =
      StringRangeSet::Range::FromRowRange(google::bigtable::v2::RowRange{});

  EXPECT_STATUS_OK(range);
  EXPECT_EQ("", range->start());
  EXPECT_EQ(StringRangeSet::Range::Infinity{}, range->end());
  EXPECT_TRUE(range->start_closed());
  EXPECT_TRUE(range->end_closed());
  EXPECT_FALSE(range->start_open());
  EXPECT_FALSE(range->end_open());
}

TEST(StringRangeSet, FromRowRangeExplicitlyInfinite) {
  for (bool end_open : {true, false}) {
    google::bigtable::v2::RowRange proto_range;
    proto_range.set_start_key_closed("");
    if (end_open) {
      proto_range.set_end_key_open("");
    } else {
      proto_range.set_end_key_closed("");
    }

    auto range = StringRangeSet::Range::FromRowRange(proto_range);
    EXPECT_STATUS_OK(range);
    EXPECT_EQ("", range->start());
    EXPECT_EQ(StringRangeSet::Range::Infinity{}, range->end());
    EXPECT_TRUE(range->start_closed());
    EXPECT_TRUE(range->end_closed());
    EXPECT_FALSE(range->start_open());
    EXPECT_FALSE(range->end_open());
  }
}

TEST(StringRangeSet, FromColumnRangeClosed) {
  google::bigtable::v2::ColumnRange proto_range;
  proto_range.set_start_qualifier_closed("A");
  proto_range.set_end_qualifier_closed("B");
  auto closed = StringRangeSet::Range::FromColumnRange(proto_range);
  EXPECT_STATUS_OK(closed);
  EXPECT_EQ("A", closed->start());
  EXPECT_EQ("B", closed->end());
  EXPECT_TRUE(closed->start_closed());
  EXPECT_TRUE(closed->end_closed());
  EXPECT_FALSE(closed->start_open());
  EXPECT_FALSE(closed->end_open());
}

TEST(StringRangeSet, FromColumnRangeOpen) {
  google::bigtable::v2::ColumnRange proto_range;
  proto_range.set_start_qualifier_open("A");
  proto_range.set_end_qualifier_open("B");
  auto open = StringRangeSet::Range::FromColumnRange(proto_range);
  EXPECT_STATUS_OK(open);
  EXPECT_EQ("A", open->start());
  EXPECT_EQ("B", open->end());
  EXPECT_FALSE(open->start_closed());
  EXPECT_FALSE(open->end_closed());
  EXPECT_TRUE(open->start_open());
  EXPECT_TRUE(open->end_open());
}

TEST(StringRangeSet, FromColumnRangeImplicitlyInfinite) {
  auto range = StringRangeSet::Range::FromColumnRange(
      google::bigtable::v2::ColumnRange{});

  EXPECT_STATUS_OK(range);
  EXPECT_EQ("", range->start());
  EXPECT_EQ(StringRangeSet::Range::Infinity{}, range->end());
  EXPECT_TRUE(range->start_closed());
  EXPECT_TRUE(range->end_closed());
  EXPECT_FALSE(range->start_open());
  EXPECT_FALSE(range->end_open());
}

TEST(StringRangeSet, FromColumnRangeExplicitlyInfinite) {
  for (bool end_open : {true, false}) {
    google::bigtable::v2::ColumnRange proto_range;
    proto_range.set_start_qualifier_closed("");
    if (end_open) {
      proto_range.set_end_qualifier_open("");
    } else {
      proto_range.set_end_qualifier_closed("");
    }

    auto range = StringRangeSet::Range::FromColumnRange(proto_range);
    EXPECT_STATUS_OK(range);
    EXPECT_EQ("", range->start());
    EXPECT_EQ(StringRangeSet::Range::Infinity{}, range->end());
    EXPECT_TRUE(range->start_closed());
    EXPECT_TRUE(range->end_closed());
    EXPECT_FALSE(range->start_open());
    EXPECT_FALSE(range->end_open());
  }
}

TEST(StringRangeSet, FromValueRangeClosed) {
  google::bigtable::v2::ValueRange proto_range;
  proto_range.set_start_value_closed("A");
  proto_range.set_end_value_closed("B");
  auto closed = StringRangeSet::Range::FromValueRange(proto_range);
  EXPECT_STATUS_OK(closed);
  EXPECT_EQ("A", closed->start());
  EXPECT_EQ("B", closed->end());
  EXPECT_TRUE(closed->start_closed());
  EXPECT_TRUE(closed->end_closed());
  EXPECT_FALSE(closed->start_open());
  EXPECT_FALSE(closed->end_open());
}

TEST(StringRangeSet, FromValueRangeOpen) {
  google::bigtable::v2::ValueRange proto_range;
  proto_range.set_start_value_open("A");
  proto_range.set_end_value_open("B");
  auto open = StringRangeSet::Range::FromValueRange(proto_range);
  EXPECT_STATUS_OK(open);
  EXPECT_EQ("A", open->start());
  EXPECT_EQ("B", open->end());
  EXPECT_FALSE(open->start_closed());
  EXPECT_FALSE(open->end_closed());
  EXPECT_TRUE(open->start_open());
  EXPECT_TRUE(open->end_open());
}

TEST(StringRangeSet, FromValueRangeImplicitlyInfinite) {
  auto range =
      StringRangeSet::Range::FromValueRange(google::bigtable::v2::ValueRange{});

  EXPECT_STATUS_OK(range);
  EXPECT_EQ("", range->start());
  EXPECT_EQ(StringRangeSet::Range::Infinity{}, range->end());
  EXPECT_TRUE(range->start_closed());
  EXPECT_TRUE(range->end_closed());
  EXPECT_FALSE(range->start_open());
  EXPECT_FALSE(range->end_open());
}

TEST(StringRangeSet, FromValueRangeExplicitlyInfinite) {
  for (bool end_open : {true, false}) {
    google::bigtable::v2::ValueRange proto_range;
    proto_range.set_start_value_closed("");
    if (end_open) {
      proto_range.set_end_value_open("");
    } else {
      proto_range.set_end_value_closed("");
    }

    auto range = StringRangeSet::Range::FromValueRange(proto_range);
    EXPECT_STATUS_OK(range);
    EXPECT_EQ("", range->start());
    EXPECT_EQ(StringRangeSet::Range::Infinity{}, range->end());
    EXPECT_TRUE(range->start_closed());
    EXPECT_TRUE(range->end_closed());
    EXPECT_FALSE(range->start_open());
    EXPECT_FALSE(range->end_open());
  }
}

TEST(StringRangeSet, RangeValueLess) {
  EXPECT_TRUE(StringRangeSet::Range::ValueLess()("A", "B"));
  EXPECT_FALSE(StringRangeSet::Range::ValueLess()("A", "A"));
  EXPECT_FALSE(StringRangeSet::Range::ValueLess()("B", "A"));
}

TEST(StringRangeSet, RangeStartLess) {
  EXPECT_TRUE(StringRangeSet::Range::StartLess()(
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever),
      StringRangeSet::Range("B", kOpen, "unimportant", kWhatever)));
  EXPECT_FALSE(StringRangeSet::Range::StartLess()(
      StringRangeSet::Range("B", kOpen, "unimportant", kWhatever),
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever)));
  EXPECT_FALSE(StringRangeSet::Range::StartLess()(
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever),
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever)));

  EXPECT_TRUE(StringRangeSet::Range::StartLess()(
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever),
      StringRangeSet::Range("B", kClosed, "unimportant", kWhatever)));
  EXPECT_FALSE(StringRangeSet::Range::StartLess()(
      StringRangeSet::Range("B", kClosed, "unimportant", kWhatever),
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever)));
  EXPECT_FALSE(StringRangeSet::Range::StartLess()(
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever),
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever)));

  EXPECT_FALSE(StringRangeSet::Range::StartLess()(
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever),
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever)));
  EXPECT_TRUE(StringRangeSet::Range::StartLess()(
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever),
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever)));
}

TEST(StringRangeSet, RangeEndLess) {
  EXPECT_TRUE(StringRangeSet::Range::EndLess()(
      StringRangeSet::Range("A", kWhatever, "A", kOpen),
      StringRangeSet::Range("A", kWhatever, "B", kOpen)));
  EXPECT_FALSE(StringRangeSet::Range::EndLess()(
      StringRangeSet::Range("A", kWhatever, "B", kOpen),
      StringRangeSet::Range("A", kWhatever, "A", kOpen)));
  EXPECT_FALSE(StringRangeSet::Range::EndLess()(
      StringRangeSet::Range("A", kWhatever, "A", kOpen),
      StringRangeSet::Range("A", kWhatever, "A", kOpen)));

  EXPECT_TRUE(StringRangeSet::Range::EndLess()(
      StringRangeSet::Range("A", kWhatever, "A", kClosed),
      StringRangeSet::Range("A", kWhatever, "B", kClosed)));
  EXPECT_FALSE(StringRangeSet::Range::EndLess()(
      StringRangeSet::Range("A", kWhatever, "B", kClosed),
      StringRangeSet::Range("A", kWhatever, "A", kClosed)));
  EXPECT_FALSE(StringRangeSet::Range::EndLess()(
      StringRangeSet::Range("A", kWhatever, "A", kClosed),
      StringRangeSet::Range("A", kWhatever, "A", kClosed)));

  EXPECT_FALSE(StringRangeSet::Range::EndLess()(
      StringRangeSet::Range("A", kWhatever, "A", kClosed),
      StringRangeSet::Range("A", kWhatever, "A", kOpen)));
  EXPECT_TRUE(StringRangeSet::Range::EndLess()(
      StringRangeSet::Range("A", kWhatever, "A", kOpen),
      StringRangeSet::Range("A", kWhatever, "A", kClosed)));
}

TEST(StringRangeSet, BelowStart) {
  StringRangeSet::Range const open("B", kOpen, "unimportant", kWhatever);
  StringRangeSet::Range const closed("B", kClosed, "unimportant", kWhatever);
  StringRangeSet::Range const infinite(
      StringRangeSet::Range::Infinity{}, kClosed,
      StringRangeSet::Range::Infinity{}, kClosed);

  EXPECT_TRUE(open.IsBelowStart("A"));
  EXPECT_TRUE(closed.IsBelowStart("A"));
  EXPECT_TRUE(open.IsBelowStart("B"));
  EXPECT_FALSE(closed.IsBelowStart("B"));
  EXPECT_FALSE(open.IsBelowStart("C"));
  EXPECT_FALSE(closed.IsBelowStart("C"));
  EXPECT_FALSE(open.IsBelowStart(StringRangeSet::Range::Infinity{}));
  EXPECT_FALSE(closed.IsBelowStart(StringRangeSet::Range::Infinity{}));
  EXPECT_TRUE(infinite.IsBelowStart("whatever_string"));
  EXPECT_FALSE(infinite.IsBelowStart(StringRangeSet::Range::Infinity{}));
}

TEST(StringRangeSet, AboveEnd) {
  StringRangeSet::Range const open("A", kWhatever, "B", kOpen);
  StringRangeSet::Range const closed("A", kWhatever, "B", kClosed);
  StringRangeSet::Range const infinite(
      "unimportant", kWhatever, StringRangeSet::Range::Infinity{}, kClosed);

  EXPECT_FALSE(open.IsAboveEnd("A"));
  EXPECT_FALSE(closed.IsAboveEnd("A"));
  EXPECT_TRUE(open.IsAboveEnd("B"));
  EXPECT_FALSE(closed.IsAboveEnd("B"));
  EXPECT_TRUE(open.IsAboveEnd("C"));
  EXPECT_TRUE(closed.IsAboveEnd("C"));
  EXPECT_FALSE(infinite.IsAboveEnd("whatever_string"));
  EXPECT_FALSE(infinite.IsAboveEnd(StringRangeSet::Range::Infinity{}));
}

TEST(StringRangeSet, IsWithin) {
  StringRangeSet::Range const closed("A", kClosed, "C", kClosed);
  EXPECT_FALSE(closed.IsWithin(""));
  EXPECT_TRUE(closed.IsWithin("A"));
  EXPECT_TRUE(closed.IsWithin("B"));
  EXPECT_TRUE(closed.IsWithin("C"));
  EXPECT_FALSE(closed.IsWithin("D"));
  EXPECT_FALSE(closed.IsWithin(StringRangeSet::Range::Infinity{}));

  StringRangeSet::Range const open("A", kOpen, "C", kOpen);
  EXPECT_FALSE(open.IsWithin(""));
  EXPECT_FALSE(open.IsWithin("A"));
  EXPECT_TRUE(open.IsWithin("B"));
  EXPECT_FALSE(open.IsWithin("C"));
  EXPECT_FALSE(open.IsWithin("D"));
  EXPECT_FALSE(open.IsWithin(StringRangeSet::Range::Infinity{}));
}

TEST(StringRangeSet, RangeEqality) {
  EXPECT_EQ(StringRangeSet::Range("A", kClosed, "B", kOpen),
            StringRangeSet::Range("A", kClosed, "B", kOpen));

  EXPECT_FALSE(StringRangeSet::Range("A", kClosed, "B", kOpen) ==
               StringRangeSet::Range("B", kClosed, "B", kOpen));
  EXPECT_FALSE(StringRangeSet::Range("A", kClosed, "B", kOpen) ==
               StringRangeSet::Range("A", kOpen, "B", kOpen));
  EXPECT_FALSE(StringRangeSet::Range("A", kClosed, "B", kOpen) ==
               StringRangeSet::Range("A", kClosed, "C", kOpen));
  EXPECT_FALSE(StringRangeSet::Range("A", kClosed, "B", kOpen) ==
               StringRangeSet::Range("A", kClosed, "B", kClosed));
}

TEST(StringRangeSet, RangePrint) {
  {
    std::stringstream os;
    os << StringRangeSet::Range("A", kClosed, "B", kOpen);
    EXPECT_EQ("[A,B)", os.str());
  }
  {
    std::stringstream os;
    os << StringRangeSet::Range("A", kOpen, "B", kClosed);
    EXPECT_EQ("(A,B]", os.str());
  }
  {
    std::stringstream os;
    os << StringRangeSet::Range("", kOpen, "", kClosed);
    EXPECT_EQ("(,]", os.str());
  }
  {
    std::stringstream os;
    os << StringRangeSet::Range(StringRangeSet::Range::Infinity{}, kClosed,
                                StringRangeSet::Range::Infinity{}, kClosed);
    EXPECT_EQ("[inf,inf]", os.str());
  }
}

// FIXME - test ConsecutiveRowKeys

TEST(StringRangeSet, IsEmpty) {
  EXPECT_FALSE(StringRangeSet::Range("A", kClosed, "A", kClosed).IsEmpty());
  EXPECT_TRUE(StringRangeSet::Range("A", kClosed, "A", kOpen).IsEmpty());
  EXPECT_TRUE(StringRangeSet::Range("A", kOpen, "A", kClosed).IsEmpty());
  EXPECT_TRUE(StringRangeSet::Range("A", kOpen, "A", kOpen).IsEmpty());

  EXPECT_FALSE(StringRangeSet::Range("A", kClosed, "C", kClosed).IsEmpty());
  EXPECT_FALSE(StringRangeSet::Range("A", kOpen, "C", kClosed).IsEmpty());
  EXPECT_FALSE(StringRangeSet::Range("A", kClosed, "C", kOpen).IsEmpty());
  EXPECT_FALSE(StringRangeSet::Range("A", kOpen, "C", kOpen).IsEmpty());

  EXPECT_FALSE(
      StringRangeSet::Range("A", kClosed, std::string("A\0", 2), kClosed)
          .IsEmpty());
  EXPECT_FALSE(StringRangeSet::Range("A", kOpen, std::string("A\0", 2), kClosed)
                   .IsEmpty());
  EXPECT_FALSE(StringRangeSet::Range("A", kClosed, std::string("A\0", 2), kOpen)
                   .IsEmpty());
  EXPECT_TRUE(StringRangeSet::Range("A", kOpen, std::string("A\0", 2), kOpen)
                  .IsEmpty());

  EXPECT_FALSE(StringRangeSet::Range("A", kClosed,
                                     StringRangeSet::Range::Infinity{}, kClosed)
                   .IsEmpty());
  EXPECT_FALSE(StringRangeSet::Range("", kClosed,
                                     StringRangeSet::Range::Infinity{}, kClosed)
                   .IsEmpty());
  EXPECT_TRUE(StringRangeSet::Range(StringRangeSet::Range::Infinity{}, kClosed,
                                    StringRangeSet::Range::Infinity{}, kClosed)
                  .IsEmpty());
}

TEST(StringRangeSet, HasOverlap) {
  EXPECT_FALSE(
      detail::HasOverlap(StringRangeSet::Range("B", kClosed, "D", kClosed),
                         StringRangeSet::Range("A", kClosed, "A", kClosed)));
  EXPECT_FALSE(
      detail::HasOverlap(StringRangeSet::Range("B", kClosed, "D", kClosed),
                         StringRangeSet::Range("A", kClosed, "B", kOpen)));
  EXPECT_FALSE(
      detail::HasOverlap(StringRangeSet::Range("B", kOpen, "D", kClosed),
                         StringRangeSet::Range("A", kClosed, "B", kClosed)));
  EXPECT_FALSE(detail::HasOverlap(
      StringRangeSet::Range("B", kOpen, "D", kClosed),
      StringRangeSet::Range("A", kClosed, std::string("B\0", 2), kOpen)));
  EXPECT_TRUE(detail::HasOverlap(
      StringRangeSet::Range("B", kClosed, "D", kClosed),
      StringRangeSet::Range("A", kClosed, std::string("B\0", 2), kOpen)));
  EXPECT_TRUE(detail::HasOverlap(
      StringRangeSet::Range("B", kOpen, "D", kClosed),
      StringRangeSet::Range("A", kClosed, std::string("B\0", 2), kClosed)));
  EXPECT_TRUE(
      detail::HasOverlap(StringRangeSet::Range("B", kClosed, "D", kClosed),
                         StringRangeSet::Range("A", kClosed, "B", kClosed)));
  EXPECT_TRUE(
      detail::HasOverlap(StringRangeSet::Range("B", kClosed, "D", kClosed),
                         StringRangeSet::Range("A", kClosed, "C", kOpen)));
  EXPECT_TRUE(detail::HasOverlap(
      StringRangeSet::Range("B", kClosed, "D", kClosed),
      StringRangeSet::Range("A", kClosed, StringRangeSet::Range::Infinity{},
                            kClosed)));

  EXPECT_FALSE(
      detail::HasOverlap(StringRangeSet::Range("B", kClosed, "D", kOpen),
                         StringRangeSet::Range("D", kClosed, "E", kOpen)));
  EXPECT_FALSE(detail::HasOverlap(
      StringRangeSet::Range("B", kClosed, std::string("D\0", 2), kOpen),
      StringRangeSet::Range("D", kOpen, "E", kOpen)));
  EXPECT_TRUE(detail::HasOverlap(
      StringRangeSet::Range("B", kClosed, std::string("D\0", 2), kClosed),
      StringRangeSet::Range("D", kOpen, "E", kOpen)));
  EXPECT_TRUE(detail::HasOverlap(
      StringRangeSet::Range("B", kClosed, std::string("D\0", 2), kOpen),
      StringRangeSet::Range("D", kClosed, "E", kOpen)));
  EXPECT_TRUE(detail::HasOverlap(
      StringRangeSet::Range("B", kClosed, "D", kClosed),
      StringRangeSet::Range("D", kClosed, StringRangeSet::Range::Infinity{},
                            kClosed)));
  EXPECT_FALSE(detail::HasOverlap(
      StringRangeSet::Range("B", kClosed, "D", kClosed),
      StringRangeSet::Range("D", kOpen, StringRangeSet::Range::Infinity{},
                            kClosed)));
  EXPECT_FALSE(detail::HasOverlap(
      StringRangeSet::Range("B", kClosed, "D", kOpen),
      StringRangeSet::Range("D", kClosed, StringRangeSet::Range::Infinity{},
                            kClosed)));
  EXPECT_FALSE(detail::HasOverlap(
      StringRangeSet::Range("B", kClosed, "D", kOpen),
      StringRangeSet::Range("D", kOpen, StringRangeSet::Range::Infinity{},
                            kClosed)));
  EXPECT_FALSE(detail::HasOverlap(
      StringRangeSet::Range("B", kClosed, "D", kClosed),
      StringRangeSet::Range("E", kClosed, StringRangeSet::Range::Infinity{},
                            kClosed)));
  EXPECT_FALSE(
      detail::HasOverlap(StringRangeSet::Range("B", kClosed, "D", kClosed),
                         StringRangeSet::Range("D", kOpen, "E", kOpen)));
}

TEST(StringRangeSet, DisjointAdjacent) {
  EXPECT_FALSE(detail::DisjointAndSortedRangesAdjacent(
      StringRangeSet::Range("A", kWhatever, "B", kOpen),
      StringRangeSet::Range("C", kOpen, "D", kWhatever)));
  EXPECT_FALSE(detail::DisjointAndSortedRangesAdjacent(
      StringRangeSet::Range("A", kWhatever, "C", kOpen),
      StringRangeSet::Range("C", kOpen, "D", kWhatever)));
  EXPECT_TRUE(detail::DisjointAndSortedRangesAdjacent(
      StringRangeSet::Range("A", kWhatever, "C", kClosed),
      StringRangeSet::Range("C", kOpen, "D", kWhatever)));
  EXPECT_TRUE(detail::DisjointAndSortedRangesAdjacent(
      StringRangeSet::Range("A", kWhatever, "C", kOpen),
      StringRangeSet::Range("C", kClosed, "D", kWhatever)));
  EXPECT_FALSE(detail::DisjointAndSortedRangesAdjacent(
      StringRangeSet::Range("A", kWhatever, "C", kOpen),
      StringRangeSet::Range(std::string("C\0", 2), kOpen, "D", kWhatever)));
  EXPECT_FALSE(detail::DisjointAndSortedRangesAdjacent(
      StringRangeSet::Range("A", kWhatever, "C", kClosed),
      StringRangeSet::Range(std::string("C\0", 2), kOpen, "D", kWhatever)));
  EXPECT_FALSE(detail::DisjointAndSortedRangesAdjacent(
      StringRangeSet::Range("A", kWhatever, "C", kOpen),
      StringRangeSet::Range(std::string("C\0", 2), kClosed, "D", kWhatever)));
  EXPECT_TRUE(detail::DisjointAndSortedRangesAdjacent(
      StringRangeSet::Range("A", kWhatever, "C", kClosed),
      StringRangeSet::Range(std::string("C\0", 2), kClosed, "D", kWhatever)));
}

// FIXME test invalid data
TEST(TimestampRangeSet, FromInfiniteTimestampRange) {
  using testing_util::chrono_literals::operator""_ms;
  auto infinite = TimestampRangeSet::Range::FromTimestampRange(
      google::bigtable::v2::TimestampRange{});
  ASSERT_STATUS_OK(infinite);
  EXPECT_EQ(0_ms, infinite->start());
  EXPECT_EQ(0_ms, infinite->start_finite());
  EXPECT_EQ(0_ms, infinite->end());
  EXPECT_TRUE(infinite->start_closed());
  EXPECT_TRUE(infinite->end_open());
  EXPECT_FALSE(infinite->start_open());
  EXPECT_FALSE(infinite->end_closed());
}

TEST(TimestampRangeSet, FromFiniteTimestampRange) {
  using testing_util::chrono_literals::operator""_ms;
  google::bigtable::v2::TimestampRange proto;
  proto.set_start_timestamp_micros(1234);
  proto.set_end_timestamp_micros(123456789);
  auto finite = TimestampRangeSet::Range::FromTimestampRange(proto);
  ASSERT_STATUS_OK(finite);
  EXPECT_EQ(1_ms, finite->start());
  EXPECT_EQ(1_ms, finite->start_finite());
  EXPECT_EQ(123456_ms, finite->end());
  EXPECT_TRUE(finite->start_closed());
  EXPECT_TRUE(finite->end_open());
  EXPECT_FALSE(finite->start_open());
  EXPECT_FALSE(finite->end_closed());
}

TEST(TimestampRangeSet, RangeStartLess) {
  using testing_util::chrono_literals::operator""_ms;
  EXPECT_TRUE(TimestampRangeSet::Range::StartLess()(
      TimestampRangeSet::Range(3_ms, 7_ms),
      TimestampRangeSet::Range(4_ms, 7_ms)));
  EXPECT_FALSE(TimestampRangeSet::Range::StartLess()(
      TimestampRangeSet::Range(4_ms, 7_ms),
      TimestampRangeSet::Range(4_ms, 7_ms)));
  EXPECT_FALSE(TimestampRangeSet::Range::StartLess()(
      TimestampRangeSet::Range(5_ms, 7_ms),
      TimestampRangeSet::Range(4_ms, 7_ms)));
}

TEST(TimestampRangeSet, RangeEndLess) {
  using testing_util::chrono_literals::operator""_ms;
  EXPECT_TRUE(TimestampRangeSet::Range::EndLess()(
      TimestampRangeSet::Range(3_ms, 7_ms),
      TimestampRangeSet::Range(4_ms, 8_ms)));
  EXPECT_FALSE(TimestampRangeSet::Range::EndLess()(
      TimestampRangeSet::Range(4_ms, 7_ms),
      TimestampRangeSet::Range(4_ms, 7_ms)));
  EXPECT_FALSE(TimestampRangeSet::Range::EndLess()(
      TimestampRangeSet::Range(4_ms, 7_ms),
      TimestampRangeSet::Range(4_ms, 6_ms)));
  EXPECT_TRUE(TimestampRangeSet::Range::EndLess()(
      TimestampRangeSet::Range(4_ms, 7_ms),
      TimestampRangeSet::Range(4_ms, 0_ms)));
}

TEST(TimestampRangeSet, BelowStart) {
  using testing_util::chrono_literals::operator""_ms;
  EXPECT_TRUE(TimestampRangeSet::Range(3_ms, 7_ms).IsBelowStart(0_ms));
  EXPECT_TRUE(TimestampRangeSet::Range(3_ms, 7_ms).IsBelowStart(2_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 7_ms).IsBelowStart(3_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 7_ms).IsBelowStart(4_ms));
}

TEST(TimestampRangeSet, AboveEnd) {
  using testing_util::chrono_literals::operator""_ms;
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 7_ms).IsAboveEnd(0_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 7_ms).IsAboveEnd(6_ms));
  EXPECT_TRUE(TimestampRangeSet::Range(3_ms, 7_ms).IsAboveEnd(7_ms));
  EXPECT_TRUE(TimestampRangeSet::Range(3_ms, 7_ms).IsAboveEnd(8_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 0_ms).IsAboveEnd(4_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 0_ms).IsAboveEnd(0_ms));
}

TEST(TimestampRangeSet, IsWithin) {
  using testing_util::chrono_literals::operator""_ms;
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 5_ms).IsWithin(0_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 5_ms).IsWithin(2_ms));
  EXPECT_TRUE(TimestampRangeSet::Range(3_ms, 5_ms).IsWithin(3_ms));
  EXPECT_TRUE(TimestampRangeSet::Range(3_ms, 5_ms).IsWithin(4_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 5_ms).IsWithin(2_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 5_ms).IsWithin(2_ms));

  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 0_ms).IsWithin(0_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 0_ms).IsWithin(2_ms));
  EXPECT_TRUE(TimestampRangeSet::Range(3_ms, 0_ms).IsWithin(3_ms));
  EXPECT_TRUE(TimestampRangeSet::Range(3_ms, 0_ms).IsWithin(4_ms));
}

TEST(TimestampRangeSet, RangeEqality) {
  using testing_util::chrono_literals::operator""_ms;
  EXPECT_EQ(TimestampRangeSet::Range(3_ms, 5_ms),
            TimestampRangeSet::Range(3_ms, 5_ms));
  EXPECT_EQ(TimestampRangeSet::Range(3_ms, 0_ms),
            TimestampRangeSet::Range(3_ms, 0_ms));

  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 5_ms) ==
               TimestampRangeSet::Range(4_ms, 5_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 5_ms) ==
               TimestampRangeSet::Range(3_ms, 6_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 0_ms) ==
               TimestampRangeSet::Range(4_ms, 0_ms));
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 0_ms) ==
               TimestampRangeSet::Range(3_ms, 10_ms));
}

TEST(TimestampRangeSet, RangePrint) {
  using testing_util::chrono_literals::operator""_ms;
  {
    std::stringstream os;
    os << TimestampRangeSet::Range(1_ms, 3_ms);
    EXPECT_EQ("[1ms,3ms)", os.str());
  }
  {
    std::stringstream os;
    os << TimestampRangeSet::Range(1_ms, 0_ms);
    EXPECT_EQ("[1ms,inf)", os.str());
  }
}

TEST(TimestampRangeSet, IsEmpty) {
  using testing_util::chrono_literals::operator""_ms;
  EXPECT_TRUE(TimestampRangeSet::Range(3_ms, 3_ms).IsEmpty());
  EXPECT_FALSE(TimestampRangeSet::Range(3_ms, 0_ms).IsEmpty());
  EXPECT_FALSE(TimestampRangeSet::Range(0_ms, 0_ms).IsEmpty());
  EXPECT_FALSE(TimestampRangeSet::Range(1_ms, 0_ms).IsEmpty());
  EXPECT_FALSE(TimestampRangeSet::Range(1_ms, 2_ms).IsEmpty());
}

TEST(TimestampRangeSet, HasOverlap) {
  using testing_util::chrono_literals::operator""_ms;
  EXPECT_FALSE(detail::HasOverlap(TimestampRangeSet::Range(4_ms, 7_ms),
                                  TimestampRangeSet::Range(0_ms, 4_ms)));
  EXPECT_TRUE(detail::HasOverlap(TimestampRangeSet::Range(4_ms, 7_ms),
                                 TimestampRangeSet::Range(0_ms, 5_ms)));
  EXPECT_TRUE(detail::HasOverlap(TimestampRangeSet::Range(4_ms, 7_ms),
                                 TimestampRangeSet::Range(6_ms, 9_ms)));
  EXPECT_FALSE(detail::HasOverlap(TimestampRangeSet::Range(4_ms, 7_ms),
                                  TimestampRangeSet::Range(7_ms, 9_ms)));
  EXPECT_TRUE(detail::HasOverlap(TimestampRangeSet::Range(4_ms, 0_ms),
                                 TimestampRangeSet::Range(7_ms, 9_ms)));
  EXPECT_FALSE(detail::HasOverlap(TimestampRangeSet::Range(4_ms, 0_ms),
                                  TimestampRangeSet::Range(3_ms, 4_ms)));
  EXPECT_TRUE(detail::HasOverlap(TimestampRangeSet::Range(4_ms, 0_ms),
                                 TimestampRangeSet::Range(3_ms, 5_ms)));
}

TEST(TimestampRangeSet, DisjointAdjacent) {
  using testing_util::chrono_literals::operator""_ms;
  EXPECT_TRUE(detail::DisjointAndSortedRangesAdjacent(
      TimestampRangeSet::Range(0_ms, 1_ms),
      TimestampRangeSet::Range(1_ms, 2_ms)));
  EXPECT_FALSE(detail::DisjointAndSortedRangesAdjacent(
      TimestampRangeSet::Range(0_ms, 1_ms),
      TimestampRangeSet::Range(2_ms, 2_ms)));
}

TEST(StringRangeSet, SingleRange) {
  StringRangeSet srs;
  srs.Sum(StringRangeSet::Range("a", kClosed, "b", kClosed));
  ASSERT_EQ(1, srs.disjoint_ranges().size());
  ASSERT_EQ(StringRangeSet::Range("a", kClosed, "b", kClosed),
            *srs.disjoint_ranges().begin());
}

std::set<TimestampRangeSet::Range, TimestampRangeSet::Range::StartLess>
TSRanges(std::vector<std::pair<std::chrono::milliseconds,
                               std::chrono::milliseconds>> const& ranges) {
  std::set<TimestampRangeSet::Range, TimestampRangeSet::Range::StartLess> res;
  std::transform(ranges.begin(), ranges.end(), std::inserter(res, res.begin()),
                 [](std::pair<std::chrono::milliseconds,
                              std::chrono::milliseconds> const& range) {
                   return TimestampRangeSet::Range(range.first, range.second);
                 });
  return res;
}

TEST(TimestampRangeSet, ThreeDisjointIntervals) {
  using testing_util::chrono_literals::operator""_ms;
  TimestampRangeSet trs;
  trs.Sum(TimestampRangeSet::Range(1_ms, 2_ms));
  trs.Sum(TimestampRangeSet::Range(3_ms, 5_ms));
  trs.Sum(TimestampRangeSet::Range(6_ms, 8_ms));
  ASSERT_EQ(TSRanges({{1_ms, 2_ms}, {3_ms, 5_ms}, {6_ms, 8_ms}}),
            trs.disjoint_ranges());
}

TEST(TimestampRangeSet, MergingAdjacentPreceding) {
  using testing_util::chrono_literals::operator""_ms;
  TimestampRangeSet trs;
  trs.Sum(TimestampRangeSet::Range(7_ms, 8_ms));
  trs.Sum(TimestampRangeSet::Range(8_ms, 9_ms));
  ASSERT_EQ(TSRanges({{7_ms, 9_ms}}), trs.disjoint_ranges());
}

TEST(TimestampRangeSet, MergingOverlappingPreceding) {
  using testing_util::chrono_literals::operator""_ms;
  TimestampRangeSet trs;
  trs.Sum(TimestampRangeSet::Range(7_ms, 9_ms));
  trs.Sum(TimestampRangeSet::Range(8_ms, 10_ms));
  ASSERT_EQ(TSRanges({{7_ms, 10_ms}}), trs.disjoint_ranges());
}

TEST(TimestampRangeSet, RemovingOverlapping) {
  using testing_util::chrono_literals::operator""_ms;
  TimestampRangeSet trs;
  trs.Sum(TimestampRangeSet::Range(1_ms, 2_ms));
  trs.Sum(TimestampRangeSet::Range(3_ms, 4_ms));
  trs.Sum(TimestampRangeSet::Range(5_ms, 6_ms));
  trs.Sum(TimestampRangeSet::Range(7_ms, 8_ms));
  trs.Sum(TimestampRangeSet::Range(1_ms, 8_ms));
  ASSERT_EQ(TSRanges({{1_ms, 8_ms}}), trs.disjoint_ranges());
}

TEST(TimestampRangeSet, RemovingOverlappingExtendEnd) {
  using testing_util::chrono_literals::operator""_ms;
  TimestampRangeSet trs;
  trs.Sum(TimestampRangeSet::Range(1_ms, 2_ms));
  trs.Sum(TimestampRangeSet::Range(3_ms, 4_ms));
  trs.Sum(TimestampRangeSet::Range(5_ms, 6_ms));
  trs.Sum(TimestampRangeSet::Range(7_ms, 8_ms));
  trs.Sum(TimestampRangeSet::Range(1_ms, 9_ms));
  ASSERT_EQ(TSRanges({{1_ms, 9_ms}}), trs.disjoint_ranges());
}

TEST(TimestampRangeSet, RemovingOverlappingEarlyEnd) {
  using testing_util::chrono_literals::operator""_ms;
  TimestampRangeSet trs;
  trs.Sum(TimestampRangeSet::Range(1_ms, 2_ms));
  trs.Sum(TimestampRangeSet::Range(3_ms, 4_ms));
  trs.Sum(TimestampRangeSet::Range(5_ms, 6_ms));
  trs.Sum(TimestampRangeSet::Range(7_ms, 9_ms));
  trs.Sum(TimestampRangeSet::Range(1_ms, 8_ms));
  ASSERT_EQ(TSRanges({{1_ms, 9_ms}}), trs.disjoint_ranges());
}

TEST(TimestampRangeSet, PluggingGap) {
  using testing_util::chrono_literals::operator""_ms;
  TimestampRangeSet trs;
  trs.Sum(TimestampRangeSet::Range(1_ms, 2_ms));
  trs.Sum(TimestampRangeSet::Range(3_ms, 5_ms));
  ASSERT_EQ(TSRanges({{1_ms, 2_ms}, {3_ms, 5_ms}}), trs.disjoint_ranges());
  trs.Sum(TimestampRangeSet::Range(2_ms, 3_ms));
  ASSERT_EQ(TSRanges({{1_ms, 5_ms}}), trs.disjoint_ranges());
}

TEST(TimestampRangeSet, IntersectAll) {
  using testing_util::chrono_literals::operator""_ms;

  auto trs = TimestampRangeSet::All();
  trs.Intersect(TimestampRangeSet::Range(3_ms, 5_ms));
  ASSERT_EQ(TSRanges({{3_ms, 5_ms}}), trs.disjoint_ranges());
}

TEST(TimestampRangeSet, IntersectPartialShorter) {
  using testing_util::chrono_literals::operator""_ms;

  auto trs = TimestampRangeSet::Empty();
  trs.Sum(TimestampRangeSet::Range(1_ms, 4_ms));
  trs.Sum(TimestampRangeSet::Range(5_ms, 6_ms));
  trs.Sum(TimestampRangeSet::Range(7_ms, 10_ms));
  trs.Intersect(TimestampRangeSet::Range(3_ms, 8_ms));
  ASSERT_EQ(TSRanges({{3_ms, 4_ms}, {5_ms, 6_ms}, {7_ms, 8_ms}}),
            trs.disjoint_ranges());
}

TEST(TimestampRangeSet, IntersectPartialLonger) {
  using testing_util::chrono_literals::operator""_ms;

  auto trs = TimestampRangeSet::Empty();
  trs.Sum(TimestampRangeSet::Range(3_ms, 4_ms));
  trs.Sum(TimestampRangeSet::Range(5_ms, 6_ms));
  trs.Sum(TimestampRangeSet::Range(7_ms, 8_ms));
  trs.Intersect(TimestampRangeSet::Range(1_ms, 10_ms));
  ASSERT_EQ(TSRanges({{3_ms, 4_ms}, {5_ms, 6_ms}, {7_ms, 8_ms}}),
            trs.disjoint_ranges());
}

TEST(TimestampRangeSet, IntersectDistinct) {
  using testing_util::chrono_literals::operator""_ms;

  auto trs = TimestampRangeSet::Empty();
  trs.Sum(TimestampRangeSet::Range(3_ms, 4_ms));
  trs.Intersect(TimestampRangeSet::Range(7_ms, 10_ms));
  ASSERT_EQ(TSRanges({}), trs.disjoint_ranges());
}

TEST(StringRangeSet, IntersectDistinct) {
  auto srs = StringRangeSet::All();
  srs.Intersect({StringRangeSet::Range("col0", false, "col0", false)});
  srs.Intersect({StringRangeSet::Range("col2", false, "col2", false)});
  std::set<StringRangeSet::Range, StringRangeSet::Range::StartLess> empty;
  ASSERT_EQ(empty, srs.disjoint_ranges());
}

}  // anonymous namespace
}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
