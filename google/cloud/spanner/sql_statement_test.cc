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

#include "google/cloud/spanner/sql_statement.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::UnorderedPointwise;

TEST(SqlStatementTest, SqlAccessor) {
  const char* statement = "select * from foo";
  SqlStatement stmt(statement);
  EXPECT_EQ(statement, stmt.sql());
}

TEST(SqlStatementTest, ParamsAccessor) {
  SqlStatement::ParamType params = {{"last", Value("Blues")},
                                    {"first", Value("Elwood")}};
  SqlStatement stmt("select * from foo", params);
  EXPECT_TRUE(params == stmt.params());
}

TEST(SqlStatementTest, ParameterNames) {
  std::vector<std::string> expected = {"first", "last"};
  SqlStatement::ParamType params = {{"last", Value("Blues")},
                                    {"first", Value("Elwood")}};
  SqlStatement stmt("select * from foo", params);
  auto results = stmt.ParameterNames();
  EXPECT_THAT(expected, UnorderedPointwise(Eq(), results));
}

TEST(SqlStatementTest, GetParameterExists) {
  auto expected = Value("Elwood");
  SqlStatement::ParamType params = {{"last", Value("Blues")},
                                    {"first", Value("Elwood")}};
  SqlStatement stmt("select * from foo", params);
  auto results = stmt.GetParameter("first");
  ASSERT_STATUS_OK(results);
  EXPECT_EQ(expected, *results);
  EXPECT_EQ(std::string("Elwood"), *(results->get<std::string>()));
}

TEST(SqlStatementTest, GetParameterNotExist) {
  SqlStatement::ParamType params = {{"last", Value("Blues")},
                                    {"first", Value("Elwood")}};
  SqlStatement stmt("select * from foo", params);
  auto results = stmt.GetParameter("middle");
  EXPECT_THAT(results,
              StatusIs(StatusCode::kNotFound, "No such parameter: middle"));
}

TEST(SqlStatementTest, OStreamOperatorNoParams) {
  SqlStatement s1("SELECT * FROM TABLE FOO;");
  std::stringstream ss;
  ss << s1;
  EXPECT_EQ(s1.sql(), ss.str());
}

TEST(SqlStatementTest, OStreamOperatorWithParams) {
  SqlStatement::ParamType params = {{"last", Value("Blues")},
                                    {"first", Value("Elwood")}};
  SqlStatement stmt("select * from foo", params);
  std::string expected1(
      "select * from foo\n"
      "[param]: {first=Elwood}\n"
      "[param]: {last=Blues}");
  std::string expected2(
      "select * from foo\n"
      "[param]: {last=Blues}\n"
      "[param]: {first=Elwood}");
  std::stringstream ss;
  ss << stmt;
  EXPECT_THAT(ss.str(), AnyOf(Eq(expected1), Eq(expected2)));
}

TEST(SqlStatementTest, Equality) {
  SqlStatement::ParamType params1 = {{"last", Value("Blues")},
                                     {"first", Value("Elwood")}};
  SqlStatement::ParamType params2 = {{"last", Value("blues")},
                                     {"first", Value("elwood")}};
  SqlStatement stmt1("select * from foo", params1);
  SqlStatement stmt2("select * from foo", params1);
  SqlStatement stmt3("SELECT * from foo", params1);
  SqlStatement stmt4("select * from foo", params2);
  EXPECT_EQ(stmt1, stmt2);
  EXPECT_NE(stmt1, stmt3);
  EXPECT_NE(stmt1, stmt4);
}

TEST(SqlStatementTest, ToProtoStatementOnly) {
  SqlStatement stmt("select * from foo");
  auto constexpr kText = R"pb(sql: "select * from foo")pb";
  spanner_internal::SqlStatementProto expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(spanner_internal::ToProto(std::move(stmt)),
              IsProtoEqual(expected));
}

TEST(SqlStatementTest, ToProtoWithParams) {
  SqlStatement::ParamType params = {{"last", Value("Blues")},
                                    {"first", Value("Elwood")},
                                    {"destroyed_cars", Value(103)}};

  auto constexpr kSql =
      "SELECT * FROM foo WHERE last = @last AND first = @first AND "
      "destroyed_cars >= @destroyed_cars";
  SqlStatement stmt(kSql, params);
  auto const text = std::string(R"(sql: ")") + kSql + R"(")" + R"pb(
    params: {
      fields: {
        key: "destroyed_cars",
        value: { string_value: "103" }
      }
      fields: {
        key: "first",
        value: { string_value: "Elwood" }
      }
      fields: {
        key: "last",
        value: { string_value: "Blues" }
      }
    }
    param_types: {
      key: "destroyed_cars",
      value: { code: INT64 }
    }
    param_types: {
      key: "last",
      value: { code: STRING }
    }
    param_types: {
      key: "first",
      value: { code: STRING }
    }
  )pb";
  spanner_internal::SqlStatementProto expected;
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(spanner_internal::ToProto(std::move(stmt)),
              IsProtoEqual(expected));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
