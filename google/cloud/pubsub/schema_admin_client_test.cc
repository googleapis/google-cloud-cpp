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

#include "schema_admin_client.h"
#include "google/cloud/pubsub/mocks/mock_schema_admin_connection.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::protobuf::TextFormat;
using ::testing::ElementsAre;

TEST(SchemaAdminClient, CreateAvroProtobuf) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();
  Schema const schema("test-project", "test-schema");

  auto constexpr kExpectedText = R"pb(
    parent: "projects/test-project"
    schema_id: "test-schema"
    schema { type: AVRO definition: "test-only-invalid-avro" }
  )pb";
  google::pubsub::v1::CreateSchemaRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &expected));

  EXPECT_CALL(*mock, CreateSchema)
      .WillOnce([&](google::pubsub::v1::CreateSchemaRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(expected));
        google::pubsub::v1::Schema response = r.schema();
        response.set_name(schema.FullName());
        return make_status_or(response);
      });
  SchemaAdminClient client(mock);
  auto const response =
      client.CreateAvroSchema(schema, "test-only-invalid-avro");
  EXPECT_THAT(response, IsOk());
  EXPECT_EQ(schema.FullName(), response->name());
}

TEST(SchemaAdminClient, CreateSchemaProtobuf) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();
  Schema const schema("test-project", "test-schema");

  auto constexpr kExpectedText = R"pb(
    parent: "projects/test-project"
    schema_id: "test-schema"
    schema { type: PROTOCOL_BUFFER definition: "test-only-invalid-protobuf" }
  )pb";
  google::pubsub::v1::CreateSchemaRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &expected));

  EXPECT_CALL(*mock, CreateSchema)
      .WillOnce([&](google::pubsub::v1::CreateSchemaRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(expected));
        google::pubsub::v1::Schema response = r.schema();
        response.set_name(schema.FullName());
        return make_status_or(response);
      });
  SchemaAdminClient client(mock);
  auto const response =
      client.CreateProtobufSchema(schema, "test-only-invalid-protobuf");
  EXPECT_THAT(response, IsOk());
  EXPECT_EQ(schema.FullName(), response->name());
}

TEST(SchemaAdminClient, GetSchemaDefault) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();
  Schema const schema("test-project", "test-schema");

  auto constexpr kExpectedText = R"pb(
    name: "projects/test-project/schemas/test-schema"
    view: BASIC
  )pb";
  google::pubsub::v1::GetSchemaRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &request));

  EXPECT_CALL(*mock, GetSchema)
      .WillOnce([&](google::pubsub::v1::GetSchemaRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(request));
        google::pubsub::v1::Schema response;
        response.set_name(schema.FullName());
        return make_status_or(response);
      });
  SchemaAdminClient client(mock);
  auto const response = client.GetSchema(schema);
  EXPECT_THAT(response, IsOk());
  EXPECT_EQ(schema.FullName(), response->name());
}

TEST(SchemaAdminClient, GetSchemaFull) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();
  Schema const schema("test-project", "test-schema");

  auto constexpr kExpectedText = R"pb(
    name: "projects/test-project/schemas/test-schema"
    view: FULL
  )pb";
  google::pubsub::v1::GetSchemaRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &request));

  EXPECT_CALL(*mock, GetSchema)
      .WillOnce([&](google::pubsub::v1::GetSchemaRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(request));
        google::pubsub::v1::Schema response;
        response.set_name(schema.FullName());
        return make_status_or(response);
      });
  SchemaAdminClient client(mock);
  auto const response = client.GetSchema(schema, google::pubsub::v1::FULL);
  EXPECT_THAT(response, IsOk());
  EXPECT_EQ(schema.FullName(), response->name());
}

TEST(SchemaAdminClient, ListSchemasDefault) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();
  Schema const s1("test-project", "schema-1");
  Schema const s2("test-project", "schema-2");

  auto constexpr kExpectedText = R"pb(
    parent: "projects/test-project"
    view: BASIC
  )pb";
  google::pubsub::v1::ListSchemasRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &request));

  EXPECT_CALL(*mock, ListSchemas)
      .WillOnce([&](google::pubsub::v1::ListSchemasRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(request));
        return internal::MakePaginationRange<ListSchemasRange>(
            google::pubsub::v1::ListSchemasRequest{},
            [&](google::pubsub::v1::ListSchemasRequest const&) {
              google::pubsub::v1::ListSchemasResponse response;
              response.add_schemas()->set_name(s1.FullName());
              response.add_schemas()->set_name(s2.FullName());
              return make_status_or(response);
            },
            [](google::pubsub::v1::ListSchemasResponse const& r) {
              std::vector<google::pubsub::v1::Schema> items;
              for (auto const& t : r.schemas()) items.push_back(t);
              return items;
            });
      });
  SchemaAdminClient client(mock);
  std::vector<std::string> names;
  for (auto const& t : client.ListSchemas("test-project")) {
    ASSERT_THAT(t, IsOk());
    names.push_back(t->name());
  }
  EXPECT_THAT(names, ElementsAre(s1.FullName(), s2.FullName()));
}

