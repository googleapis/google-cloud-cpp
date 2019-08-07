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

#include "google/cloud/spanner/client.h"
#include "google/cloud/spanner/database_admin_client.h"
#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/testing/random_database_name.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

/// @test Verify the basic insert operations for transaction commits.
TEST(CommitIntegrationTest, Insert) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  auto instance_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
          .value_or("");
  ASSERT_FALSE(project_id.empty());
  ASSERT_FALSE(instance_id.empty());

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::string database_id = spanner_testing::RandomDatabaseName(generator);

  DatabaseAdminClient admin_client;
  auto database_future =
      admin_client.CreateDatabase(project_id, instance_id, database_id, {R"""(
                             CREATE TABLE Singers (
                                SingerId   INT64 NOT NULL,
                                FirstName  STRING(1024),
                                LastName   STRING(1024)
                             ) PRIMARY KEY (SingerId)
                            )"""});
  auto database = database_future.get();
  ASSERT_STATUS_OK(database);

  auto database_name = MakeDatabaseName(project_id, instance_id, database_id);
  auto conn = MakeConnection(database_name);
  Client client(std::move(conn));

  auto commit_result = client.Commit(
      MakeReadWriteTransaction(),
      {InsertMutationBuilder("Singers", {"SingerId", "FirstName", "LastName"})
           .EmplaceRow(1, "test-first-name-1", "test-last-name-1")
           .EmplaceRow(2, "test-first-name-2", "test-last-name-2")
           .Build()});
  EXPECT_STATUS_OK(commit_result);

  // TODO(#283) - Stop using SpannerStub once Client::Read() is implemented.
  ClientOptions opts;
  auto stub = internal::CreateDefaultSpannerStub(opts.credentials(),
                                                 opts.admin_endpoint());
  grpc::ClientContext session_context;
  google::spanner::v1::CreateSessionRequest session_request;
  session_request.set_database(database_name);
  auto session = stub->CreateSession(session_context, session_request);
  EXPECT_STATUS_OK(session);

  grpc::ClientContext read_context;
  google::spanner::v1::ReadRequest request;
  request.set_table("Singers");
  request.set_session(session->name());
  *request.add_columns() = "SingerId";
  *request.add_columns() = "FirstName";
  *request.add_columns() = "LastName";
  request.mutable_key_set()->set_all(true);
  auto result_set = stub->Read(read_context, request);
  EXPECT_STATUS_OK(result_set);

  using RowType = Row<std::int64_t, std::string, std::string>;
  std::vector<RowType> returned_rows;
  int row_number = 0;
  for (auto const& row : result_set.value().rows()) {
    SCOPED_TRACE("Parsing row[" + std::to_string(row_number++) + "]");
    auto parsed_row = ParseRow<std::int64_t, std::string,
                               std::string>(std::array<Value, 3>{
        internal::FromProto(result_set->metadata().row_type().fields(0).type(),
                            row.values(0)),
        internal::FromProto(result_set->metadata().row_type().fields(1).type(),
                            row.values(1)),
        internal::FromProto(result_set->metadata().row_type().fields(2).type(),
                            row.values(2))});
    EXPECT_STATUS_OK(parsed_row);
    returned_rows.emplace_back(*parsed_row);
  }

  EXPECT_THAT(returned_rows,
              ::testing::UnorderedElementsAre(
                  RowType(1, "test-first-name-1", "test-last-name-1"),
                  RowType(2, "test-first-name-2", "test-last-name-2")));

  auto drop_status =
      admin_client.DropDatabase(project_id, instance_id, database_id);
  EXPECT_STATUS_OK(drop_status);
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
