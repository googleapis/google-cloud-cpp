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
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <utility>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(Row, DefaultConstruct) {
  Row row;
  EXPECT_EQ(0, row.size());
}

TEST(Row, ValueSemantics) {
  Row row = MakeRow({
      {"a", Value(1)},       //
      {"b", Value("blah")},  //
      {"c", Value(true)}     //
  });

  Row copy = row;
  EXPECT_EQ(copy, row);

  copy = row;
  EXPECT_EQ(copy, row);

  Row move = std::move(row);
  EXPECT_EQ(move, copy);

  row = copy;
  move = std::move(row);
  EXPECT_EQ(move, copy);
}

TEST(Row, BasicAccessors) {
  auto values = std::vector<Value>{Value(1), Value("blah"), Value(true)};
  auto columns = std::vector<std::string>{"a", "b", "c"};
  Row row = MakeRow({
      {columns[0], values[0]},  //
      {columns[1], values[1]},  //
      {columns[2], values[2]}   //
  });

  EXPECT_EQ(3, row.size());
  EXPECT_EQ(values, row.values());
  EXPECT_EQ(columns, row.columns());
  EXPECT_EQ(values, std::move(row).values());
}

TEST(Row, GetByPosition) {
  Row row = MakeRow({
      {"a", Value(1)},       //
      {"b", Value("blah")},  //
      {"c", Value(true)}     //
  });

  EXPECT_STATUS_OK(row.get(0));
  EXPECT_STATUS_OK(row.get(1));
  EXPECT_STATUS_OK(row.get(2));
  EXPECT_FALSE(row.get(3).ok());

  EXPECT_EQ(Value(1), *row.get(0));
  EXPECT_EQ(Value("blah"), *row.get(1));
  EXPECT_EQ(Value(true), *row.get(2));
}

TEST(Row, GetByColumnName) {
  Row row = MakeRow({
      {"a", Value(1)},       //
      {"b", Value("blah")},  //
      {"c", Value(true)}     //
  });

  EXPECT_STATUS_OK(row.get("a"));
  EXPECT_STATUS_OK(row.get("b"));
  EXPECT_STATUS_OK(row.get("c"));
  EXPECT_FALSE(row.get("not a column name").ok());

  EXPECT_EQ(Value(1), *row.get("a"));
  EXPECT_EQ(Value("blah"), *row.get("b"));
  EXPECT_EQ(Value(true), *row.get("c"));
}

TEST(Row, TemplatedGetByPosition) {
  Row row = MakeRow({
      {"a", Value(1)},       //
      {"b", Value("blah")},  //
      {"c", Value(true)}     //
  });

  EXPECT_STATUS_OK(row.get<std::int64_t>(0));
  EXPECT_STATUS_OK(row.get<std::string>(1));
  EXPECT_STATUS_OK(row.get<bool>(2));

  // Ensures that the wrong type specification results in a failure.
  EXPECT_FALSE(row.get<bool>(0).ok());
  EXPECT_FALSE(row.get<std::int64_t>(1).ok());
  EXPECT_FALSE(row.get<std::string>(2).ok());
  EXPECT_FALSE(row.get<std::int64_t>(3).ok());

  EXPECT_EQ(1, *row.get<std::int64_t>(0));
  EXPECT_EQ("blah", *row.get<std::string>(1));
  EXPECT_EQ(true, *row.get<bool>(2));
}

TEST(Row, TemplatedGetByColumnName) {
  Row row = MakeRow({
      {"a", Value(1)},       //
      {"b", Value("blah")},  //
      {"c", Value(true)}     //
  });

  EXPECT_STATUS_OK(row.get<std::int64_t>("a"));
  EXPECT_STATUS_OK(row.get<std::string>("b"));
  EXPECT_STATUS_OK(row.get<bool>("c"));

  // Ensures that the wrong type specification results in a failure.
  EXPECT_FALSE(row.get<bool>("a").ok());
  EXPECT_FALSE(row.get<std::int64_t>("b").ok());
  EXPECT_FALSE(row.get<std::string>("c").ok());
  EXPECT_FALSE(row.get<std::string>("column does not exist").ok());

  EXPECT_EQ(1, *row.get<std::int64_t>("a"));
  EXPECT_EQ("blah", *row.get<std::string>("b"));
  EXPECT_EQ(true, *row.get<bool>("c"));
}

TEST(Row, TemplatedGetAsTuple) {
  Row row = MakeRow({
      {"a", Value(1)},       //
      {"b", Value("blah")},  //
      {"c", Value(true)}     //
  });

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  EXPECT_STATUS_OK(row.get<RowType>());
  EXPECT_EQ(std::make_tuple(1, "blah", true), *row.get<RowType>());

  using TooFewTypes = std::tuple<std::int64_t, std::string>;
  EXPECT_FALSE(row.get<TooFewTypes>().ok());
  Row copy = row;
  EXPECT_FALSE(std::move(copy).get<TooFewTypes>().ok());

  using TooManyTypes = std::tuple<std::int64_t, std::string, bool, bool>;
  EXPECT_FALSE(row.get<TooManyTypes>().ok());
  copy = row;
  EXPECT_FALSE(std::move(copy).get<TooManyTypes>().ok());

  using WrongType = std::tuple<std::int64_t, std::string, std::int64_t>;
  EXPECT_FALSE(row.get<WrongType>().ok());
  copy = row;
  EXPECT_FALSE(std::move(copy).get<WrongType>().ok());

  EXPECT_EQ(std::make_tuple(1, "blah", true), *std::move(row).get<RowType>());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
