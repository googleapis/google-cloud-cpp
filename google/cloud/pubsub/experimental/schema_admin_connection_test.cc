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

#include "google/cloud/pubsub/experimental/schema_admin_connection.h"
#include "google/cloud/pubsub/testing/mock_schema_stub.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_experimental {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::pubsub_testing::TestBackoffPolicy;
using ::google::cloud::pubsub_testing::TestRetryPolicy;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;

TEST(SchemaAdminConnectionTest, Create) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();

  auto constexpr kRequestText = R"pb(
    parent: "projects/test-project"
    schema_id: "test-schema"
    schema { type: AVRO definition: "test-only-invalid" }
  )pb";
  google::pubsub::v1::CreateSchemaRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(kRequestText, &request));

  auto constexpr kResponseText = R"pb(
    name: "projects/test-project/schemas/test-schema"
    type: PROTOCOL_BUFFER
    definition: "test-only-invalid"
  )pb";
  google::pubsub::v1::Schema response;
  ASSERT_TRUE(TextFormat::ParseFromString(kResponseText, &response));
  EXPECT_CALL(*mock, CreateSchema(_, IsProtoEqual(request)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext& context,
                    google::pubsub::v1::CreateSchemaRequest const&) {
        // Use this test to also verify the metadata decorator is automatically
        // configured.
        EXPECT_THAT(google::cloud::testing_util::IsContextMDValid(
                        context, "google.pubsub.v1.SchemaService.CreateSchema",
                        google::cloud::internal::ApiClientHeader()),
                    IsOk());
        return make_status_or(response);
      });

  auto schema_admin = pubsub_internal::MakeSchemaAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());

  auto actual = schema_admin->CreateSchema(request);
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(response));
}

TEST(SchemaAdminConnectionTest, Get) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();

  auto constexpr kRequestText = R"pb(
    name: "projects/test-project/schemas/test-schema"
    view: FULL
  )pb";
  google::pubsub::v1::GetSchemaRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(kRequestText, &request));

  auto constexpr kResponseText = R"pb(
    name: "projects/test-project/schemas/test-schema"
    type: PROTOCOL_BUFFER
    definition: "test-only-invalid"
  )pb";
  google::pubsub::v1::Schema response;
  ASSERT_TRUE(TextFormat::ParseFromString(kResponseText, &response));

  EXPECT_CALL(*mock, GetSchema(_, IsProtoEqual(request)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(make_status_or(response)));

  auto schema_admin = pubsub_internal::MakeSchemaAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto actual = schema_admin->GetSchema(request);
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(response));
}

TEST(SchemaAdminConnectionTest, List) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();

  std::string const text = R"pb(
    parent: "projects/test-project" view: BASIC
  )pb";
  google::pubsub::v1::ListSchemasRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(text, &request));

  EXPECT_CALL(*mock, ListSchemas(_, IsProtoEqual(request)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce([&](grpc::ClientContext&,
                    google::pubsub::v1::ListSchemasRequest const&) {
        google::pubsub::v1::ListSchemasResponse response;
        response.add_schemas()->set_name("test-schema-01");
        response.add_schemas()->set_name("test-schema-02");
        return make_status_or(response);
      });

  auto schema_admin = pubsub_internal::MakeSchemaAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  std::vector<std::string> schema_names;
  for (auto& t : schema_admin->ListSchemas(request)) {
    ASSERT_THAT(t, IsOk());
    schema_names.push_back(t->name());
  }
  EXPECT_THAT(schema_names, ElementsAre("test-schema-01", "test-schema-02"));
}

/**
 * @test Verify DeleteSchema() and logging works.
 *
 * We use this test for both DeleteSchema and logging. DeleteSchema has a simple
 * return type, so it is a good candidate to do the logging test too.
 */
TEST(SchemaAdminConnectionTest, DeleteWithLogging) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();
  testing_util::ScopedLog log;

  std::string const text = R"pb(
    name: "projects/test-project/schemas/test-schema"
  )pb";
  google::pubsub::v1::DeleteSchemaRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(text, &request));

  EXPECT_CALL(*mock, DeleteSchema(_, IsProtoEqual(request)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(Status{}));

  auto schema_admin = pubsub_internal::MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}.enable_tracing("rpc"), mock,
      TestRetryPolicy(), TestBackoffPolicy());
  auto response = schema_admin->DeleteSchema(request);
  ASSERT_THAT(response, IsOk());

  EXPECT_THAT(log.ExtractLines(), Contains(HasSubstr("DeleteSchema")));
}

TEST(SchemaAdminConnectionTest, ValidateSchema) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();

  std::string const text = R"pb(
    parent: "projects/test-project"
    schema { type: AVRO definition: "test-only-invalid" }
  )pb";
  google::pubsub::v1::ValidateSchemaRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(text, &request));

  EXPECT_CALL(*mock, ValidateSchema(_, IsProtoEqual(request)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(
          Return(make_status_or(google::pubsub::v1::ValidateSchemaResponse{})));

  auto schema_admin = pubsub_internal::MakeSchemaAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto response = schema_admin->ValidateSchema(request);
  ASSERT_THAT(response, IsOk());
}

TEST(SchemaAdminConnectionTest, ValidateMessage) {
  auto mock = std::make_shared<pubsub_testing::MockSchemaStub>();

  std::string const text = R"pb(
    parent: "projects/test-project"
    encoding: JSON
    message: "test-only-invalid"
    schema { type: AVRO definition: "test-only-invalid" }
  )pb";
  google::pubsub::v1::ValidateMessageRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(text, &request));

  EXPECT_CALL(*mock, ValidateMessage(_, IsProtoEqual(request)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")))
      .WillOnce(Return(
          make_status_or(google::pubsub::v1::ValidateMessageResponse{})));

  auto schema_admin = pubsub_internal::MakeSchemaAdminConnection(
      {}, mock, TestRetryPolicy(), TestBackoffPolicy());
  auto response = schema_admin->ValidateMessage(request);
  ASSERT_THAT(response, IsOk());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_experimental
}  // namespace cloud
}  // namespace google
