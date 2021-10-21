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

#include "google/cloud/bigtable/row_set.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

TEST(RowSetTest, DefaultConstructor) {
  auto proto = RowSet().as_proto();
  EXPECT_EQ(0, proto.row_keys_size());
  EXPECT_EQ(0, proto.row_ranges_size());
}

TEST(RowSetTest, AppendRange) {
  RowSet row_set;
  row_set.Append(RowRange::Range("a", "b"));
  auto proto = row_set.as_proto();
  ASSERT_EQ(1, proto.row_ranges_size());
  EXPECT_EQ("a", proto.row_ranges(0).start_key_closed());
  EXPECT_EQ("b", proto.row_ranges(0).end_key_open());

  row_set.Append(RowRange::Range("f", "k"));
  proto = row_set.as_proto();
  ASSERT_EQ(2, proto.row_ranges_size());
  EXPECT_EQ("f", proto.row_ranges(1).start_key_closed());
  EXPECT_EQ("k", proto.row_ranges(1).end_key_open());
}

TEST(RowSetTest, AppendRowKey) {
  RowSet row_set;
  row_set.Append(std::string("foo"));
  auto proto = row_set.as_proto();
  ASSERT_EQ(1, proto.row_keys_size());
  EXPECT_EQ("foo", proto.row_keys(0));

  row_set.Append("bar");
  proto = row_set.as_proto();
  ASSERT_EQ(2, proto.row_keys_size());
  EXPECT_EQ("bar", proto.row_keys(1));
}

TEST(RowSetTest, AppendMixed) {
  RowSet row_set;
  row_set.Append("foo");
  row_set.Append(RowRange::Range("a", "b"));

  auto proto = row_set.as_proto();
  ASSERT_EQ(1, proto.row_ranges_size());
  ASSERT_EQ(1, proto.row_keys_size());
}

TEST(RowSetTest, VariadicConstructor) {
  using R = RowRange;
  RowSet row_set(R::Range("a", "b"), "foo", R::LeftOpen("k", "m"), "bar");
  auto const& proto = row_set.as_proto();
  ASSERT_EQ(2, proto.row_ranges_size());
  EXPECT_EQ(R::Range("a", "b"), R(proto.row_ranges(0)));
  EXPECT_EQ(R::LeftOpen("k", "m"), R(proto.row_ranges(1)));
  ASSERT_EQ(2, proto.row_keys_size());
  EXPECT_EQ("foo", proto.row_keys(0));
  EXPECT_EQ("bar", proto.row_keys(1));

  EXPECT_TRUE(RowSet(R::Empty()).IsEmpty());
}

TEST(RowSetTest, IntersectRightOpen) {
  using R = RowRange;
  RowSet row_set(R::Range("a", "b"), "foo", R::LeftOpen("k", "m"), "zzz");

  auto proto = row_set.Intersect(R::StartingAt("l")).as_proto();
  ASSERT_EQ(1, proto.row_ranges_size());
  EXPECT_EQ(R::Closed("l", "m"), R(proto.row_ranges(0)));
  ASSERT_EQ(1, proto.row_keys_size());
  EXPECT_EQ("zzz", proto.row_keys(0));
}

TEST(RowSetTest, DefaultSetNotEmpty) {
  RowSet row_set;
  EXPECT_FALSE(row_set.IsEmpty());
}

TEST(RowSetTest, IntersectDefaultSetKeepsArgument) {
  using R = RowRange;
  auto proto = RowSet().Intersect(R::Range("a", "b")).as_proto();
  EXPECT_TRUE(proto.row_keys().empty());
  ASSERT_EQ(1, proto.row_ranges_size());
  EXPECT_EQ(R::Range("a", "b"), R(proto.row_ranges(0)));
}

TEST(RowSetTest, IntersectWithEmptyIsEmpty) {
  using R = RowRange;
  EXPECT_TRUE(RowSet().Intersect(R::Empty()).IsEmpty());
  EXPECT_TRUE(RowSet("a", R::Range("a", "b")).Intersect(R::Empty()).IsEmpty());
}

TEST(RowSetTest, IntersectWithDisjointIsEmpty) {
  using R = RowRange;
  EXPECT_TRUE(
      RowSet("a", R::Range("a", "b")).Intersect(R::Range("c", "d")).IsEmpty());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
