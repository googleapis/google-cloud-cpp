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

#include "google/cloud/pubsub/internal/schema_logging.h"
#include "google/cloud/pubsub/testing/mock_schema_stub.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

class SchemaLoggingTest : public ::testing::Test {
 protected:
  static Status TransientError() {
    return Status(StatusCode::kUnavailable, "try-again");
  }

  testing_util::ScopedLog log_;
};

TEST_F(SchemaLoggingTest, CreateSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, CreateSchema)
      .WillOnce(Return(make_status_or(google::pubsub::v1::Schema{})));
  SchemaLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::CreateSchemaRequest request;
  auto status = stub.CreateSchema(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("CreateSchema")));
}

TEST_F(SchemaLoggingTest, GetSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, GetSchema)
      .WillOnce(Return(make_status_or(google::pubsub::v1::Schema{})));
  SchemaLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::GetSchemaRequest request;
  auto status = stub.GetSchema(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("GetSchema")));
}

TEST_F(SchemaLoggingTest, ListSchemas) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ListSchemas)
      .WillOnce(
          Return(make_status_or(google::pubsub::v1::ListSchemasResponse{})));
  SchemaLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::ListSchemasRequest request;
  auto status = stub.ListSchemas(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("ListSchemas")));
}

TEST_F(SchemaLoggingTest, DeleteSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, DeleteSchema).WillOnce(Return(Status{}));
  SchemaLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::DeleteSchemaRequest request;
  auto status = stub.DeleteSchema(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("DeleteSchema")));
}

TEST_F(SchemaLoggingTest, ValidateSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ValidateSchema)
      .WillOnce(
          Return(make_status_or(google::pubsub::v1::ValidateSchemaResponse{})));
  SchemaLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::ValidateSchemaRequest request;
  auto status = stub.ValidateSchema(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("ValidateSchema")));
}

TEST_F(SchemaLoggingTest, ValidateMessage) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  EXPECT_CALL(*mock, ValidateMessage)
      .WillOnce(Return(
          make_status_or(google::pubsub::v1::ValidateMessageResponse{})));
  SchemaLogging stub(mock, TracingOptions{}.SetOptions("single_line_mode"));
  grpc::ClientContext context;
  google::pubsub::v1::ValidateMessageRequest request;
  auto status = stub.ValidateMessage(context, request);
  EXPECT_STATUS_OK(status);
  EXPECT_THAT(log_.ExtractLines(), Contains(HasSubstr("ValidateMessage")));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
