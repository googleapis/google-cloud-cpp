// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may
// obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/query_row.h"
#include "google/cloud/bigtable/mocks/mock_query_row.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <tuple>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::Not;

TEST(QueryRow, DefaultConstruct) {
  QueryRow row;
  EXPECT_EQ(0, row.size());
}

TEST(QueryRow, ValueSemantics) {
  QueryRow row = bigtable_mocks::MakeQueryRow(1, "blah", true);

  QueryRow copy = row;
  EXPECT_EQ(copy, row);

  copy = row;
  EXPECT_EQ(copy, row);

  QueryRow move = std::move(row);
  EXPECT_EQ(move, copy);

  row = copy;
  move = std::move(row);
  EXPECT_EQ(move, copy);
}

TEST(QueryRow, BasicAccessors) {
  auto values = std::vector<Value>{Value(1), Value("blah"), Value(true)};
  auto columns = std::vector<std::string>{"a", "b", "c"};
  QueryRow row = bigtable_mocks::MakeQueryRow({{columns[0], values[0]},
                                               {columns[1], values[1]},
                                               {columns[2], values[2]}});

  EXPECT_EQ(3, row.size());
  EXPECT_EQ(values, row.values());
  EXPECT_EQ(columns, row.columns());
  EXPECT_EQ(values, std::move(row).values());
}

TEST(QueryRow, GetByPosition) {
  QueryRow row = bigtable_mocks::MakeQueryRow(1, "blah", true);

  EXPECT_STATUS_OK(row.get(0));
  EXPECT_STATUS_OK(row.get(1));
  EXPECT_STATUS_OK(row.get(2));
  EXPECT_THAT(row.get(3), Not(IsOk()));

  EXPECT_EQ(Value(1), *row.get(0));
  EXPECT_EQ(Value("blah"), *row.get(1));
  EXPECT_EQ(Value(true), *row.get(2));
}

TEST(QueryRow, GetByColumnName) {
  QueryRow row = bigtable_mocks::MakeQueryRow(
      {{"a", Value(1)}, {"b", Value("blah")}, {"c", Value(true)}});

  EXPECT_STATUS_OK(row.get("a"));
  EXPECT_STATUS_OK(row.get("b"));
  EXPECT_STATUS_OK(row.get("c"));
  EXPECT_THAT(row.get("not a column name"), Not(IsOk()));

  EXPECT_EQ(Value(1), *row.get("a"));
  EXPECT_EQ(Value("blah"), *row.get("b"));
  EXPECT_EQ(Value(true), *row.get("c"));
}

TEST(QueryRow, TemplatedGetByPosition) {
  QueryRow row = bigtable_mocks::MakeQueryRow(1, "blah", true);

  EXPECT_STATUS_OK(row.get<std::int64_t>(0));
  EXPECT_STATUS_OK(row.get<std::string>(1));
  EXPECT_STATUS_OK(row.get<bool>(2));

  // Ensures that the wrong type specification results in a failure.
  EXPECT_THAT(row.get<bool>(0), Not(IsOk()));
  EXPECT_THAT(row.get<std::int64_t>(1), Not(IsOk()));
  EXPECT_THAT(row.get<std::string>(2), Not(IsOk()));
  EXPECT_THAT(row.get<std::int64_t>(3), Not(IsOk()));

  EXPECT_EQ(1, *row.get<std::int64_t>(0));
  EXPECT_EQ("blah", *row.get<std::string>(1));
  EXPECT_EQ(true, *row.get<bool>(2));
}

TEST(QueryRow, TemplatedGetByColumnName) {
  QueryRow row = bigtable_mocks::MakeQueryRow(
      {{"a", Value(1)}, {"b", Value("blah")}, {"c", Value(true)}});

  EXPECT_STATUS_OK(row.get<std::int64_t>("a"));
  EXPECT_STATUS_OK(row.get<std::string>("b"));
  EXPECT_STATUS_OK(row.get<bool>("c"));

  // Ensures that the wrong type specification results in a failure.
  EXPECT_THAT(row.get<bool>("a"), Not(IsOk()));
  EXPECT_THAT(row.get<std::int64_t>("b"), Not(IsOk()));
  EXPECT_THAT(row.get<std::string>("c"), Not(IsOk()));
  EXPECT_THAT(row.get<std::string>("column does not exist"), Not(IsOk()));

  EXPECT_EQ(1, *row.get<std::int64_t>("a"));
  EXPECT_EQ("blah", *row.get<std::string>("b"));
  EXPECT_EQ(true, *row.get<bool>("c"));
}

TEST(QueryRow, TemplatedGetAsTuple) {
  QueryRow row = bigtable_mocks::MakeQueryRow(1, "blah", true);

  using RowType = std::tuple<std::int64_t, std::string, bool>;
  EXPECT_STATUS_OK(row.get<RowType>());
  EXPECT_EQ(std::make_tuple(1, "blah", true), *row.get<RowType>());

  using TooFewTypes = std::tuple<std::int64_t, std::string>;
  EXPECT_THAT(row.get<TooFewTypes>(), Not(IsOk()));
  QueryRow copy = row;
  EXPECT_THAT(std::move(copy).get<TooFewTypes>(), Not(IsOk()));

  using TooManyTypes = std::tuple<std::int64_t, std::string, bool, bool>;
  EXPECT_THAT(row.get<TooManyTypes>(), Not(IsOk()));
  copy = row;
  EXPECT_THAT(std::move(copy).get<TooManyTypes>(), Not(IsOk()));

  using WrongType = std::tuple<std::int64_t, std::string, std::int64_t>;
  EXPECT_THAT(row.get<WrongType>(), Not(IsOk()));
  copy = row;
  EXPECT_THAT(std::move(copy).get<WrongType>(), Not(IsOk()));

  EXPECT_EQ(std::make_tuple(1, "blah", true), *std::move(row).get<RowType>());
}

TEST(MakeQueryRow, ExplicitColumnNames) {
  auto row = bigtable_mocks::MakeQueryRow({{"a", Value(42)}, {"b", Value(52)}});
  EXPECT_EQ(Value(42), *row.get("a"));
  EXPECT_EQ(Value(52), *row.get("b"));
}

TEST(MakeQueryRow, ImplicitColumnNames) {
  auto row = bigtable_mocks::MakeQueryRow(42, 52);
  EXPECT_EQ(Value(42), *row.get("0"));
  EXPECT_EQ(Value(52), *row.get("1"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
