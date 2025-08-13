// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_SPANNER_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_SPANNER_STUB_H

#include "google/cloud/spanner/internal/spanner_stub.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class MockSpannerStub : public google::cloud::spanner_internal::SpannerStub {
 public:
  MOCK_METHOD(StatusOr<google::spanner::v1::Session>, CreateSession,
              (grpc::ClientContext&, Options const&,
               google::spanner::v1::CreateSessionRequest const&),
              (override));

  MOCK_METHOD(future<StatusOr<google::spanner::v1::Session>>,
              AsyncCreateSession,
              (google::cloud::CompletionQueue & cq,
               std::shared_ptr<grpc::ClientContext> context,
               google::cloud::internal::ImmutableOptions options,
               google::spanner::v1::CreateSessionRequest const& request),
              (override));

  MOCK_METHOD(StatusOr<google::spanner::v1::BatchCreateSessionsResponse>,
              BatchCreateSessions,
              (grpc::ClientContext&, Options const&,
               google::spanner::v1::BatchCreateSessionsRequest const&),
              (override));

  MOCK_METHOD(
      future<StatusOr<google::spanner::v1::BatchCreateSessionsResponse>>,
      AsyncBatchCreateSessions,
      (CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
       google::cloud::internal::ImmutableOptions,
       google::spanner::v1::BatchCreateSessionsRequest const&),
      (override));

  MOCK_METHOD(Status, DeleteSession,
              (grpc::ClientContext&, Options const&,
               google::spanner::v1::DeleteSessionRequest const&),
              (override));

  MOCK_METHOD(future<Status>, AsyncDeleteSession,
              (CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::spanner::v1::DeleteSessionRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::spanner::v1::ResultSet>, ExecuteSql,
              (grpc::ClientContext&, Options const&,
               google::spanner::v1::ExecuteSqlRequest const&),
              (override));

  MOCK_METHOD(future<StatusOr<google::spanner::v1::ResultSet>>, AsyncExecuteSql,
              (CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
               google::cloud::internal::ImmutableOptions,
               google::spanner::v1::ExecuteSqlRequest const&),
              (override));

  MOCK_METHOD(
      std::unique_ptr<
          internal::StreamingReadRpc<google::spanner::v1::PartialResultSet>>,
      ExecuteStreamingSql,
      (std::shared_ptr<grpc::ClientContext>, Options const&,
       google::spanner::v1::ExecuteSqlRequest const&),
      (override));

  MOCK_METHOD(StatusOr<google::spanner::v1::ExecuteBatchDmlResponse>,
              ExecuteBatchDml,
              (grpc::ClientContext&, Options const&,
               google::spanner::v1::ExecuteBatchDmlRequest const&),
              (override));

  MOCK_METHOD(
      std::unique_ptr<
          internal::StreamingReadRpc<google::spanner::v1::PartialResultSet>>,
      StreamingRead,
      (std::shared_ptr<grpc::ClientContext>, Options const&,
       google::spanner::v1::ReadRequest const&),
      (override));

  MOCK_METHOD(StatusOr<google::spanner::v1::Transaction>, BeginTransaction,
              (grpc::ClientContext&, Options const&,
               google::spanner::v1::BeginTransactionRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::spanner::v1::CommitResponse>, Commit,
              (grpc::ClientContext&, Options const&,
               google::spanner::v1::CommitRequest const&),
              (override));

  MOCK_METHOD(Status, Rollback,
              (grpc::ClientContext&, Options const&,
               google::spanner::v1::RollbackRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::spanner::v1::PartitionResponse>, PartitionQuery,
              (grpc::ClientContext&, Options const&,
               google::spanner::v1::PartitionQueryRequest const&),
              (override));

  MOCK_METHOD(StatusOr<google::spanner::v1::PartitionResponse>, PartitionRead,
              (grpc::ClientContext&, Options const&,
               google::spanner::v1::PartitionReadRequest const&),
              (override));

  MOCK_METHOD(
      std::unique_ptr<
          internal::StreamingReadRpc<google::spanner::v1::BatchWriteResponse>>,
      BatchWrite,
      (std::shared_ptr<grpc::ClientContext>, Options const&,
       google::spanner::v1::BatchWriteRequest const&),
      (override));
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_SPANNER_STUB_H
