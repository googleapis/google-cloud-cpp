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

#include "google/cloud/bigtable/client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "mocks/mock_data_connection.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::bigtable::v2::PrepareQueryResponse;
using ::google::cloud::testing_util::StatusIs;

TEST(Client, PrepareQuery) {
  auto conn_mock = std::make_shared<bigtable_mocks::MockDataConnection>();
  InstanceResource instance(Project("the-project"), "the-instance");
  SqlStatement sql("SELECT * FROM the-table");
  EXPECT_CALL(*conn_mock, PrepareQuery)
      .WillOnce([](PrepareQueryParams const& params) {
        EXPECT_EQ("projects/the-project/instances/the-instance",
                  params.instance.FullName());
        EXPECT_EQ("SELECT * FROM the-table", params.sql_statement.sql());
        PreparedQuery q(CompletionQueue{}, params.instance,
                        params.sql_statement, PrepareQueryResponse{});
        return q;
      });

  Client client(std::move(conn_mock));
  auto prepared_query = client.PrepareQuery(instance, sql);
  ASSERT_STATUS_OK(prepared_query);
}

TEST(Client, AsyncPrepareQuery) {
  auto conn = MakeDataConnection();
  Client client(conn);
  InstanceResource instance(Project("test-project"), "test-instance");
  SqlStatement sql("SELECT * FROM `test-table`");
  auto prepared_query = client.AsyncPrepareQuery(instance, sql);
  EXPECT_THAT(prepared_query.get().status(),
              StatusIs(StatusCode::kUnimplemented, "not implemented"));
}

TEST(Client, ExecuteQuery) {
  auto conn = MakeDataConnection();
  Client client(conn);
  InstanceResource instance(Project("test-project"), "test-instance");
  SqlStatement sql("SELECT * FROM `test-table`");
  auto prepared_query =
      PreparedQuery(CompletionQueue{}, instance, sql, PrepareQueryResponse{});
  auto bound_query = prepared_query.BindParameters({});
  auto row_stream = client.ExecuteQuery(std::move(bound_query));
  // We expect a row stream with a single unimplemented status row while
  // this is not implemented.
  for (auto const& row : row_stream) {
    EXPECT_THAT(row.status(),
                StatusIs(StatusCode::kUnimplemented, "not implemented"));
  }
  EXPECT_EQ(1, std::distance(row_stream.begin(), row_stream.end()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
