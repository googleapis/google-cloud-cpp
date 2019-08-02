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
#include "google/cloud/spanner/internal/spanner_stub.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

namespace spanner_proto = ::google::spanner::v1;

// gmock makes clang-tidy very angry, disable a few warnings that we have no
// control over.
// NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall)
class MockSpannerStub : public internal::SpannerStub {
 public:
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(CreateSession, StatusOr<spanner_proto::Session>(
                                  grpc::ClientContext&,
                                  spanner_proto::CreateSessionRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(GetSession, StatusOr<spanner_proto::Session>(
                               grpc::ClientContext&,
                               spanner_proto::GetSessionRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(ListSessions, StatusOr<spanner_proto::ListSessionsResponse>(
                                 grpc::ClientContext&,
                                 spanner_proto::ListSessionsRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(DeleteSession,
               Status(grpc::ClientContext&,
                      spanner_proto::DeleteSessionRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(ExecuteSql, StatusOr<spanner_proto::ResultSet>(
                               grpc::ClientContext&,
                               spanner_proto::ExecuteSqlRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(
      ExecuteStreamingSql,
      std::unique_ptr<
          grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>(
          grpc::ClientContext&, spanner_proto::ExecuteSqlRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(ExecuteBatchDml,
               StatusOr<spanner_proto::ExecuteBatchDmlResponse>(
                   grpc::ClientContext&,
                   spanner_proto::ExecuteBatchDmlRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(Read,
               StatusOr<spanner_proto::ResultSet>(
                   grpc::ClientContext&, spanner_proto::ReadRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(
      StreamingRead,
      std::unique_ptr<
          grpc::ClientReaderInterface<spanner_proto::PartialResultSet>>(
          grpc::ClientContext&, spanner_proto::ReadRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(BeginTransaction,
               StatusOr<spanner_proto::Transaction>(
                   grpc::ClientContext&,
                   spanner_proto::BeginTransactionRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(Commit,
               StatusOr<spanner_proto::CommitResponse>(
                   grpc::ClientContext&, spanner_proto::CommitRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(Rollback, Status(grpc::ClientContext&,
                                spanner_proto::RollbackRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(PartitionQuery,
               StatusOr<spanner_proto::PartitionResponse>(
                   grpc::ClientContext&,
                   spanner_proto::PartitionQueryRequest const&));

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  MOCK_METHOD2(PartitionRead, StatusOr<spanner_proto::PartitionResponse>(
                                  grpc::ClientContext&,
                                  spanner_proto::PartitionReadRequest const&));
};

TEST(SpannerClientTest, MakeDatabaseName) {
  EXPECT_EQ(
      "projects/dummy_project/instances/dummy_instance/databases/"
      "dummy_database_id",
      MakeDatabaseName("dummy_project", "dummy_instance", "dummy_database_id"));
}

// Dummy placeholder test.
TEST(SpannerClientTest, ReadTest) {
  auto mock = std::make_shared<MockSpannerStub>();

  Client client(
      MakeDatabaseName("dummy_project", "dummy_instance", "dummy_database_id"),
      mock);
  EXPECT_EQ(StatusCode::kUnimplemented,
            client.Read("table", KeySet(), {"column"}).status().code());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
