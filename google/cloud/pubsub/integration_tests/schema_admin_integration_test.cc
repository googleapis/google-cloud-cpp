// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/schema_stub.h"
#include "google/cloud/pubsub/schema.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Not;

bool UsingEmulator() {
  return google::cloud::internal::GetEnv("PUBSUB_EMULATOR_HOST").has_value();
}

// TODO(#5706) - use the user-facing SchemaClient class. For now, as we
//      bootstrap the classes, just use the stub.
TEST(SchemaAdminIntegrationTest, SchemaCRUD) {
  if (!UsingEmulator()) GTEST_SKIP();  // TODO(#5706)
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());

  auto schema_admin = CreateDefaultSchemaStub(pubsub::ConnectionOptions{}, 0);

  auto constexpr kTestAvroSchema = R"js({
     "type": "record",
     "namespace": "com.example",
     "name": "TestSchema",
     "fields": [
       { "name": "sensorId", "type": "string" },
       { "name": "value", "type": "double" }
     ]
  })js";
  auto constexpr kTestMessage =
      R"js({ {"sensorId", "temp0"}, {"value", "42"})js";

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const schema_id = pubsub_testing::RandomSchemaId(generator);
  {
    grpc::ClientContext context;
    google::pubsub::v1::CreateSchemaRequest request;
    request.set_parent("projects/" + project_id);
    request.set_schema_id(schema_id);
    request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
    request.mutable_schema()->set_definition(kTestAvroSchema);
    auto schema = schema_admin->CreateSchema(context, request);
    ASSERT_THAT(schema, IsOk());
  }

  {
    grpc::ClientContext context;
    google::pubsub::v1::GetSchemaRequest request;
    request.set_name(pubsub::Schema(project_id, schema_id).FullName());
    auto schema = schema_admin->GetSchema(context, request);
    EXPECT_THAT(schema, IsOk());
  }

  {
    grpc::ClientContext context;
    google::pubsub::v1::ListSchemasRequest request;
    request.set_parent("projects/" + project_id);
    auto response = schema_admin->ListSchemas(context, request);
    EXPECT_THAT(response, IsOk());
  }

  {
    grpc::ClientContext context;
    google::pubsub::v1::ValidateSchemaRequest request;
    request.set_parent("projects/" + project_id);
    request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
    request.mutable_schema()->set_definition(kTestAvroSchema);
    auto result = schema_admin->ValidateSchema(context, request);
    EXPECT_THAT(result, IsOk());
  }

  {
    grpc::ClientContext context;
    google::pubsub::v1::ValidateMessageRequest request;
    request.set_parent("projects/" + project_id);
    request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
    request.mutable_schema()->set_definition(kTestAvroSchema);
    request.set_encoding(google::pubsub::v1::JSON);
    request.set_message(kTestMessage);
    (void)schema_admin->ValidateMessage(context, request);
    // TODO(#5706) - still don't know how to create a valid message.
    // EXPECT_THAT(result, IsOk());
  }

  {
    grpc::ClientContext context;
    google::pubsub::v1::DeleteSchemaRequest request;
    request.set_name(pubsub::Schema(project_id, schema_id).FullName());
    auto result = schema_admin->DeleteSchema(context, request);
    EXPECT_THAT(result, IsOk());
  }
}

TEST(SchemaAdminIntegrationTest, CreateSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = CreateDefaultSchemaStub(pubsub::ConnectionOptions{}, 0);
  grpc::ClientContext context;
  google::pubsub::v1::CreateSchemaRequest request;
  auto response = schema_admin->CreateSchema(context, request);
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, GetSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = CreateDefaultSchemaStub(pubsub::ConnectionOptions{}, 0);
  grpc::ClientContext context;
  google::pubsub::v1::GetSchemaRequest request;
  auto response = schema_admin->GetSchema(context, request);
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, ListSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = CreateDefaultSchemaStub(pubsub::ConnectionOptions{}, 0);
  grpc::ClientContext context;
  google::pubsub::v1::ListSchemasRequest request;
  auto response = schema_admin->ListSchemas(context, request);
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, DeleteSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = CreateDefaultSchemaStub(pubsub::ConnectionOptions{}, 0);
  grpc::ClientContext context;
  google::pubsub::v1::DeleteSchemaRequest request;
  auto response = schema_admin->DeleteSchema(context, request);
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, ValidateSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = CreateDefaultSchemaStub(pubsub::ConnectionOptions{}, 0);
  grpc::ClientContext context;
  google::pubsub::v1::DeleteSchemaRequest request;
  auto response = schema_admin->DeleteSchema(context, request);
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, ValidateMessage) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = CreateDefaultSchemaStub(pubsub::ConnectionOptions{}, 0);
  grpc::ClientContext context;
  google::pubsub::v1::ValidateMessageRequest request;
  auto response = schema_admin->ValidateMessage(context, request);
  EXPECT_THAT(response, Not(IsOk()));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
