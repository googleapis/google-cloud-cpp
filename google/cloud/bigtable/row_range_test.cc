// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/row_range.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

namespace btproto = ::google::bigtable::v2;

TEST(RowRangeTest, InfiniteRange) {
  auto proto = RowRange::InfiniteRange().as_proto();
  EXPECT_EQ(btproto::RowRange::START_KEY_NOT_SET, proto.start_key_case());
  EXPECT_EQ(btproto::RowRange::END_KEY_NOT_SET, proto.end_key_case());
}

TEST(RowRangeTest, StartingAt) {
  auto proto = RowRange::StartingAt("foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyClosed, proto.start_key_case());
  EXPECT_EQ("foo", proto.start_key_closed());
  EXPECT_EQ(btproto::RowRange::END_KEY_NOT_SET, proto.end_key_case());
}

TEST(RowRangeTest, EndingAt) {
  auto proto = RowRange::EndingAt("foo").as_proto();
  EXPECT_EQ(btproto::RowRange::START_KEY_NOT_SET, proto.start_key_case());
  EXPECT_EQ(btproto::RowRange::kEndKeyClosed, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_closed());
}

TEST(RowRangeTest, Range) {
  auto proto = RowRange::Range("bar", "foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyClosed, proto.start_key_case());
  EXPECT_EQ("bar", proto.start_key_closed());
  EXPECT_EQ(btproto::RowRange::kEndKeyOpen, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_open());
}

TEST(RowRangeTest, Prefix) {
  auto proto = RowRange::Prefix("bar/baz/").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyClosed, proto.start_key_case());
  EXPECT_EQ("bar/baz/", proto.start_key_closed());
  EXPECT_EQ(btproto::RowRange::kEndKeyOpen, proto.end_key_case());
  EXPECT_EQ("bar/baz0", proto.end_key_open());
}

TEST(RowRangeTest, RightOpen) {
  auto proto = RowRange::RightOpen("bar", "foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyClosed, proto.start_key_case());
  EXPECT_EQ("bar", proto.start_key_closed());
  EXPECT_EQ(btproto::RowRange::kEndKeyOpen, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_open());
}

TEST(RowRangeTest, LeftOpen) {
  auto proto = RowRange::LeftOpen("bar", "foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyOpen, proto.start_key_case());
  EXPECT_EQ("bar", proto.start_key_open());
  EXPECT_EQ(btproto::RowRange::kEndKeyClosed, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_closed());
}

TEST(RowRangeTest, Open) {
  auto proto = RowRange::Open("bar", "foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyOpen, proto.start_key_case());
  EXPECT_EQ("bar", proto.start_key_open());
  EXPECT_EQ(btproto::RowRange::kEndKeyOpen, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_open());
}

TEST(RowRangeTest, Closed) {
  auto proto = RowRange::Closed("bar", "foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyClosed, proto.start_key_case());
  EXPECT_EQ("bar", proto.start_key_closed());
  EXPECT_EQ(btproto::RowRange::kEndKeyClosed, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_closed());
}

TEST(RowRangeTest, IsEmpty) {
  EXPECT_TRUE(RowRange::Empty().IsEmpty());
  EXPECT_FALSE(RowRange::InfiniteRange().IsEmpty());
  EXPECT_FALSE(RowRange::StartingAt("bar").IsEmpty());
  EXPECT_FALSE(RowRange::Range("bar", "foo").IsEmpty());
  EXPECT_TRUE(RowRange::Range("foo", "foo").IsEmpty());
  EXPECT_TRUE(RowRange::Range("foo", "bar").IsEmpty());
  EXPECT_FALSE(RowRange::StartingAt("").IsEmpty());

  std::string const only_00 = std::string("\0", 1);
  EXPECT_FALSE(RowRange::RightOpen("", only_00).IsEmpty());
  EXPECT_TRUE(RowRange::Open("", only_00).IsEmpty());
}

TEST(RowRangeTest, ContainsRightOpen) {
  auto range = RowRange::RightOpen("bar", "foo");
  EXPECT_FALSE(range.Contains("baq"));
  EXPECT_TRUE(range.Contains("bar"));
  EXPECT_FALSE(range.Contains("foo"));
  EXPECT_FALSE(range.Contains("fop"));
  EXPECT_TRUE(range.Contains("bar-foo"));
}

TEST(RowRangeTest, ContainsLeftOpen) {
  auto range = RowRange::LeftOpen("bar", "foo");
  EXPECT_FALSE(range.Contains("baq"));
  EXPECT_FALSE(range.Contains("bar"));
  EXPECT_TRUE(range.Contains("foo"));
  EXPECT_FALSE(range.Contains("fop"));
  EXPECT_TRUE(range.Contains("bar-foo"));
}

TEST(RowRangeTest, ContainsOpen) {
  auto range = RowRange::Open("bar", "foo");
  EXPECT_FALSE(range.Contains("baq"));
  EXPECT_FALSE(range.Contains("bar"));
  EXPECT_FALSE(range.Contains("foo"));
  EXPECT_FALSE(range.Contains("fop"));
  EXPECT_TRUE(range.Contains("bar-foo"));
}

TEST(RowRangeTest, ContainsClosed) {
  auto range = RowRange::Closed("bar", "foo");
  EXPECT_FALSE(range.Contains("baq"));
  EXPECT_TRUE(range.Contains("bar"));
  EXPECT_TRUE(range.Contains("foo"));
  EXPECT_FALSE(range.Contains("fop"));
  EXPECT_TRUE(range.Contains("bar-foo"));
}

TEST(RowRangeTest, ContainsPrefix) {
  auto range = RowRange::Prefix("foo");
  EXPECT_FALSE(range.Contains("fop"));
  EXPECT_TRUE(range.Contains("foo"));
  EXPECT_TRUE(range.Contains("foo-bar"));
  EXPECT_TRUE(range.Contains("fooa"));
  EXPECT_TRUE(range.Contains("foo\xFF"));
  EXPECT_FALSE(range.Contains("fop"));
}

TEST(RowRangeTest, ContainsPrefixWithFFFF) {
  std::string many_ffs("\xFF\xFF\xFF\xFF\xFF", 5);
  auto range = RowRange::Prefix(many_ffs);
  EXPECT_FALSE(range.Contains(std::string("\xFF\xFF\xFF\xFF\xFE", 5)));
  EXPECT_TRUE(range.Contains(std::string("\xFF\xFF\xFF\xFF\xFF", 5)));
  EXPECT_TRUE(range.Contains(std::string("\xFF\xFF\xFF\xFF\xFF/")));
  EXPECT_TRUE(range.Contains(std::string("\xFF\xFF\xFF\xFF\xFF/foo/bar/baz")));
  EXPECT_FALSE(range.Contains(std::string("\x00\x00\x00\x00\x00\x01", 6)));
}

TEST(RowRangeTest, ContainsStartingAt) {
  auto range = RowRange::StartingAt("foo");
  EXPECT_FALSE(range.Contains(""));
  EXPECT_FALSE(range.Contains("fon"));
  EXPECT_TRUE(range.Contains("foo"));
  EXPECT_TRUE(range.Contains("fop"));
}

TEST(RowRangeTest, ContainsEndingAt) {
  auto range = RowRange::EndingAt("foo");
  EXPECT_TRUE(range.Contains(""));
  EXPECT_TRUE(range.Contains(std::string("\x01", 1)));
  EXPECT_TRUE(range.Contains("foo"));
  EXPECT_FALSE(range.Contains("fop"));
}

TEST(RowRangeTest, StreamingRightOpen) {
  std::ostringstream os;
  os << RowRange::RightOpen("a", "b");
  EXPECT_EQ("['a', 'b')", os.str());
}

TEST(RowRangeTest, StreamingLeftOpen) {
  std::ostringstream os;
  os << RowRange::LeftOpen("a", "b");
  EXPECT_EQ("('a', 'b']", os.str());
}

TEST(RowRangeTest, StreamingClosed) {
  std::ostringstream os;
  os << RowRange::Closed("a", "b");
  EXPECT_EQ("['a', 'b']", os.str());
}

TEST(RowRangeTest, StreamingOpen) {
  std::ostringstream os;
  os << RowRange::Open("a", "b");
  EXPECT_EQ("('a', 'b')", os.str());
}

TEST(RowRangeTest, StreamingStartingAt) {
  std::ostringstream os;
  os << RowRange::StartingAt("a");
  EXPECT_EQ("['a', '')", os.str());
}

TEST(RowRangeTest, StreamingEndingAt) {
  std::ostringstream os;
  os << RowRange::EndingAt("a");
  EXPECT_EQ("['', 'a']", os.str());
}

std::string const kA00 = std::string("a\x00", 2);
std::string const kD00 = std::string("d\x00", 2);
std::string const kC00 = std::string("c\x00", 2);
std::string const kAFFFF00 = std::string("a\xFF\xFF\x00", 4);

TEST(RowRangeTest, EqualsRightOpen) {
  using R = RowRange;
  EXPECT_EQ(R::RightOpen("a", "d"), R::RightOpen("a", "d"));
  EXPECT_NE(R::RightOpen("a", "d"), R::RightOpen("a", "c"));
  EXPECT_NE(R::RightOpen("a", "d"), R::RightOpen("b", "d"));
  EXPECT_NE(R::RightOpen("a", "d"), R::LeftOpen("a", "d"));
  EXPECT_NE(R::RightOpen("a", "d"), R::Closed("a", "d"));
  EXPECT_NE(R::RightOpen("a", "d"), R::Open("a", "d"));

  EXPECT_EQ(R::RightOpen(kA00, kD00), R::RightOpen(kA00, kD00));
  EXPECT_NE(R::RightOpen(kA00, kD00), R::RightOpen(kA00, kC00));
  EXPECT_NE(R::RightOpen(kA00, kD00), R::RightOpen("a", "d"));
  EXPECT_NE(R::RightOpen(kAFFFF00, kD00), R::RightOpen("a", kD00));
}

TEST(RowRangeTest, EqualsLeftOpen) {
  using R = RowRange;
  EXPECT_EQ(R::LeftOpen("a", "d"), R::LeftOpen("a", "d"));
  EXPECT_NE(R::LeftOpen("a", "d"), R::LeftOpen("a", "c"));
  EXPECT_NE(R::LeftOpen("a", "d"), R::LeftOpen("b", "d"));
  EXPECT_NE(R::LeftOpen("a", "d"), R::RightOpen("a", "d"));
  EXPECT_NE(R::LeftOpen("a", "d"), R::Closed("a", "d"));
  EXPECT_NE(R::LeftOpen("a", "d"), R::Open("a", "d"));

  EXPECT_EQ(R::LeftOpen(kA00, kD00), R::LeftOpen(kA00, kD00));
  EXPECT_NE(R::LeftOpen(kA00, kD00), R::LeftOpen(kA00, kC00));
  EXPECT_NE(R::LeftOpen(kA00, kD00), R::LeftOpen("a", "d"));
  EXPECT_NE(R::LeftOpen(kAFFFF00, kD00), R::LeftOpen("a", kD00));
}

TEST(RowRangeTest, EqualsClosed) {
  using R = RowRange;
  EXPECT_EQ(R::Closed("a", "d"), R::Closed("a", "d"));
  EXPECT_NE(R::Closed("a", "d"), R::Closed("a", "c"));
  EXPECT_NE(R::Closed("a", "d"), R::Closed("b", "d"));
  EXPECT_NE(R::Closed("a", "d"), R::RightOpen("a", "d"));
  EXPECT_NE(R::Closed("a", "d"), R::LeftOpen("a", "d"));
  EXPECT_NE(R::Closed("a", "d"), R::Open("a", "d"));

  EXPECT_EQ(R::Closed(kA00, kD00), R::Closed(kA00, kD00));
  EXPECT_NE(R::Closed(kA00, kD00), R::Closed(kA00, kC00));
  EXPECT_NE(R::Closed(kA00, kD00), R::Closed("a", "d"));
  EXPECT_NE(R::Closed(kAFFFF00, kD00), R::Closed("a", kD00));
}

TEST(RowRangeTest, EqualsOpen) {
  using R = RowRange;
  EXPECT_EQ(R::Open("a", "d"), R::Open("a", "d"));
  EXPECT_NE(R::Open("a", "d"), R::Open("a", "c"));
  EXPECT_NE(R::Open("a", "d"), R::Open("b", "d"));
  EXPECT_NE(R::Open("a", "d"), R::Closed("a", "d"));
  EXPECT_NE(R::Open("a", "d"), R::LeftOpen("a", "d"));
  EXPECT_NE(R::Open("a", "d"), R::Closed("a", "d"));

  EXPECT_EQ(R::Open(kA00, kD00), R::Open(kA00, kD00));
  EXPECT_NE(R::Open(kA00, kD00), R::Open(kA00, kC00));
  EXPECT_NE(R::Open(kA00, kD00), R::Open("a", "d"));
  EXPECT_NE(R::Open(kAFFFF00, kD00), R::Open("a", kD00));
}

TEST(RowRangeTest, EqualsStartingAt) {
  using R = RowRange;
  EXPECT_EQ(R::StartingAt("a"), R::StartingAt("a"));
  EXPECT_EQ(R::StartingAt("a"), R::RightOpen("a", ""));
  EXPECT_NE(R::StartingAt("a"), R::StartingAt("b"));
  EXPECT_NE(R::StartingAt("a"), R::RightOpen("a", "d"));
  EXPECT_NE(R::StartingAt("a"), R::LeftOpen("a", "d"));
  EXPECT_NE(R::StartingAt("a"), R::Open("a", "d"));
  EXPECT_NE(R::StartingAt("a"), R::Closed("a", "d"));

  EXPECT_EQ(R::StartingAt(kA00), R::StartingAt(kA00));
  EXPECT_NE(R::StartingAt(kA00), R::StartingAt("a"));
  EXPECT_NE(R::StartingAt(kA00), R::StartingAt(kAFFFF00));
}

TEST(RowRangeTest, EqualsEndingAt) {
  using R = RowRange;
  EXPECT_EQ(R::EndingAt("b"), R::EndingAt("b"));
  EXPECT_NE(R::EndingAt("b"), R::Closed("", "b"));
  EXPECT_NE(R::EndingAt("b"), R::EndingAt("a"));
  EXPECT_NE(R::EndingAt("b"), R::RightOpen("a", "b"));
  EXPECT_NE(R::EndingAt("b"), R::LeftOpen("a", "b"));
  EXPECT_NE(R::EndingAt("b"), R::Open("a", "b"));
  EXPECT_NE(R::EndingAt("b"), R::Closed("a", "b"));

  EXPECT_EQ(R::EndingAt(kA00), R::EndingAt(kA00));
  EXPECT_NE(R::EndingAt(kA00), R::EndingAt("a"));
  EXPECT_NE(R::EndingAt(kA00), R::EndingAt(kAFFFF00));
}

// This is a fairly exhausting (and maybe exhaustive) set of cases for
// intersecting a RightOpen range against other ranges.
using R = RowRange;

TEST(RowRangeTest, IntersectRightOpenEmpty) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::Empty());
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenCompletelyBelow) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::RightOpen("a", "b"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenMatchingBoundariesBelow) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::RightOpen("a", "c"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenCompletelyAbove) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::RightOpen("n", "q"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenMatchingBoundariesAbove) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::RightOpen("m", "q"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenStartBelowEndInside) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::RightOpen("a", "d"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::RightOpen("c", "d"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenStartBelowEndInsideClosed) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::LeftOpen("a", "d"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("c", "d"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenCompletelyInsideRightOpen) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::RightOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::RightOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenCompletelyInsideLeftOpen) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::LeftOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenCompletelyInsideOpen) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::Open("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Open("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenCompletelyInsideClosed) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::Closed("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenStartInsideEndAbove) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::RightOpen("k", "z"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::RightOpen("k", "m"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenStartInsideEndAboveOpen) {
  auto tuple = R::RightOpen("c", "m").Intersect(R::LeftOpen("k", "z"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Open("k", "m"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectRightOpenNonAsciiEndpoints) {
  auto tuple = R::RightOpen(kA00, kD00).Intersect(R::LeftOpen(kAFFFF00, kC00));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen(kAFFFF00, kC00), std::get<1>(tuple));
}

// The cases for a LeftOpen interval.
TEST(RowRangeTest, IntersectLeftOpenEmpty) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::Empty());
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenCompletelyBelow) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::LeftOpen("a", "b"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenMatchingBoundariesBelow) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::RightOpen("a", "c"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenCompletelyAbove) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::LeftOpen("n", "q"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenMatchingBoundariesAbove) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::LeftOpen("m", "q"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenStartBelowEndInside) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::RightOpen("a", "d"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Open("c", "d"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenStartBelowEndInsideClosed) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::LeftOpen("a", "d"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen("c", "d"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenCompletelyInsideRightOpen) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::RightOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::RightOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenCompletelyInsideLeftOpen) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::LeftOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenCompletelyInsideOpen) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::Open("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Open("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenCompletelyInsideClosed) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::Closed("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenStartInsideEndAbove) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::RightOpen("k", "z"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("k", "m"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectLeftOpenStartInsideEndAboveOpen) {
  auto tuple = R::LeftOpen("c", "m").Intersect(R::LeftOpen("k", "z"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen("k", "m"), std::get<1>(tuple));
}

// The cases for a Open interval.
TEST(RowRangeTest, IntersectOpenEmpty) {
  auto tuple = R::Open("c", "m").Intersect(R::Empty());
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectOpenCompletelyBelow) {
  auto tuple = R::Open("c", "m").Intersect(R::RightOpen("a", "b"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectOpenMatchingBoundariesBelow) {
  auto tuple = R::Open("c", "m").Intersect(R::RightOpen("a", "c"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectOpenCompletelyAbove) {
  auto tuple = R::Open("c", "m").Intersect(R::RightOpen("n", "q"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectOpenMatchingBoundariesAbove) {
  auto tuple = R::Open("c", "m").Intersect(R::RightOpen("m", "q"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectOpenStartBelowEndInside) {
  auto tuple = R::Open("c", "m").Intersect(R::RightOpen("a", "d"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Open("c", "d"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectOpenStartBelowEndInsideClosed) {
  auto tuple = R::Open("c", "m").Intersect(R::LeftOpen("a", "d"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen("c", "d"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectOpenCompletelyInsideRightOpen) {
  auto tuple = R::Open("c", "m").Intersect(R::RightOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::RightOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectOpenCompletelyInsideLeftOpen) {
  auto tuple = R::Open("c", "m").Intersect(R::LeftOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectOpenCompletelyInsideOpen) {
  auto tuple = R::Open("c", "m").Intersect(R::Open("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Open("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectOpenCompletelyInsideClosed) {
  auto tuple = R::Open("c", "m").Intersect(R::Closed("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectOpenStartInsideEndAbove) {
  auto tuple = R::Open("c", "m").Intersect(R::RightOpen("k", "z"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::RightOpen("k", "m"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectOpenStartInsideEndAboveOpen) {
  auto tuple = R::Open("c", "m").Intersect(R::LeftOpen("k", "z"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Open("k", "m"), std::get<1>(tuple));
}

// The cases for a Closed interval.
TEST(RowRangeTest, IntersectClosedEmpty) {
  auto tuple = R::Closed("c", "m").Intersect(R::Empty());
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectClosedCompletelyBelow) {
  auto tuple = R::Closed("c", "m").Intersect(R::RightOpen("a", "b"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectClosedMatchingBoundariesBelow) {
  auto tuple = R::Closed("c", "m").Intersect(R::RightOpen("a", "c"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectClosedCompletelyAbove) {
  auto tuple = R::Closed("c", "m").Intersect(R::RightOpen("n", "q"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectClosedMatchingBoundariesAbove) {
  auto tuple = R::Closed("c", "m").Intersect(R::LeftOpen("m", "q"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectClosedStartBelowEndInside) {
  auto tuple = R::Closed("c", "m").Intersect(R::RightOpen("a", "d"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::RightOpen("c", "d"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectClosedStartBelowEndInsideClosed) {
  auto tuple = R::Closed("c", "m").Intersect(R::LeftOpen("a", "d"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("c", "d"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectClosedCompletelyInsideRightOpen) {
  auto tuple = R::Closed("c", "m").Intersect(R::RightOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::RightOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectClosedCompletelyInsideLeftOpen) {
  auto tuple = R::Closed("c", "m").Intersect(R::LeftOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectClosedCompletelyInsideOpen) {
  auto tuple = R::Closed("c", "m").Intersect(R::Open("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Open("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectClosedCompletelyInsideClosed) {
  auto tuple = R::Closed("c", "m").Intersect(R::Closed("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectClosedStartInsideEndAbove) {
  auto tuple = R::Closed("c", "m").Intersect(R::RightOpen("k", "z"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("k", "m"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectClosedStartInsideEndAboveOpen) {
  auto tuple = R::Closed("c", "m").Intersect(R::LeftOpen("k", "z"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen("k", "m"), std::get<1>(tuple));
}

// The cases for a StartingAt interval.
TEST(RowRangeTest, IntersectStartingAtEmpty) {
  auto tuple = R::StartingAt("c").Intersect(R::Empty());
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectStartingAtCompletelyBelow) {
  auto tuple = R::StartingAt("c").Intersect(R::RightOpen("a", "b"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectStartingAtMatchingBoundariesBelow) {
  auto tuple = R::StartingAt("c").Intersect(R::RightOpen("a", "c"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectStartingAtStartBelowEndInside) {
  auto tuple = R::StartingAt("c").Intersect(R::RightOpen("a", "d"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::RightOpen("c", "d"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectStartingAtStartBelowEndInsideClosed) {
  auto tuple = R::StartingAt("c").Intersect(R::LeftOpen("a", "d"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("c", "d"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectStartingAtCompletelyInsideRightOpen) {
  auto tuple = R::StartingAt("c").Intersect(R::RightOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::RightOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectStartingAtCompletelyInsideLeftOpen) {
  auto tuple = R::StartingAt("c").Intersect(R::LeftOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectStartingAtCompletelyInsideOpen) {
  auto tuple = R::StartingAt("c").Intersect(R::Open("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Open("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectStartingAtCompletelyInsideClosed) {
  auto tuple = R::StartingAt("c").Intersect(R::Closed("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectStartingAtStartInsideEndAbove) {
  auto tuple = R::StartingAt("c").Intersect(R::StartingAt("k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::StartingAt("k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectStartingAtStartInsideEndAboveOpen) {
  auto tuple = R::StartingAt("c").Intersect(R::LeftOpen("k", ""));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Open("k", ""), std::get<1>(tuple));
}

// The cases for a EndingAt interval.
TEST(RowRangeTest, IntersectEndingAtEmpty) {
  auto tuple = R::EndingAt("m").Intersect(R::Empty());
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectEndingAtCompletelyAbove) {
  auto tuple = R::EndingAt("m").Intersect(R::RightOpen("n", "q"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectEndingAtMatchingBoundariesAbove) {
  auto tuple = R::EndingAt("m").Intersect(R::LeftOpen("m", "q"));
  EXPECT_FALSE(std::get<0>(tuple));
}

TEST(RowRangeTest, IntersectEndingAtCompletelyInsideRightOpen) {
  auto tuple = R::EndingAt("m").Intersect(R::RightOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::RightOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectEndingAtCompletelyInsideLeftOpen) {
  auto tuple = R::EndingAt("m").Intersect(R::LeftOpen("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectEndingAtCompletelyInsideOpen) {
  auto tuple = R::EndingAt("m").Intersect(R::Open("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Open("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectEndingAtCompletelyInsideClosed) {
  auto tuple = R::EndingAt("m").Intersect(R::Closed("d", "k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("d", "k"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectEndingAtStartInsideEndAbove) {
  auto tuple = R::EndingAt("m").Intersect(R::RightOpen("k", "z"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::Closed("k", "m"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectEndingAtStartInsideEndAboveOpen) {
  auto tuple = R::EndingAt("m").Intersect(R::LeftOpen("k", "z"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::LeftOpen("k", "m"), std::get<1>(tuple));
}

TEST(RowRangeTest, IntersectEndingAtEndingAt) {
  auto tuple = R::EndingAt("m").Intersect(R::EndingAt("k"));
  EXPECT_TRUE(std::get<0>(tuple));
  EXPECT_EQ(R::EndingAt("k"), std::get<1>(tuple));
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
