// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/row.h"
#include <gmock/gmock.h>
#include <array>
#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {
template <typename... Ts>
void VerifyRegularType(Ts&&... ts) {
  auto const row = MakeRow(std::forward<Ts>(ts)...);
  EXPECT_EQ(row, row);

  auto copy_implicit = row;
  EXPECT_EQ(copy_implicit, row);

  auto copy_explicit(copy_implicit);
  EXPECT_EQ(copy_explicit, copy_implicit);

  using RowType = typename std::decay<decltype(row)>::type;
  RowType assign;  // Default construction.
  assign = row;
  EXPECT_EQ(assign, row);

  auto const moved = std::move(assign);
  EXPECT_EQ(moved, row);
}

TEST(Row, RegularType) {
  VerifyRegularType();
  VerifyRegularType(true);
  VerifyRegularType(true, std::int64_t{42});
}

TEST(Row, ZeroTypes) {
  Row<> const row;
  EXPECT_EQ(0, row.size());
  EXPECT_EQ(std::tuple<>{}, row.get());
}

TEST(Row, OneType) {
  Row<bool> row;
  EXPECT_EQ(1, row.size());
  row = Row<bool>{true};
  EXPECT_EQ(true, row.get<0>());
  EXPECT_EQ(std::make_tuple(true), row.get());
}

TEST(Row, TwoTypes) {
  Row<bool, std::int64_t> const row(true, 42);
  EXPECT_EQ(2, row.size());
  EXPECT_EQ(true, row.get<0>());
  EXPECT_EQ(42, row.get<1>());
  EXPECT_EQ(std::make_tuple(true, std::int64_t{42}), row.get());
  EXPECT_EQ(std::make_tuple(true, std::int64_t{42}), (row.get<0, 1>()));
  EXPECT_EQ(std::make_tuple(std::int64_t{42}, true), (row.get<1, 0>()));
}

TEST(Row, ThreeTypes) {
  Row<bool, std::int64_t, std::string> const row(true, 42, "hello");
  EXPECT_EQ(3, row.size());
  EXPECT_EQ(true, row.get<0>());
  EXPECT_EQ(42, row.get<1>());
  EXPECT_EQ(std::make_tuple(true, std::int64_t{42}, std::string("hello")),
            row.get());
  EXPECT_EQ(std::make_tuple(true, std::int64_t{42}, std::string("hello")),
            (row.get<0, 1, 2>()));
  EXPECT_EQ(std::make_tuple(true, std::int64_t{42}), (row.get<0, 1>()));
  EXPECT_EQ(std::int64_t{42}, (row.get<1>()));
}

TEST(Row, Equality) {
  EXPECT_EQ(MakeRow(), MakeRow());
  EXPECT_EQ(MakeRow(true, 42), MakeRow(true, 42));
  EXPECT_NE(MakeRow(true, 42), MakeRow(false, 42));
  EXPECT_NE(MakeRow(true, 42), MakeRow(true, 99));
}

TEST(Row, Relational) {
  EXPECT_LE(MakeRow(), MakeRow());
  EXPECT_GE(MakeRow(), MakeRow());
  EXPECT_LT(MakeRow(10), MakeRow(20));
  EXPECT_GT(MakeRow(20), MakeRow(10));
  EXPECT_LT(MakeRow(false, 10), MakeRow(true, 20));
  EXPECT_GT(MakeRow("abc"), MakeRow("ab"));
}

TEST(Row, MoveFromNonConstGet) {
  // This test relies on common, but unspecified behavior of std::string.
  // Specifically this test creates a string that is bigger than the SSO so it
  // will likely be heap allocated, and it "moves" that string, which will
  // likely leave the original string empty. This is all valid code (it's not
  // UB), but it's also not specified that a string *must* leave the moved-from
  // string in an "empty" state (any valid state would be OK). So if this test
  // becomes flaky, we may want to disable it. But until then, it's probably
  // worth having.
  std::string const long_string = "12345678901234567890";
  auto row = MakeRow(long_string, long_string, long_string);
  auto const copy = row;
  EXPECT_EQ(row, copy);

  std::string col0 = std::move(row.get<0>());
  EXPECT_EQ(col0, long_string);

  EXPECT_NE(row.get<0>(), long_string);  // Unspecified behvaior
  EXPECT_NE(row, copy);  // The two original Rows are no longer equal
}

TEST(Row, SetUsingNonConstGet) {
  std::string const data = "data";
  auto row = MakeRow(data, data, data);
  auto const copy = row;
  EXPECT_EQ(row, copy);

  // "Sets" the value at column 0.
  row.get<0>() = "hello";
  EXPECT_NE(row, copy);

  EXPECT_EQ("hello", row.get<0>());
}

Row<bool, std::string> F() { return MakeRow(true, "hello"); }

TEST(Row, RvalueGet) {
  EXPECT_TRUE(F().get<0>());
  EXPECT_EQ("hello", F().get<1>());
}

TEST(Row, GetAllRefOverloads) {
  // Note: This test relies on unspecified moved-from behavior of std::string.
  // See the comment in "MoveFromNonConstGet" for more details.
  std::string const long_string = "12345678901234567890";
  auto const row_const = MakeRow(long_string, long_string);
  auto row_mut = row_const;
  EXPECT_EQ(row_const, row_mut);

  auto tup_const = row_const.get();
  auto tup_moved = std::move(row_mut).get();
  EXPECT_EQ(tup_const, tup_moved);
}

TEST(Row, MakeRowTypePromotion) {
  auto row = MakeRow(true, 42, "hello");
  static_assert(
      std::is_same<Row<bool, std::int64_t, std::string>, decltype(row)>::value,
      "Promotes int -> std::int64_t and char const* -> std::string");
}

TEST(Row, MakeRowCVQualifications) {
  std::string const s = "hello";
  static_assert(std::is_same<std::string const, decltype(s)>::value,
                "s is an lvalue const string");
  auto row = MakeRow(s);
  static_assert(std::is_same<Row<std::string>, decltype(row)>::value,
                "row holds a non-const string value");
}

TEST(Row, ParseRowEmpty) {
  std::array<Value, 0> const array = {};
  auto const row = ParseRow(array);
  EXPECT_TRUE(row.ok());
  EXPECT_EQ(MakeRow(), *row);
}

TEST(Row, ParseRowOneValue) {
  std::array<Value, 1> const array = {Value(42)};
  auto const row = ParseRow<std::int64_t>(array);
  EXPECT_TRUE(row.ok());
  EXPECT_EQ(MakeRow(42), *row);
  // Tests parsing the Value with the wrong type.
  auto const error_row = ParseRow<double>(array);
  EXPECT_FALSE(error_row.ok());
}

TEST(Row, ParseRowThree) {
  std::array<Value, 3> array = {Value(true), Value(42), Value("hello")};
  auto row = ParseRow<bool, std::int64_t, std::string>(array);
  EXPECT_TRUE(row.ok());
  EXPECT_EQ(MakeRow(true, 42, "hello"), *row);
  // Tests parsing the Value with the wrong type.
  auto const error_row = ParseRow<bool, double, std::string>(array);
  EXPECT_FALSE(error_row.ok());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
