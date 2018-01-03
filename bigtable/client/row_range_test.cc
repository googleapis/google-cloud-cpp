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

#include "bigtable/client/row_range.h"

#include <gmock/gmock.h>

namespace btproto = ::google::bigtable::v2;

TEST(RowRangeTest, InfiniteRange) {
  auto proto = bigtable::RowRange::InfiniteRange().as_proto();
  EXPECT_EQ(btproto::RowRange::START_KEY_NOT_SET, proto.start_key_case());
  EXPECT_EQ(btproto::RowRange::END_KEY_NOT_SET, proto.end_key_case());
}

TEST(RowRangeTest, StartingAt) {
  auto proto = bigtable::RowRange::StartingAt("foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyClosed, proto.start_key_case());
  EXPECT_EQ("foo", proto.start_key_closed());
  EXPECT_EQ(btproto::RowRange::END_KEY_NOT_SET, proto.end_key_case());
}

TEST(RowRangeTest, EndingAt) {
  auto proto = bigtable::RowRange::EndingAt("foo").as_proto();
  EXPECT_EQ(btproto::RowRange::START_KEY_NOT_SET, proto.start_key_case());
  EXPECT_EQ(btproto::RowRange::kEndKeyClosed, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_closed());
}

TEST(RowRangeTest, Range) {
  auto proto = bigtable::RowRange::Range("bar", "foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyClosed, proto.start_key_case());
  EXPECT_EQ("bar", proto.start_key_closed());
  EXPECT_EQ(btproto::RowRange::kEndKeyOpen, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_open());
}

TEST(RowRangeTest, RightOpen) {
  auto proto = bigtable::RowRange::RightOpen("bar", "foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyClosed, proto.start_key_case());
  EXPECT_EQ("bar", proto.start_key_closed());
  EXPECT_EQ(btproto::RowRange::kEndKeyOpen, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_open());
}

TEST(RowRangeTest, LeftOpen) {
  auto proto = bigtable::RowRange::LeftOpen("bar", "foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyOpen, proto.start_key_case());
  EXPECT_EQ("bar", proto.start_key_open());
  EXPECT_EQ(btproto::RowRange::kEndKeyClosed, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_closed());
}

TEST(RowRangeTest, Open) {
  auto proto = bigtable::RowRange::Open("bar", "foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyOpen, proto.start_key_case());
  EXPECT_EQ("bar", proto.start_key_open());
  EXPECT_EQ(btproto::RowRange::kEndKeyOpen, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_open());
}

TEST(RowRangeTest, Closed) {
  auto proto = bigtable::RowRange::Closed("bar", "foo").as_proto();
  EXPECT_EQ(btproto::RowRange::kStartKeyClosed, proto.start_key_case());
  EXPECT_EQ("bar", proto.start_key_closed());
  EXPECT_EQ(btproto::RowRange::kEndKeyClosed, proto.end_key_case());
  EXPECT_EQ("foo", proto.end_key_closed());
}

TEST(RowRangeTest, IsEmpty) {
  EXPECT_TRUE(bigtable::RowRange::Empty().IsEmpty());
  EXPECT_FALSE(bigtable::RowRange::InfiniteRange().IsEmpty());
  EXPECT_FALSE(bigtable::RowRange::StartingAt("bar").IsEmpty());
  EXPECT_FALSE(bigtable::RowRange::Range("bar", "foo").IsEmpty());
  EXPECT_TRUE(bigtable::RowRange::Range("foo", "foo").IsEmpty());
  EXPECT_TRUE(bigtable::RowRange::Range("foo", "bar").IsEmpty());
}

TEST(RowRangeTest, ContainsRightOpen) {
  auto range = bigtable::RowRange::RightOpen("bar", "foo");
  EXPECT_TRUE(range.Contains("bar"));
  EXPECT_FALSE(range.Contains("foo"));
  EXPECT_TRUE(range.Contains("bar-foo"));
}

TEST(RowRangeTest, ContainsLeftOpen) {
  auto range = bigtable::RowRange::LeftOpen("bar", "foo");
  EXPECT_FALSE(range.Contains("bar"));
  EXPECT_TRUE(range.Contains("foo"));
  EXPECT_TRUE(range.Contains("bar-foo"));
}

TEST(RowRangeTest, ContainsOpen) {
  auto range = bigtable::RowRange::Open("bar", "foo");
  EXPECT_FALSE(range.Contains("bar"));
  EXPECT_FALSE(range.Contains("foo"));
  EXPECT_TRUE(range.Contains("bar-foo"));
}

TEST(RowRangeTest, ContainsClosed) {
  auto range = bigtable::RowRange::Closed("bar", "foo");
  EXPECT_TRUE(range.Contains("bar"));
  EXPECT_TRUE(range.Contains("foo"));
  EXPECT_TRUE(range.Contains("bar-foo"));
}
