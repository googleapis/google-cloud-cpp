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

TEST(RowRangeTest, Equal) {
  const std::string keys[] = {
    "",
    "a",
    "b",
    "c",
  };

  int size = sizeof(keys)/sizeof(*keys);

  // Test non-empty combinations (x,y where x!=y) of the keys above.
  for (int i = 0; i < size; i++) {
    for (int j = i+1; j < size; j++) {
      for (int k = 0; k < size; k++) {
        for (int l = k+1; l < size; l++) {
          bigtable::RowRange r1(keys[i], keys[j]);
          bigtable::RowRange r2(keys[k], keys[l]);

          if ((keys[i] == keys[k]) && (keys[j] == keys[l])) {
            // Ranges must be the same
            ASSERT_TRUE(  (r1 == r2)) << ": " << r1 << "/" << r2;
            ASSERT_TRUE(! (r1 != r2)) << ": " << r1 << "/" << r2;
          } else {
            // Ranges must be diffetent
            ASSERT_TRUE(! (r1 == r2)) << ": " << r1 << "/" << r2;
            ASSERT_TRUE(  (r1 != r2)) << ": " << r1 << "/" << r2;
          }
        }
      }
    }
  }
  // Empty range.
  ASSERT_TRUE(bigtable::RowRange("a", "a") == bigtable::RowRange("b", "b"));
  // Infinite range ! = Empty range.
  ASSERT_TRUE(bigtable::RowRange("", "") != bigtable::RowRange("b", "b"));
  // Infinite ranges.
  ASSERT_TRUE(bigtable::RowRange("", "") == bigtable::RowRange("", ""));
}

TEST(RowRangeTest, Prefix) {
  EXPECT_EQ(bigtable::RowRange::prefix_range("foo"),
            bigtable::RowRange("foo", "fop"));
  EXPECT_EQ(bigtable::RowRange::prefix_range(""),
            bigtable::RowRange("", ""));
  EXPECT_EQ(bigtable::RowRange::prefix_range("foo\377\377"),
            bigtable::RowRange("foo\377\377", "fop"));
  EXPECT_EQ(bigtable::RowRange::prefix_range("\377\377"),
            bigtable::RowRange("\377\377", ""));
}

TEST(RowRangeTest, RowRanges) {
  EXPECT_EQ(bigtable::RowRange("fop", "fox"),
            bigtable::RowRange::open_interval("foo", "fox"));
  EXPECT_EQ(bigtable::RowRange("foo", "foy"),
            bigtable::RowRange::closed_interval("foo", "fox"));
  EXPECT_EQ(bigtable::RowRange("fop", "foy"),
            bigtable::RowRange::left_open("foo", "fox"));
  EXPECT_EQ(bigtable::RowRange("foo", "fox"),
            bigtable::RowRange::right_open("foo", "fox"));
  EXPECT_EQ(bigtable::RowRange("foo", "fox"),
            bigtable::RowRange::range("foo", "fox"));
  EXPECT_EQ(bigtable::RowRange("foo", "foo"),
            bigtable::RowRange::empty());
}

static std::string Intersect(const std::string& s1, const std::string& l1,
                             const std::string& s2, const std::string& l2) {
  bigtable::RowRange result = bigtable::RowRange::intersect(
      bigtable::RowRange(s1, l1),
      bigtable::RowRange(s2, l2));
  return result.debug_string();
}

TEST(RowRangeTest, IntersectEqual) {
  // Equal ranges
  EXPECT_EQ("[  ..  )", Intersect("", "", "", ""));
  EXPECT_EQ("[ a .. z )", Intersect("a", "z", "a", "z"));
  EXPECT_EQ("[ a .. a )", Intersect("a", "a", "a", "a"));

  // Disjoint ranges
  EXPECT_EQ("[ c .. c )", Intersect("a", "b", "c", "d"));
  EXPECT_EQ("[ c .. c )", Intersect("c", "d", "a", "b"));
  EXPECT_EQ("[ b .. b )", Intersect("a", "b", "b", "c"));
  EXPECT_EQ("[ b .. b )", Intersect("b", "c", "a", "b"));

  // First is a subset
  EXPECT_EQ("[ j .. m )", Intersect("j", "m", "a", "z"));

  // First is a superset
  EXPECT_EQ("[ j .. m )", Intersect("a", "z", "j", "m"));

  // First starts before second
  EXPECT_EQ("[ j .. m )", Intersect("a", "m", "j", "z"));

  // First ends after second
  EXPECT_EQ("[ j .. m )", Intersect("j", "z", "a", "m"));
}
