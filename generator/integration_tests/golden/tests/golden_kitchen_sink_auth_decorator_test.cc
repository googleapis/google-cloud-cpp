// Copyright 2021 Google LLC
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

#include "generator/integration_tests/golden/internal/golden_kitchen_sink_auth_decorator.h"
#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include "generator/integration_tests/golden/mocks/mock_golden_kitchen_sink_stub.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace golden_internal {
inline namespace GOOGLE_CLOUD_CPP_GENERATED_NS {
namespace {

using ::google::cloud::internal::GrpcAuthenticationStrategy;
using ::google::cloud::internal::StreamingReadRpcError;
using ::google::cloud::testing_util::MockAuthenticationStrategy;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsNull;

std::shared_ptr<GrpcAuthenticationStrategy> MakeMockAuth() {
  auto auth = std::make_shared<MockAuthenticationStrategy>();
  EXPECT_CALL(*auth, ConfigureContext)
      .WillOnce([](grpc::ClientContext&) {
        return Status(StatusCode::kInvalidArgument, "cannot-set-credentials");
      })
      .WillOnce([](grpc::ClientContext& context) {
        context.set_credentials(
            grpc::AccessTokenCredentials("test-only-invalid"));
        return Status{};
      });
  return auth;
}

// The general pattern of these test is to make two requests, both of which
// return an error. The first one because the auth strategy fails, the second
// because the operation in the mock stub fails.

TEST(GoldenKitchenSinkAuthDecoratorTest, GenerateAccessToken) {
  auto mock = std::make_shared<MockGoldenKitchenSinkStub>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce(
          ::testing::Return(Status(StatusCode::kPermissionDenied, "uh-oh")));

  auto under_test = GoldenKitchenSinkAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenKitchenSinkAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenKitchenSinkAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenKitchenSinkAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenKitchenSinkAuth(MakeMockAuth(), mock);
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

  auto under_test = GoldenKitchenSinkAuth(MakeMockAuth(), mock);
  ::google::test::admin::database::v1::ListServiceAccountKeysRequest request;
  grpc::ClientContext ctx;
  auto auth_failure = under_test.ListServiceAccountKeys(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListServiceAccountKeys(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_GENERATED_NS
}  // namespace golden_internal
}  // namespace cloud
}  // namespace google
