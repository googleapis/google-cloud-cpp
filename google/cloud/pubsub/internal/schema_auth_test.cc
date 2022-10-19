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

#include "google/cloud/pubsub/internal/schema_auth_decorator.h"
#include "google/cloud/pubsub/testing/mock_schema_stub.h"
#include "google/cloud/testing_util/mock_grpc_authentication_strategy.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MakeTypicalMockAuth;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Return;

TEST(SchemaAuthTest, CreateSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, CreateSchema)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SchemaServiceAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::CreateSchemaRequest request;
  auto auth_failure = under_test.CreateSchema(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.CreateSchema(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SchemaAuthTest, GetSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, GetSchema)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SchemaServiceAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::GetSchemaRequest request;
  auto auth_failure = under_test.GetSchema(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.GetSchema(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SchemaAuthTest, ListSchemas) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ListSchemas)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SchemaServiceAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::ListSchemasRequest request;
  auto auth_failure = under_test.ListSchemas(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ListSchemas(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SchemaAuthTest, DeleteSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, DeleteSchema)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SchemaServiceAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::DeleteSchemaRequest request;
  auto auth_failure = under_test.DeleteSchema(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.DeleteSchema(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SchemaAuthTest, ValidateSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ValidateSchema)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SchemaServiceAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::ValidateSchemaRequest request;
  auto auth_failure = under_test.ValidateSchema(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ValidateSchema(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

TEST(SchemaAuthTest, ValidateMessage) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ValidateMessage)
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "uh-oh")));
  auto under_test = SchemaServiceAuth(MakeTypicalMockAuth(), mock);
  grpc::ClientContext ctx;
  google::pubsub::v1::ValidateMessageRequest request;
  auto auth_failure = under_test.ValidateMessage(ctx, request);
  EXPECT_THAT(ctx.credentials(), IsNull());
  EXPECT_THAT(auth_failure, StatusIs(StatusCode::kInvalidArgument));

  auto auth_success = under_test.ValidateMessage(ctx, request);
  EXPECT_THAT(ctx.credentials(), Not(IsNull()));
  EXPECT_THAT(auth_success, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
