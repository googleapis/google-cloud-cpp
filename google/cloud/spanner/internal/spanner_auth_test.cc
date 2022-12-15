// Copyright 2022 Google LLC
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

#include "google/cloud/spanner/internal/spanner_auth.h"
#include "google/cloud/spanner/testing/mock_spanner_stub.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::spanner_testing::MockSpannerStub;
using ::google::cloud::testing_util::MakeTypicalAsyncMockAuth;
using ::google::cloud::testing_util::MakeTypicalMockAuth;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Return;

// The general pattern of these tests is to make two requests, both of which
// return an error. The first one because the auth strategy fails, the second
// because the operation in the mock stub fails.

TEST(SpannerAuthTest, CreateSession) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, CreateSession)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::CreateSessionRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.CreateSession(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.CreateSession(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, BatchCreateSessions) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, BatchCreateSessions)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::BatchCreateSessionsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.BatchCreateSessions(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.BatchCreateSessions(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, DeleteSession) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, DeleteSession)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::DeleteSessionRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.DeleteSession(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DeleteSession(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, ExecuteSql) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, ExecuteSql)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::ExecuteSqlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ExecuteSql(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ExecuteSql(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, ExecuteStreamingSql) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, ExecuteStreamingSql)
      .WillOnce([](grpc::ClientContext&,
                   google::spanner::v1::ExecuteSqlRequest const&) {
        return absl::make_unique<ClientReaderInterfaceError>(
            Status(StatusCode::kPermissionDenied, "uh-oh"));
      });

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::ExecuteSqlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ExecuteStreamingSql(ctx, request);
  google::spanner::v1::PartialResultSet response;
  ASSERT_FALSE(auth_failure->Read(&response));
  EXPECT_THAT(MakeStatusFromRpcError(auth_failure->Finish()),
              StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ExecuteStreamingSql(ctx, request);
  ASSERT_FALSE(auth_success->Read(&response));
  EXPECT_THAT(MakeStatusFromRpcError(auth_success->Finish()),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, ExecuteBatchDml) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, ExecuteBatchDml)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::ExecuteBatchDmlRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ExecuteBatchDml(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ExecuteBatchDml(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, StreamingRead) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, StreamingRead)
      .WillOnce(
          [](grpc::ClientContext&, google::spanner::v1::ReadRequest const&) {
            return absl::make_unique<ClientReaderInterfaceError>(
                Status(StatusCode::kPermissionDenied, "uh-oh"));
          });

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::ReadRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.StreamingRead(ctx, request);
  google::spanner::v1::PartialResultSet response;
  ASSERT_FALSE(auth_failure->Read(&response));
  EXPECT_THAT(MakeStatusFromRpcError(auth_failure->Finish()),
              StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.StreamingRead(ctx, request);
  ASSERT_FALSE(auth_success->Read(&response));
  EXPECT_THAT(MakeStatusFromRpcError(auth_success->Finish()),
              StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, BeginTransaction) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, BeginTransaction)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::BeginTransactionRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.BeginTransaction(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.BeginTransaction(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, Commit) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, Commit)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::CommitRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.Commit(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.Commit(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, Rollback) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, Rollback)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::RollbackRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.Rollback(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.Rollback(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, PartitionQuery) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, PartitionQuery)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::PartitionQueryRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.PartitionQuery(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.PartitionQuery(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, PartitionRead) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, PartitionRead)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  SpannerAuth under_test(MakeTypicalMockAuth(), mock);
  google::spanner::v1::PartitionReadRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.PartitionRead(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.PartitionRead(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, AsyncBatchCreateSessions) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, AsyncBatchCreateSessions)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::spanner::v1::BatchCreateSessionsRequest const&) {
        return make_ready_future(
            StatusOr<google::spanner::v1::BatchCreateSessionsResponse>(
                Status(StatusCode::kPermissionDenied, "uh-oh")));
      });

  SpannerAuth under_test(MakeTypicalAsyncMockAuth(), mock);
  google::spanner::v1::BatchCreateSessionsRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncBatchCreateSessions(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncBatchCreateSessions(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, AsyncDeleteSession) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, AsyncDeleteSession)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::spanner::v1::DeleteSessionRequest const&) {
        return make_ready_future(
            Status(StatusCode::kPermissionDenied, "uh-oh"));
      });

  SpannerAuth under_test(MakeTypicalAsyncMockAuth(), mock);
  google::spanner::v1::DeleteSessionRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncDeleteSession(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncDeleteSession(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

TEST(SpannerAuthTest, AsyncExecuteSql) {
  auto mock = std::make_shared<MockSpannerStub>();
  EXPECT_CALL(*mock, AsyncExecuteSql)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::spanner::v1::ExecuteSqlRequest const&) {
        return make_ready_future(StatusOr<google::spanner::v1::ResultSet>(
            Status(StatusCode::kPermissionDenied, "uh-oh")));
      });

  SpannerAuth under_test(MakeTypicalAsyncMockAuth(), mock);
  google::spanner::v1::ExecuteSqlRequest request;
  CompletionQueue cq;
  auto auth_failure = under_test.AsyncExecuteSql(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_failure.get(), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncExecuteSql(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  EXPECT_THAT(auth_success.get(), StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
