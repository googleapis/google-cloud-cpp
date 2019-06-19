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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

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

TEST(SqlStatementTest, OStreamOperatorNoParams) {
  SqlStatement s1("SELECT * FROM TABLE FOO;");
  std::stringstream ss;
  ss << s1;
  EXPECT_EQ(s1.sql(), ss.str());
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
