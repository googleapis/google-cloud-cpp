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
#include <sstream>

namespace btproto = ::google::bigtable::v2;

/**
 * Basic test directly on the  ::google::bigtable::v2::RowRange proto.
 */
TEST(RowRangeTest, RowRangeProto) {
  {
    // Both ends not set.
    // The range should be [..NOT_SET):
    btproto::RowRange range = bigtable::RowRange::infinite_range().as_proto();
    ASSERT_EQ(btproto::RowRange::START_KEY_NOT_SET, range.start_key_case());
    ASSERT_EQ(btproto::RowRange::END_KEY_NOT_SET, range.end_key_case());
    EXPECT_EQ("", range.start_key_closed());
  }
  // Tests for as_proto.
  {  // ["foo", "fox")
    btproto::RowRange range = bigtable::RowRange("foo", "fox").as_proto();
    ASSERT_EQ(btproto::RowRange::kStartKeyClosed, range.start_key_case());
    ASSERT_EQ(btproto::RowRange::kEndKeyOpen, range.end_key_case());
    EXPECT_EQ("foo", range.start_key_closed());
    EXPECT_EQ("fox", range.end_key_open());
  }
  {
    btproto::RowRange range =
        bigtable::RowRange::left_open("foo", "fox").as_proto();
    ASSERT_EQ(btproto::RowRange::kStartKeyOpen, range.start_key_case());
    ASSERT_EQ(btproto::RowRange::kEndKeyClosed, range.end_key_case());
    EXPECT_EQ("foo", range.start_key_open());
    EXPECT_EQ("fox", range.end_key_closed());
  }
  {
    btproto::RowRange range =
        bigtable::RowRange::right_open("foo", "fox").as_proto();
    ASSERT_EQ(btproto::RowRange::kStartKeyClosed, range.start_key_case());
    ASSERT_EQ(btproto::RowRange::kEndKeyOpen, range.end_key_case());
    EXPECT_EQ("foo", range.start_key_closed());
    EXPECT_EQ("fox", range.end_key_open());
  }
  {
    btproto::RowRange range =
        bigtable::RowRange::closed_interval("foo", "fox").as_proto();
    ASSERT_EQ(btproto::RowRange::kStartKeyClosed, range.start_key_case());
    ASSERT_EQ(btproto::RowRange::kEndKeyClosed, range.end_key_case());
    EXPECT_EQ("foo", range.start_key_closed());
    EXPECT_EQ("fox", range.end_key_closed());
  }
}

/**
 * Unit tests for output redirection to ostream, and the builders for the
 * RowRange class.
 */
TEST(RowRangeTest, RedirectAndRowRanges) {
  {
    std::ostringstream oss;
    bigtable::RowRange tmp;  // Default constructor.
    oss << tmp;
    EXPECT_EQ("[..NOT_SET)", oss.str());  // Infinite range.
  }
  {
    std::ostringstream oss;
    bigtable::RowRange tmp;  // Default constructor.
    oss << tmp << "##$$^" << tmp << bigtable::RowRange("fop", "fox");
    EXPECT_EQ("[..NOT_SET)##$$^[..NOT_SET)[fop..fox)", oss.str());
  }
  {
    std::ostringstream oss;
    oss << bigtable::RowRange::range("fop", "fox");
    EXPECT_EQ("[fop..fox)", oss.str());
  }
  {  // Does not check bounds during object construction.
    std::ostringstream oss;
    oss << bigtable::RowRange::range("fox", "fop");
    EXPECT_EQ("[fox..fop)", oss.str());
  }
  {
    std::ostringstream oss;
    oss << bigtable::RowRange::open_interval("fop", "fox");
    EXPECT_EQ("(fop..fox)", oss.str());
  }
  {
    std::ostringstream oss;
    oss << bigtable::RowRange::closed_interval("fop", "fox");
    EXPECT_EQ("[fop..fox]", oss.str());
  }
  {
    std::ostringstream oss;
    oss << bigtable::RowRange::empty();
    EXPECT_EQ("[..)", oss.str());
  }
  {
    std::ostringstream oss;
    oss << bigtable::RowRange::infinite_range();
    EXPECT_EQ("[..NOT_SET)", oss.str());
  }
  {  // Semi-infinite range.
    std::ostringstream oss;
    oss << bigtable::RowRange("Foo", "");
    EXPECT_EQ("[Foo..NOT_SET)", oss.str());
  }
  {
    std::ostringstream oss;
    oss << bigtable::RowRange::left_open("foo", "fox");
    EXPECT_EQ("(foo..fox]", oss.str());
  }
  {
    std::ostringstream oss;
    oss << bigtable::RowRange::right_open("foo", "fox");
    EXPECT_EQ("[foo..fox)", oss.str());
  }
  {
    std::ostringstream oss;
    oss << bigtable::RowRange::range("foo", "fox");
    EXPECT_EQ("[foo..fox)", oss.str());
  }
}

