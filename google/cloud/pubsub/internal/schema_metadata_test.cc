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

#include "google/cloud/pubsub/internal/schema_metadata.h"
#include "google/cloud/pubsub/schema.h"
#include "google/cloud/pubsub/testing/mock_schema_stub.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::pubsub::Schema;
using ::google::cloud::testing_util::IsContextMDValid;
using ::google::cloud::testing_util::IsOk;

TEST(SchemaMetadataTest, CreateSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, CreateSchema)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::CreateSchemaRequest const&) {
        EXPECT_THAT(IsContextMDValid(
                        context, "google.pubsub.v1.SchemaService.CreateSchema",
                        google::cloud::internal::ApiClientHeader()),
                    IsOk());
        return make_status_or(google::pubsub::v1::Schema{});
      });
  SchemaMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::CreateSchemaRequest request;
  request.set_parent("projects/test-project");
  auto status = stub.CreateSchema(context, request);
  EXPECT_THAT(status, IsOk());
}

TEST(SchemaMetadataTest, GetSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, GetSchema)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::GetSchemaRequest const&) {
        EXPECT_THAT(IsContextMDValid(
                        context, "google.pubsub.v1.SchemaService.GetSchema",
                        google::cloud::internal::ApiClientHeader()),
                    IsOk());
        return make_status_or(google::pubsub::v1::Schema{});
      });
  SchemaMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::GetSchemaRequest request;
  request.set_name(Schema("test-project", "test-schema").FullName());
  auto status = stub.GetSchema(context, request);
  EXPECT_THAT(status, IsOk());
}

TEST(SchemaMetadataTest, ListSchemas) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ListSchemas)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::ListSchemasRequest const&) {
        EXPECT_THAT(IsContextMDValid(
                        context, "google.pubsub.v1.SchemaService.ListSchemas",
                        google::cloud::internal::ApiClientHeader()),
                    IsOk());
        return make_status_or(google::pubsub::v1::ListSchemasResponse{});
      });
  SchemaMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::ListSchemasRequest request;
  request.set_parent("projects/test-project");
  auto status = stub.ListSchemas(context, request);
  EXPECT_THAT(status, IsOk());
}

TEST(SchemaMetadataTest, DeleteSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, DeleteSchema)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::DeleteSchemaRequest const&) {
        EXPECT_THAT(IsContextMDValid(
                        context, "google.pubsub.v1.SchemaService.DeleteSchema",
                        google::cloud::internal::ApiClientHeader()),
                    IsOk());
        return Status{};
      });
  SchemaMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::DeleteSchemaRequest request;
  request.set_name(Schema("test-project", "test-schema").FullName());
  auto status = stub.DeleteSchema(context, request);
  EXPECT_THAT(status, IsOk());
}

TEST(SchemaMetadataTest, ValidateSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ValidateSchema)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::ValidateSchemaRequest const&) {
        EXPECT_THAT(
            IsContextMDValid(context,
                             "google.pubsub.v1.SchemaService.ValidateSchema",
                             google::cloud::internal::ApiClientHeader()),
            IsOk());
        return make_status_or(google::pubsub::v1::ValidateSchemaResponse{});
      });
  SchemaMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::ValidateSchemaRequest request;
  request.set_parent("projects/test-project");
  auto status = stub.ValidateSchema(context, request);
  EXPECT_THAT(status, IsOk());
}

TEST(SchemaMetadataTest, ValidateMessage) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ValidateMessage)
      .WillOnce([](grpc::ClientContext& context,
                   google::pubsub::v1::ValidateMessageRequest const&) {
        EXPECT_THAT(
            IsContextMDValid(context,
                             "google.pubsub.v1.SchemaService.ValidateMessage",
                             google::cloud::internal::ApiClientHeader()),
            IsOk());
        return make_status_or(google::pubsub::v1::ValidateMessageResponse{});
      });
  SchemaMetadata stub(mock);
  grpc::ClientContext context;
  google::pubsub::v1::ValidateMessageRequest request;
  request.set_parent("projects/test-project");
  auto status = stub.ValidateMessage(context, request);
  EXPECT_THAT(status, IsOk());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
