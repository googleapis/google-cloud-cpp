// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/sql_statement.h"
#include "google/cloud/bigtable/instance_resource.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::UnorderedPointwise;

TEST(SqlStatementTest, SqlAccessor) {
  char const* statement = "SELECT * FROM foo";
  SqlStatement stmt(statement);
  EXPECT_EQ(statement, stmt.sql());
}

TEST(SqlStatementTest, ParamsAccessor) {
  SqlStatement::ParamType params = {{"last", Parameter("Blues")},
                                    {"first", Parameter("Elwood")}};
  SqlStatement stmt("SELECT * FROM foo", params);
  EXPECT_TRUE(params == stmt.params());
}

TEST(SqlStatementTest, ParameterNames) {
  std::vector<std::string> expected = {"first", "last"};
  SqlStatement::ParamType params = {{"last", Parameter("Blues")},
                                    {"first", Parameter("Elwood")}};
  SqlStatement stmt("SELECT * FROM foo", params);
  auto results = stmt.ParameterNames();
  EXPECT_THAT(expected, UnorderedPointwise(Eq(), results));
}

TEST(SqlStatementTest, GetParameterExists) {
  auto expected = Parameter("Elwood");
  SqlStatement::ParamType params = {{"last", Parameter("Blues")},
                                    {"first", Parameter("Elwood")}};
  SqlStatement stmt("SELECT * FROM foo", params);
  auto results = stmt.GetParameter("first");
  ASSERT_STATUS_OK(results);
  EXPECT_EQ(expected, *results);
  EXPECT_TRUE(results->type().has_string_type());
}

TEST(SqlStatementTest, GetParameterNotExist) {
  SqlStatement::ParamType params = {{"last", Parameter("Blues")},
                                    {"first", Parameter("Elwood")}};
  SqlStatement stmt("SELECT * FROM foo", params);
  auto results = stmt.GetParameter("middle");
  EXPECT_THAT(results,
              StatusIs(StatusCode::kNotFound, "No such parameter: middle"));
}

TEST(SqlStatementTest, OStreamOperatorNoParams) {
  SqlStatement s1("SELECT * FROM foo;");
  std::stringstream ss;
  ss << s1;
  EXPECT_EQ(s1.sql(), ss.str());
}

TEST(SqlStatementTest, OStreamOperatorWithParams) {
  SqlStatement::ParamType params = {{"last", Parameter("Blues")},
                                    {"first", Parameter("Elwood")}};
  SqlStatement stmt("SELECT * FROM foo", params);
  std::string expected1(
      "SELECT * FROM foo\n"
      "[param]: {first=Elwood}\n"
      "[param]: {last=Blues}");
  std::string expected2(
      "SELECT * FROM foo\n"
      "[param]: {last=Blues}\n"
      "[param]: {first=Elwood}");
  std::stringstream ss;
  ss << stmt;
  EXPECT_THAT(ss.str(), AnyOf(Eq(expected1), Eq(expected2)));
}

TEST(SqlStatementTest, Equality) {
  SqlStatement::ParamType params1 = {{"last", Parameter("Blues")},
                                     {"first", Parameter("Elwood")}};
  SqlStatement::ParamType params2 = {{"last", Parameter("blues")},
                                     {"first", Parameter("elwood")}};
  SqlStatement stmt1("select * from foo", params1);
  SqlStatement stmt2("select * from foo", params1);
  SqlStatement stmt3("SELECT * from foo", params1);
  SqlStatement stmt4("select * from foo", params2);
  EXPECT_EQ(stmt1, stmt2);
  EXPECT_NE(stmt1, stmt3);
  EXPECT_NE(stmt1, stmt4);
}

TEST(SqlStatementTest, ToProtoStatementOnly) {
  InstanceResource instance(Project("test-project"), "test-instance");
  SqlStatement stmt("SELECT * FROM foo");
  auto constexpr kText =
      R"pb(query: "SELECT * FROM foo"
           instance_name: "projects/test-project/instances/test-instance"
      )pb";
  bigtable_internal::PrepareQueryProto expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kText, &expected));
  EXPECT_THAT(bigtable_internal::ToProto(std::move(stmt), instance),
              IsProtoEqual(expected));
}

TEST(SqlStatementTest, ToProtoWithParams) {
  InstanceResource instance(Project("test-project"), "test-instance");
  SqlStatement::ParamType params = {
      {"last", Parameter("Blues")},
      {"first", Parameter("Elwood")},
      {"destroyed_cars", Parameter(static_cast<std::int64_t>(103))}};

  auto constexpr kSql =
      "SELECT * FROM foo WHERE last = @last AND first = @first AND "
      "destroyed_cars >= @destroyed_cars";
  SqlStatement stmt(kSql, params);
  auto const text = std::string(R"(query: ")") + kSql + R"(")" + R"pb(
    instance_name: "projects/test-project/instances/test-instance"
    param_types {
      key: "destroyed_cars"
      value { int64_type {} }
    }
    param_types {
      key: "first"
      value { string_type {} }
    }
    param_types {
      key: "last"
      value { string_type {} }
    }
  )pb";
  bigtable_internal::PrepareQueryProto expected;
  ASSERT_TRUE(TextFormat::ParseFromString(text, &expected));
  EXPECT_THAT(bigtable_internal::ToProto(std::move(stmt), instance),
              IsProtoEqual(expected));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