/**
 * Test for constructing RowRanges with 'prefix_range'.
 */
TEST(RowRangeTest, PrefixRowRanges) {
  EXPECT_EQ(bigtable::RowRange::prefix_range("foo\xFF\xFF"),
            bigtable::RowRange("foo\xFF\xFF", "fop"));
  EXPECT_EQ(bigtable::RowRange::prefix_range("\xFF\xFF"),
            bigtable::RowRange("\xFF\xFF", "\xFF\xFF"));
  EXPECT_EQ(bigtable::RowRange::prefix_range({"\0\0\0\xFF", 4}),
            bigtable::RowRange({"\0\0\0\xFF", 4}, {"\0\0\1", 3}));
  EXPECT_EQ(bigtable::RowRange::prefix_range(""), bigtable::RowRange("", ""));
  EXPECT_EQ(bigtable::RowRange::prefix_range({"\0\0\0\0abc", 7}),
            bigtable::RowRange({"\0\0\0\0abc", 7}, {"\0\0\0\0abd", 7}));
  EXPECT_EQ(bigtable::RowRange::prefix_range({"\0\0\0\0\0", 5}),
            bigtable::RowRange({"\0\0\0\0\0", 5}, {"\0\0\0\0\1", 5}));
  EXPECT_EQ(bigtable::RowRange::prefix_range({"\0\0", 2}),
            bigtable::RowRange({"\0\0", 2}, {"\0\1", 2}));
  EXPECT_NE(bigtable::RowRange::prefix_range({"\0\0", 2}),
            bigtable::RowRange({"\0", 3}, {"\0\1\1", 2}));
}

TEST(RowRangeTest, EqualityRowRanges) {
  EXPECT_TRUE(bigtable::RowRange("a", "d") == bigtable::RowRange("a", "d"));
  EXPECT_FALSE(bigtable::RowRange("a", "d") != bigtable::RowRange("a", "d"));
  EXPECT_TRUE(bigtable::RowRange("d", "a") == bigtable::RowRange("d", "c"));
  EXPECT_FALSE(bigtable::RowRange("d", "a") != bigtable::RowRange("d", "a"));

  // Empty range.
  ASSERT_TRUE(bigtable::RowRange("a", "a") == bigtable::RowRange("b", "b"));
  ASSERT_TRUE(bigtable::RowRange("a", "a") == bigtable::RowRange::empty());
  // Infinite range != Empty range.
  ASSERT_TRUE(bigtable::RowRange("", "") != bigtable::RowRange("b", "b"));
  // Right-sided infinite ranges.
  ASSERT_TRUE(bigtable::RowRange("Foo", "") != bigtable::RowRange("Fox", ""));
  // Infinite ranges.
  ASSERT_TRUE(bigtable::RowRange("", "") ==
              bigtable::RowRange::infinite_range());
  ASSERT_FALSE(bigtable::RowRange("", "") !=
               bigtable::RowRange::infinite_range());
}

static std::string Intersect(std::string const& s1, std::string const& l1,
                             std::string const& s2, std::string const& l2) {
  bigtable::RowRange result = bigtable::RowRange::intersect(
      bigtable::RowRange(s1, l1), bigtable::RowRange(s2, l2));
  return result.debug_string();
}

