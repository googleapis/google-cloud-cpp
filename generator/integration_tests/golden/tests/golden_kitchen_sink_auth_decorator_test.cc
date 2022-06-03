// Copyright 2021 Google LLC
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

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_auth_decorator.h"
#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/internal/async_streaming_write_rpc_impl.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include "generator/integration_tests/golden/mocks/mock_golden_kitchen_sink_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_internal::MockWriteObjectStreamingWriteRpc;
using ::google::cloud::internal::StreamingReadRpcError;
using ::google::cloud::testing_util::MakeTypicalAsyncMockAuth;
using ::google::cloud::testing_util::MakeTypicalMockAuth;
using ::google::cloud::testing_util::StatusIs;
using ::google::test::admin::database::v1::TailLogEntriesRequest;
using ::google::test::admin::database::v1::TailLogEntriesResponse;
using ::google::test::admin::database::v1::WriteObjectRequest;
using ::google::test::admin::database::v1::WriteObjectResponse;
using ::testing::ByMove;
using ::testing::IsNull;
using ::testing::Return;

// The general pattern of these test is to make two requests, both of which
// return an error. The first one because the auth strategy fails, the second
// because the operation in the mock stub fails.

TEST(GoldenKitchenSinkAuthDecoratorTest, GenerateAccessToken) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenKitchenSinkAuth(MakeTypicalMockAuth(), mock);
  ::google::test::admin::database::v1::GenerateAccessTokenRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GenerateAccessToken(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GenerateAccessToken(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenKitchenSinkAuthDecoratorTest, GenerateIdToken) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, GenerateIdToken)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenKitchenSinkAuth(MakeTypicalMockAuth(), mock);
  ::google::test::admin::database::v1::GenerateIdTokenRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.GenerateIdToken(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GenerateIdToken(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenKitchenSinkAuthDecoratorTest, WriteLogEntries) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, WriteLogEntries)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenKitchenSinkAuth(MakeTypicalMockAuth(), mock);
  ::google::test::admin::database::v1::WriteLogEntriesRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.WriteLogEntries(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.WriteLogEntries(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenKitchenSinkAuthDecoratorTest, ListLogs) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ListLogs)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenKitchenSinkAuth(MakeTypicalMockAuth(), mock);
  ::google::test::admin::database::v1::ListLogsRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListLogs(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListLogs(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

// This test is fairly different because we need to return a streaming RPC.
TEST(GoldenKitchenSinkAuthDecoratorTest, TailLogEntries) {
  using ResponseType =
      google::test::admin::database::v1::TailLogEntriesResponse;
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, TailLogEntries)
      .WillOnce([](::testing::Unused, ::testing::Unused) {
        return absl::make_unique<StreamingReadRpcError<ResponseType>>(
            Status(StatusCode::kPermissionDenied, "uh-oh"));
      });

  auto under_test = GoldenKitchenSinkAuth(MakeTypicalMockAuth(), mock);
  ::google::test::admin::database::v1::TailLogEntriesRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.TailLogEntries(
      absl::make_unique<grpc::ClientContext>(), request);
  auto v = auth_failure->Read();
  ASSERT_TRUE(absl::holds_alternative<Status>(v));
  EXPECT_THAT(absl::get<Status>(v), StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.TailLogEntries(
      absl::make_unique<grpc::ClientContext>(), request);
  v = auth_success->Read();
  ASSERT_TRUE(absl::holds_alternative<Status>(v));
  EXPECT_THAT(absl::get<Status>(v), StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenKitchenSinkAuthDecoratorTest, ListServiceAccountKeys) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ListServiceAccountKeys)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenKitchenSinkAuth(MakeTypicalMockAuth(), mock);
  ::google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListServiceAccountKeys(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListServiceAccountKeys(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenKitchenSinkAuthDecoratorTest, WriteObject) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, WriteObject)
      .WillOnce([](std::unique_ptr<grpc::ClientContext>) {
        auto stream = absl::make_unique<MockWriteObjectStreamingWriteRpc>();
        EXPECT_CALL(*stream, Write)
            .WillOnce(Return(true))
            .WillOnce(Return(false));
        EXPECT_CALL(*stream, Close)
            .WillOnce(Return(StatusOr<WriteObjectResponse>(
                Status(StatusCode::kPermissionDenied, "uh-oh"))));
        return stream;
      });

  auto under_test = GoldenKitchenSinkAuth(MakeTypicalMockAuth(), mock);
  auto stream =
      under_test.WriteObject(absl::make_unique<grpc::ClientContext>());
  EXPECT_FALSE(stream->Write(WriteObjectRequest{}, grpc::WriteOptions()));
  auto response = stream->Close();
  EXPECT_THAT(response, StatusIs(StatusCode::kInvalidArgument));

  stream = under_test.WriteObject(absl::make_unique<grpc::ClientContext>());
  EXPECT_TRUE(stream->Write(WriteObjectRequest{}, grpc::WriteOptions()));
  EXPECT_FALSE(stream->Write(WriteObjectRequest{}, grpc::WriteOptions()));
  response = stream->Close();
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenKitchenSinkAuthDecoratorTest, AsyncTailLogEntries) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  using ErrorStream = ::google::cloud::internal::AsyncStreamingReadRpcError<
      TailLogEntriesResponse>;
  EXPECT_CALL(*mock, AsyncTailLogEntries)
      .WillOnce(Return(ByMove(absl::make_unique<ErrorStream>(
          Status(StatusCode::kAborted, "uh-oh")))));

  google::cloud::CompletionQueue cq;
  auto under_test = GoldenKitchenSinkAuth(MakeTypicalAsyncMockAuth(), mock);
  ::google::test::admin::database::v1::TailLogEntriesRequest request;

  auto auth_failure = under_test.AsyncTailLogEntries(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  auto start = auth_failure->Start().get();
  EXPECT_FALSE(start);
  auto finish = auth_failure->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncTailLogEntries(
      cq, absl::make_unique<grpc::ClientContext>(), request);
  start = auth_success->Start().get();
  EXPECT_FALSE(start);
  finish = auth_success->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

TEST(GoldenKitchenSinkAuthDecoratorTest, AsyncWriteObject) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  using ErrorStream = ::google::cloud::internal::AsyncStreamingWriteRpcError<
      WriteObjectRequest, WriteObjectResponse>;
  EXPECT_CALL(*mock, AsyncWriteObject)
      .WillOnce(Return(ByMove(absl::make_unique<ErrorStream>(
          Status(StatusCode::kAborted, "uh-oh")))));

  google::cloud::CompletionQueue cq;
  auto under_test = GoldenKitchenSinkAuth(MakeTypicalAsyncMockAuth(), mock);

  auto auth_failure =
      under_test.AsyncWriteObject(cq, absl::make_unique<grpc::ClientContext>());
  auto start = auth_failure->Start().get();
  EXPECT_FALSE(start);
  auto finish = auth_failure->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success =
      under_test.AsyncWriteObject(cq, absl::make_unique<grpc::ClientContext>());
  start = auth_success->Start().get();
  EXPECT_FALSE(start);
  finish = auth_success->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