TEST(SchemaAdminClient, ListSchemasFull) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();
  Schema const s1("test-project", "schema-1");
  Schema const s2("test-project", "schema-2");

  auto constexpr kExpectedText = R"pb(
    parent: "projects/test-project" view: FULL
  )pb";
  google::pubsub::v1::ListSchemasRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &request));

  EXPECT_CALL(*mock, ListSchemas)
      .WillOnce([&](google::pubsub::v1::ListSchemasRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(request));
        return internal::MakePaginationRange<ListSchemasRange>(
            google::pubsub::v1::ListSchemasRequest{},
            [&](google::pubsub::v1::ListSchemasRequest const&) {
              google::pubsub::v1::ListSchemasResponse response;
              response.add_schemas()->set_name(s1.FullName());
              response.add_schemas()->set_name(s2.FullName());
              return make_status_or(response);
            },
            [](google::pubsub::v1::ListSchemasResponse const& r) {
              std::vector<google::pubsub::v1::Schema> items;
              for (auto const& t : r.schemas()) items.push_back(t);
              return items;
            });
      });
  SchemaAdminClient client(mock);
  std::vector<std::string> names;
  for (auto const& t :
       client.ListSchemas("test-project", google::pubsub::v1::FULL)) {
    ASSERT_THAT(t, IsOk());
    names.push_back(t->name());
  }
  EXPECT_THAT(names, ElementsAre(s1.FullName(), s2.FullName()));
}

TEST(SchemaAdminClient, DeleteSchema) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();
  Schema const schema("test-project", "test-schema");

  auto constexpr kExpectedText = R"pb(
    name: "projects/test-project/schemas/test-schema"
  )pb";
  google::pubsub::v1::DeleteSchemaRequest request;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &request));

  EXPECT_CALL(*mock, DeleteSchema)
      .WillOnce([&](google::pubsub::v1::DeleteSchemaRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(request));
        return Status{};
      });
  SchemaAdminClient client(mock);
  auto const response = client.DeleteSchema(schema);
  EXPECT_THAT(response, IsOk());
}

TEST(SchemaAdminClient, ValidateSchemaAvro) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();

  auto constexpr kExpectedText = R"pb(
    parent: "projects/test-project"
    schema { type: AVRO definition: "test-only-invalid-avro" }
  )pb";
  google::pubsub::v1::ValidateSchemaRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &expected));

  EXPECT_CALL(*mock, ValidateSchema)
      .WillOnce([&](google::pubsub::v1::ValidateSchemaRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(expected));
        google::pubsub::v1::ValidateSchemaResponse response;
        return make_status_or(response);
      });
  SchemaAdminClient client(mock);
  auto const response =
      client.ValidateAvroSchema("test-project", "test-only-invalid-avro");
  EXPECT_THAT(response, IsOk());
}

TEST(SchemaAdminClient, ValidateSchemaProtobuf) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();

  auto constexpr kExpectedText = R"pb(
    parent: "projects/test-project"
    schema { type: PROTOCOL_BUFFER definition: "test-only-invalid-protobuf" }
  )pb";
  google::pubsub::v1::ValidateSchemaRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &expected));

  EXPECT_CALL(*mock, ValidateSchema)
      .WillOnce([&](google::pubsub::v1::ValidateSchemaRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(expected));
        google::pubsub::v1::ValidateSchemaResponse response;
        return make_status_or(response);
      });
  SchemaAdminClient client(mock);
  auto const response = client.ValidateProtobufSchema(
      "test-project", "test-only-invalid-protobuf");

  EXPECT_THAT(response, IsOk());
}

TEST(SchemaAdminClient, ValidateMessageWithNamedSchema) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();

  auto constexpr kExpectedText = R"pb(
    parent: "projects/test-project"
    name: "projects/test-project/schemas/test-schema"
    encoding: BINARY
    message: "test-only-invalid-message"
  )pb";
  google::pubsub::v1::ValidateMessageRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &expected));

  EXPECT_CALL(*mock, ValidateMessage)
      .WillOnce([&](google::pubsub::v1::ValidateMessageRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(expected));
        google::pubsub::v1::ValidateMessageResponse response;
        return make_status_or(response);
      });
  SchemaAdminClient client(mock);
  auto const response = client.ValidateMessageWithNamedSchema(
      google::pubsub::v1::BINARY, "test-only-invalid-message",
      Schema("test-project", "test-schema"));
  EXPECT_THAT(response, IsOk());
}

TEST(SchemaAdminClient, ValidateMessageWithAvro) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();

  auto constexpr kExpectedText = R"pb(
    parent: "projects/test-project"
    encoding: BINARY
    message: "test-only-invalid-message"
    schema { type: AVRO definition: "test-only-invalid-schema" }
  )pb";
  google::pubsub::v1::ValidateMessageRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &expected));

  EXPECT_CALL(*mock, ValidateMessage)
      .WillOnce([&](google::pubsub::v1::ValidateMessageRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(expected));
        google::pubsub::v1::ValidateMessageResponse response;
        return make_status_or(response);
      });
  SchemaAdminClient client(mock);
  auto const response = client.ValidateMessageWithAvro(
      google::pubsub::v1::BINARY, "test-only-invalid-message", "test-project",
      "test-only-invalid-schema");
  EXPECT_THAT(response, IsOk());
}

TEST(SchemaAdminClient, ValidateMessageWithProtobuf) {
  auto mock = std::make_shared<pubsub::MockSchemaAdminConnection>();

  auto constexpr kExpectedText = R"pb(
    parent: "projects/test-project"
    encoding: BINARY
    message: "test-only-invalid-message"
    schema { type: PROTOCOL_BUFFER definition: "test-only-invalid-schema" }
  )pb";
  google::pubsub::v1::ValidateMessageRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedText, &expected));

  EXPECT_CALL(*mock, ValidateMessage)
      .WillOnce([&](google::pubsub::v1::ValidateMessageRequest const& r) {
        EXPECT_THAT(r, IsProtoEqual(expected));
        google::pubsub::v1::ValidateMessageResponse response;
        return make_status_or(response);
      });
  SchemaAdminClient client(mock);
  auto const response = client.ValidateMessageWithProtobuf(
      google::pubsub::v1::BINARY, "test-only-invalid-message", "test-project",
      "test-only-invalid-schema");
  EXPECT_THAT(response, IsOk());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google
