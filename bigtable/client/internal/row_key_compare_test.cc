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

#include "bigtable/client/internal/row_key_compare.h"

#include <gmock/gmock.h>

TEST(RowKeyCompareTest, Simple) {
  EXPECT_EQ(0, bigtable::internal::RowKeyCompare("a", "a"));
  EXPECT_EQ(0, bigtable::internal::RowKeyCompare("abc", "abc"));
  EXPECT_EQ(1, bigtable::internal::RowKeyCompare("abcd", "abc"));
  EXPECT_EQ(1, bigtable::internal::RowKeyCompare("abd", "abc"));
  EXPECT_EQ(-1, bigtable::internal::RowKeyCompare("abc", "abcd"));
  EXPECT_EQ(-1, bigtable::internal::RowKeyCompare("abc", "abd"));
}

TEST(RowKeyCompareTest, UnsignedRange) {
  std::string xffff("\xFF\xFF", 2);
  std::string xfffe("\xFF\xFE", 2);
  std::string xffff01("\xFF\xFF\x01", 3);
  EXPECT_EQ(0, bigtable::internal::RowKeyCompare(xffff, xffff));
  EXPECT_EQ(1, bigtable::internal::RowKeyCompare(xffff, xfffe));
  EXPECT_EQ(-1, bigtable::internal::RowKeyCompare(xfffe, xffff));
  EXPECT_EQ(-1, bigtable::internal::RowKeyCompare(xffff, xffff01));
}
