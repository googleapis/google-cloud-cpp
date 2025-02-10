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
#include "google/cloud/testing_util/is_proto_equal.h"
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

namespace btproto = ::google::bigtable::v2;
using ::google::cloud::testing_util::IsProtoEqual;

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
  auto open = StringRangeSet::Range::FromRowRange(
      RowRange::Open("A", "B").as_proto());
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
  auto range =
      StringRangeSet::Range::FromColumnRange(google::bigtable::v2::ColumnRange{});

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
  EXPECT_TRUE(StringRangeSet::RangeValueLess()("A", "B"));
  EXPECT_FALSE(StringRangeSet::RangeValueLess()("A", "A"));
  EXPECT_FALSE(StringRangeSet::RangeValueLess()("B", "A"));
}

TEST(StringRangeSet, RangeStartLess) {
  EXPECT_TRUE(StringRangeSet::RangeStartLess()(
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever),
      StringRangeSet::Range("B", kOpen, "unimportant", kWhatever)));
  EXPECT_FALSE(StringRangeSet::RangeStartLess()(
      StringRangeSet::Range("B", kOpen, "unimportant", kWhatever),
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever)));
  EXPECT_FALSE(StringRangeSet::RangeStartLess()(
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever),
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever)));

  EXPECT_TRUE(StringRangeSet::RangeStartLess()(
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever),
      StringRangeSet::Range("B", kClosed, "unimportant", kWhatever)));
  EXPECT_FALSE(StringRangeSet::RangeStartLess()(
      StringRangeSet::Range("B", kClosed, "unimportant", kWhatever),
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever)));
  EXPECT_FALSE(StringRangeSet::RangeStartLess()(
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever),
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever)));

  EXPECT_FALSE(StringRangeSet::RangeStartLess()(
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever),
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever)));
  EXPECT_TRUE(StringRangeSet::RangeStartLess()(
      StringRangeSet::Range("A", kClosed, "unimportant", kWhatever),
      StringRangeSet::Range("A", kOpen, "unimportant", kWhatever)));
}

TEST(StringRangeSet, RangeEndLess) {
  EXPECT_TRUE(StringRangeSet::RangeEndLess()(
      StringRangeSet::Range("unimportant", kWhatever, "A", kOpen),
      StringRangeSet::Range("unimportant", kWhatever, "B", kOpen)));
  EXPECT_FALSE(StringRangeSet::RangeEndLess()(
      StringRangeSet::Range("unimportant", kWhatever, "B", kOpen),
      StringRangeSet::Range("unimportant", kWhatever, "A", kOpen)));
  EXPECT_FALSE(StringRangeSet::RangeEndLess()(
      StringRangeSet::Range("unimportant", kWhatever, "A", kOpen),
      StringRangeSet::Range("unimportant", kWhatever, "A", kOpen)));

  EXPECT_TRUE(StringRangeSet::RangeEndLess()(
      StringRangeSet::Range("unimportant", kWhatever, "A", kClosed),
      StringRangeSet::Range("unimportant", kWhatever, "B", kClosed)));
  EXPECT_FALSE(StringRangeSet::RangeEndLess()(
      StringRangeSet::Range("unimportant", kWhatever, "B", kClosed),
      StringRangeSet::Range("unimportant", kWhatever, "A", kClosed)));
  EXPECT_FALSE(StringRangeSet::RangeEndLess()(
      StringRangeSet::Range("unimportant", kWhatever, "A", kClosed),
      StringRangeSet::Range("unimportant", kWhatever, "A", kClosed)));

  EXPECT_FALSE(StringRangeSet::RangeEndLess()(
      StringRangeSet::Range("unimportant", kWhatever, "A", kClosed),
      StringRangeSet::Range("unimportant", kWhatever, "A", kOpen)));
  EXPECT_TRUE(StringRangeSet::RangeEndLess()(
      StringRangeSet::Range("unimportant", kWhatever, "A", kOpen),
      StringRangeSet::Range("unimportant", kWhatever, "A", kClosed)));
}

TEST(StringRangeSet, BelowStart) {
  StringRangeSet::Range const open("B", kOpen, "unimportant", kWhatever);
  StringRangeSet::Range const closed("B", kClosed, "unimportant", kWhatever);
  StringRangeSet::Range const infinite(StringRangeSet::Range::Infinity{}, kClosed,
                                       StringRangeSet::Range::Infinity{},
                                       kClosed);

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
  StringRangeSet::Range const open("unimportant", kWhatever, "B", kOpen);
  StringRangeSet::Range const closed("unimportant", kWhatever, "B", kClosed);
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

  EXPECT_FALSE(detail::HasOverlap(
      StringRangeSet::Range("B", kClosed, "D", kOpen),
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

TEST(StringRangeSet, SingleRange) {
  StringRangeSet srs;
  srs.Insert(StringRangeSet::Range("a", kClosed, "b", kClosed));
  ASSERT_EQ(1, srs.disjoint_ranges().size());
  ASSERT_EQ(StringRangeSet::Range("a", kClosed, "b", kClosed),
            *srs.disjoint_ranges().begin());
}

}  // anonymous namespace
}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
