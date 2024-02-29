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

#include "generator/integration_tests/golden/v1/internal/golden_kitchen_sink_auth_decorator.h"
#include "generator/integration_tests/tests/mock_golden_kitchen_sink_stub.h"
#include "google/cloud/internal/async_streaming_read_rpc_impl.h"
#include "google/cloud/internal/async_streaming_write_rpc_impl.h"
#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_v1_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::golden_v1_internal::MockStreamingWriteRpc;
using ::google::cloud::internal::StreamingReadRpcError;
using ::google::cloud::testing_util::MakeTypicalAsyncMockAuth;
using ::google::cloud::testing_util::MakeTypicalMockAuth;
using ::google::cloud::testing_util::StatusIs;
using ::google::test::admin::database::v1::Request;
using ::google::test::admin::database::v1::Response;
using ::testing::ByMove;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::VariantWith;

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
  auto auth_failure = under_test.GenerateAccessToken(ctx, Options{}, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GenerateAccessToken(ctx, Options{}, request);
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
  auto auth_failure = under_test.GenerateIdToken(ctx, Options{}, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GenerateIdToken(ctx, Options{}, request);
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
  auto auth_failure = under_test.WriteLogEntries(ctx, Options{}, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.WriteLogEntries(ctx, Options{}, request);
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
  auto auth_failure = under_test.ListLogs(ctx, Options{}, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListLogs(ctx, Options{}, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

// This test is fairly different because we need to return a streaming RPC.
TEST(GoldenKitchenSinkAuthDecoratorTest, StreamingRead) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, StreamingRead).WillOnce([] {
    return std::make_unique<StreamingReadRpcError<Response>>(
        Status(StatusCode::kPermissionDenied, "uh-oh"));
  });

  auto under_test = GoldenKitchenSinkAuth(MakeTypicalMockAuth(), mock);
  ::google::test::admin::database::v1::Request request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.StreamingRead(
      std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_failure->Read(),
              VariantWith<Status>(StatusIs(StatusCode::kInvalidArgument)));

  auto auth_success = under_test.StreamingRead(
      std::make_shared<grpc::ClientContext>(), Options{}, request);
  EXPECT_THAT(auth_success->Read(),
              VariantWith<Status>(StatusIs(StatusCode::kPermissionDenied)));
}

TEST(GoldenKitchenSinkAuthDecoratorTest, ListServiceAccountKeys) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, ListServiceAccountKeys)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenKitchenSinkAuth(MakeTypicalMockAuth(), mock);
  ::google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  grpc::ClientContext ctx;
  auto auth_failure =
      under_test.ListServiceAccountKeys(ctx, Options{}, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success =
      under_test.ListServiceAccountKeys(ctx, Options{}, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenKitchenSinkAuthDecoratorTest, StreamingWrite) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, StreamingWrite).WillOnce([] {
    auto stream = std::make_unique<MockStreamingWriteRpc>();
    EXPECT_CALL(*stream, Write).WillOnce(Return(true)).WillOnce(Return(false));
    EXPECT_CALL(*stream, Close)
        .WillOnce(Return(StatusOr<Response>(
            Status(StatusCode::kPermissionDenied, "uh-oh"))));
    return stream;
  });

  auto under_test = GoldenKitchenSinkAuth(MakeTypicalMockAuth(), mock);
  auto stream = under_test.StreamingWrite(
      std::make_shared<grpc::ClientContext>(), Options{});
  EXPECT_FALSE(stream->Write(Request{}, grpc::WriteOptions()));
  auto response = stream->Close();
  EXPECT_THAT(response, StatusIs(StatusCode::kInvalidArgument));

  stream = under_test.StreamingWrite(std::make_shared<grpc::ClientContext>(),
                                     Options{});
  EXPECT_TRUE(stream->Write(Request{}, grpc::WriteOptions()));
  EXPECT_FALSE(stream->Write(Request{}, grpc::WriteOptions()));
  response = stream->Close();
  EXPECT_THAT(response, StatusIs(StatusCode::kPermissionDenied));
}

TEST(GoldenKitchenSinkAuthDecoratorTest, AsyncStreamingRead) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  using ErrorStream =
      ::google::cloud::internal::AsyncStreamingReadRpcError<Response>;
  EXPECT_CALL(*mock, AsyncStreamingRead)
      .WillOnce(Return(ByMove(std::make_unique<ErrorStream>(
          Status(StatusCode::kAborted, "uh-oh")))));

  google::cloud::CompletionQueue cq;
  auto under_test = GoldenKitchenSinkAuth(MakeTypicalAsyncMockAuth(), mock);
  ::google::test::admin::database::v1::Request request;

  auto auth_failure = under_test.AsyncStreamingRead(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}), request);
  auto start = auth_failure->Start().get();
  EXPECT_FALSE(start);
  auto finish = auth_failure->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncStreamingRead(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}), request);
  start = auth_success->Start().get();
  EXPECT_FALSE(start);
  finish = auth_success->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

TEST(GoldenKitchenSinkAuthDecoratorTest, AsyncStreamingWrite) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  using ErrorStream =
      ::google::cloud::internal::AsyncStreamingWriteRpcError<Request, Response>;
  EXPECT_CALL(*mock, AsyncStreamingWrite)
      .WillOnce(Return(ByMove(std::make_unique<ErrorStream>(
          Status(StatusCode::kAborted, "uh-oh")))));

  google::cloud::CompletionQueue cq;
  auto under_test = GoldenKitchenSinkAuth(MakeTypicalAsyncMockAuth(), mock);

  auto auth_failure = under_test.AsyncStreamingWrite(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}));
  auto start = auth_failure->Start().get();
  EXPECT_FALSE(start);
  auto finish = auth_failure->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.AsyncStreamingWrite(
      cq, std::make_shared<grpc::ClientContext>(),
      google::cloud::internal::MakeImmutableOptions({}));
  start = auth_success->Start().get();
  EXPECT_FALSE(start);
  finish = auth_success->Finish().get();
  EXPECT_THAT(finish, StatusIs(StatusCode::kAborted));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace golden_v1_internal
}  // namespace cloud
}  // namespace google