TEST(RowRangeTest, IntersectEqual) {
  // Equal ranges.
  EXPECT_EQ("[..NOT_SET)", Intersect("", "", "", ""));
  EXPECT_EQ("[a..z)", Intersect("a", "z", "a", "z"));
  EXPECT_EQ("[a..a)", Intersect("a", "a", "a", "a"));

  // Disjoint ranges.
  EXPECT_EQ("[c..c)", Intersect("a", "b", "c", "d"));
  EXPECT_EQ("[c..c)", Intersect("c", "d", "a", "b"));
  EXPECT_EQ("[b..b)", Intersect("a", "b", "b", "c"));
  EXPECT_EQ("[b..b)", Intersect("b", "c", "a", "b"));

  // First is a subset.
  EXPECT_EQ("[j..m)", Intersect("j", "m", "a", "z"));

  // First is a superset.
  EXPECT_EQ("[j..m)", Intersect("a", "z", "j", "m"));

  // First starts before second.
  EXPECT_EQ("[j..m)", Intersect("a", "m", "j", "z"));

  // First ends after second.
  EXPECT_EQ("[j..m)", Intersect("j", "z", "a", "m"));

  // Output range is open interval: ('a', 'p').
  EXPECT_EQ(
      bigtable::RowRange::open_interval("a", "p"),
      bigtable::RowRange::intersect(bigtable::RowRange::right_open("a", "p"),
                                    bigtable::RowRange::left_open("a", "p")));

  // Output range is left open ('d', 'p'].
  EXPECT_EQ(
      bigtable::RowRange::left_open("d", "p"),
      bigtable::RowRange::intersect(bigtable::RowRange::left_open("a", "p"),
                                    bigtable::RowRange::left_open("d", "r")));

  // Output range is right open ['d', 'p').
  EXPECT_EQ(
      bigtable::RowRange::right_open("d", "p"),
      bigtable::RowRange::intersect(bigtable::RowRange::left_open("a", "p"),
                                    bigtable::RowRange::right_open("d", "p")));

  // Output range is  closed ['d', 'p'].
  EXPECT_EQ(
      bigtable::RowRange::closed_interval("d", "p"),
      bigtable::RowRange::intersect(bigtable::RowRange::left_open("a", "p"),
                                    bigtable::RowRange::right_open("d", "r")));

  // semi-infinite range ("d", "").
  EXPECT_EQ(bigtable::RowRange::open_interval("d", ""),
            bigtable::RowRange::intersect(
                bigtable::RowRange::infinite_range(),
                bigtable::RowRange::open_interval("d", "")));

  // semi-infinite range ["d", ""): 'end' is always open for
  // (semi-)infinite range.
  EXPECT_EQ(
      bigtable::RowRange::range("d", ""),
      bigtable::RowRange::intersect(bigtable::RowRange::infinite_range(),
                                    bigtable::RowRange::right_open("d", "")));
}

TEST(RowRangeTest, ContainsRowRanges) {
  // Key is within the range.
  EXPECT_TRUE(bigtable::RowRange("abc", "abd").contains(""));
  EXPECT_TRUE(bigtable::RowRange("abc", "abd").contains("abc"));
  EXPECT_TRUE(bigtable::RowRange("abc", "abd").contains("abcd"));
  EXPECT_TRUE(bigtable::RowRange("abc", "").contains("abcd"));
  EXPECT_TRUE(bigtable::RowRange("abc", "").contains("\xFF\xFF\xFF\xFF"));
  EXPECT_TRUE(bigtable::RowRange::left_open("aaa", "aad").contains("aad"));
  EXPECT_TRUE(bigtable::RowRange::right_open("aaa", "aad").contains("aaa"));
  EXPECT_TRUE(bigtable::RowRange::closed_interval("aa", "ad").contains("aa"));
  EXPECT_TRUE(bigtable::RowRange::closed_interval("aa", "ad").contains("ad"));

  // Key is not within the range.
  EXPECT_FALSE(bigtable::RowRange("aaa", "aad").contains("baa"));
  EXPECT_FALSE(bigtable::RowRange("aaa", "aad").contains("aad"));
  EXPECT_FALSE(bigtable::RowRange::open_interval("aaa", "aad").contains("aaa"));
  EXPECT_FALSE(bigtable::RowRange::open_interval("aaa", "aad").contains("aad"));
  EXPECT_FALSE(bigtable::RowRange("\xFF\xFF\xFF", "").contains("\xFF\xFF\xFE"));
}
