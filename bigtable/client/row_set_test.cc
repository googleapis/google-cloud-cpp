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

#include "bigtable/client/row_set.h"

#include <gmock/gmock.h>

namespace btproto = ::google::bigtable::v2;

TEST(RowSetTest, Empty) {
  auto proto = bigtable::RowSet().as_proto();
  EXPECT_EQ(0, proto.row_keys_size());
  EXPECT_EQ(0, proto.row_ranges_size());
}

TEST(RowSetTest, AppendRange) {
  bigtable::RowSet row_set;
  row_set.Append(bigtable::RowRange::Range("a", "b"));
  auto proto = row_set.as_proto();
  ASSERT_EQ(1, proto.row_ranges_size());
  EXPECT_EQ("a", proto.row_ranges(0).start_key_closed());
  EXPECT_EQ("b", proto.row_ranges(0).end_key_open());

  row_set.Append(bigtable::RowRange::Range("f", "k"));
  proto = row_set.as_proto();
  ASSERT_EQ(2, proto.row_ranges_size());
  EXPECT_EQ("f", proto.row_ranges(1).start_key_closed());
  EXPECT_EQ("k", proto.row_ranges(1).end_key_open());
}

TEST(RowSetTest, AppendRowKey) {
  bigtable::RowSet row_set;
  row_set.Append(std::string("foo"));
  auto proto = row_set.as_proto();
  ASSERT_EQ(1, proto.row_keys_size());
  EXPECT_EQ("foo", proto.row_keys(0));

  absl::string_view view("bar");
  row_set.Append(view);
  proto = row_set.as_proto();
  ASSERT_EQ(2, proto.row_keys_size());
  EXPECT_EQ("bar", proto.row_keys(1));
}

TEST(RowSetTest, AppendMixed) {
  bigtable::RowSet row_set;
  row_set.Append("foo");
  row_set.Append(bigtable::RowRange::Range("a", "b"));

  auto proto = row_set.as_proto();
  ASSERT_EQ(1, proto.row_ranges_size());
  ASSERT_EQ(1, proto.row_keys_size());
}

TEST(RowSetTest, VariadicConstructor) {
  bigtable::RowSet row_set(bigtable::RowRange::Range("a", "b"), "foo",
                           bigtable::RowRange::Range("k", "m"), "bar");
  auto proto = row_set.as_proto();
  ASSERT_EQ(2, proto.row_ranges_size());
  EXPECT_EQ("a", proto.row_ranges(0).start_key_closed());
  EXPECT_EQ("b", proto.row_ranges(0).end_key_open());
  EXPECT_EQ("k", proto.row_ranges(1).start_key_closed());
  EXPECT_EQ("m", proto.row_ranges(1).end_key_open());
  ASSERT_EQ(2, proto.row_keys_size());
  EXPECT_EQ("foo", proto.row_keys(0));
  EXPECT_EQ("bar", proto.row_keys(1));
}
