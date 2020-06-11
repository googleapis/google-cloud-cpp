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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_SPANNER_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_SPANNER_STUB_H

#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

class MockSpannerStub : public google::cloud::spanner::internal::SpannerStub {
 public:
  MOCK_METHOD2(CreateSession,
               StatusOr<google::spanner::v1::Session>(
                   grpc::ClientContext&,
                   google::spanner::v1::CreateSessionRequest const&));

  MOCK_METHOD2(BatchCreateSessions,
               StatusOr<google::spanner::v1::BatchCreateSessionsResponse>(
                   grpc::ClientContext&,
                   google::spanner::v1::BatchCreateSessionsRequest const&));

  MOCK_METHOD3(AsyncBatchCreateSessions,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::spanner::v1::BatchCreateSessionsResponse>>(
                   grpc::ClientContext&,
                   google::spanner::v1::BatchCreateSessionsRequest const&,
                   grpc::CompletionQueue*));

  MOCK_METHOD2(GetSession, StatusOr<google::spanner::v1::Session>(
                               grpc::ClientContext&,
                               google::spanner::v1::GetSessionRequest const&));

  MOCK_METHOD2(ListSessions,
               StatusOr<google::spanner::v1::ListSessionsResponse>(
                   grpc::ClientContext&,
                   google::spanner::v1::ListSessionsRequest const&));

  MOCK_METHOD2(DeleteSession,
               Status(grpc::ClientContext&,
                      google::spanner::v1::DeleteSessionRequest const&));

  MOCK_METHOD3(
      AsyncDeleteSession,
      std::unique_ptr<
          grpc::ClientAsyncResponseReaderInterface<google::protobuf::Empty>>(
          grpc::ClientContext&,
          google::spanner::v1::DeleteSessionRequest const&,
          grpc::CompletionQueue*));

  MOCK_METHOD2(ExecuteSql, StatusOr<google::spanner::v1::ResultSet>(
                               grpc::ClientContext&,
                               google::spanner::v1::ExecuteSqlRequest const&));

  MOCK_METHOD3(AsyncExecuteSql,
               std::unique_ptr<grpc::ClientAsyncResponseReaderInterface<
                   google::spanner::v1::ResultSet>>(
                   grpc::ClientContext&,
                   google::spanner::v1::ExecuteSqlRequest const&,
                   grpc::CompletionQueue*));

  MOCK_METHOD2(
      ExecuteStreamingSql,
      std::unique_ptr<
          grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>(
          grpc::ClientContext&, google::spanner::v1::ExecuteSqlRequest const&));

  MOCK_METHOD2(ExecuteBatchDml,
               StatusOr<google::spanner::v1::ExecuteBatchDmlResponse>(
                   grpc::ClientContext&,
                   google::spanner::v1::ExecuteBatchDmlRequest const&));

  MOCK_METHOD2(Read, StatusOr<google::spanner::v1::ResultSet>(
                         grpc::ClientContext&,
                         google::spanner::v1::ReadRequest const&));

  MOCK_METHOD2(
      StreamingRead,
      std::unique_ptr<
          grpc::ClientReaderInterface<google::spanner::v1::PartialResultSet>>(
          grpc::ClientContext&, google::spanner::v1::ReadRequest const&));

  MOCK_METHOD2(BeginTransaction,
               StatusOr<google::spanner::v1::Transaction>(
                   grpc::ClientContext&,
                   google::spanner::v1::BeginTransactionRequest const&));

  MOCK_METHOD2(Commit, StatusOr<google::spanner::v1::CommitResponse>(
                           grpc::ClientContext&,
                           google::spanner::v1::CommitRequest const&));

  MOCK_METHOD2(Rollback, Status(grpc::ClientContext&,
                                google::spanner::v1::RollbackRequest const&));

  MOCK_METHOD2(PartitionQuery,
               StatusOr<google::spanner::v1::PartitionResponse>(
                   grpc::ClientContext&,
                   google::spanner::v1::PartitionQueryRequest const&));

  MOCK_METHOD2(PartitionRead,
               StatusOr<google::spanner::v1::PartitionResponse>(
                   grpc::ClientContext&,
                   google::spanner::v1::PartitionReadRequest const&));
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_SPANNER_STUB_H
