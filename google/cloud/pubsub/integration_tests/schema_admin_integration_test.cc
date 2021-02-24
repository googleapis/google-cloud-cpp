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

#include "google/cloud/pubsub/experimental/schema.h"
#include "google/cloud/pubsub/experimental/schema_admin_connection.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::pubsub_experimental::Schema;
using ::google::cloud::pubsub_testing::TestBackoffPolicy;
using ::google::cloud::pubsub_testing::TestRetryPolicy;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
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

  auto schema_admin = pubsub_experimental::MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{});

  auto constexpr kTestAvroSchema = R"js({
     "type": "record",
     "namespace": "com.example",
     "name": "TestSchema",
     "fields": [
       { "name": "sensorId", "type": "string" },
       { "name": "value", "type": "double" }
     ]
  })js";

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  auto const schema_id = pubsub_testing::RandomSchemaId(generator);
  {
    google::pubsub::v1::CreateSchemaRequest request;
    request.set_parent("projects/" + project_id);
    request.set_schema_id(schema_id);
    request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
    request.mutable_schema()->set_definition(kTestAvroSchema);
    auto schema = schema_admin->CreateSchema(request);
    ASSERT_THAT(schema, IsOk());
  }

  {
    google::pubsub::v1::GetSchemaRequest request;
    request.set_name(Schema(project_id, schema_id).FullName());
    auto schema = schema_admin->GetSchema(request);
    EXPECT_THAT(schema, IsOk());
  }

  {
    google::pubsub::v1::ListSchemasRequest request;
    request.set_parent("projects/" + project_id);
    std::vector<std::string> names;
    for (auto&& r : schema_admin->ListSchemas(request)) {
      EXPECT_THAT(r, IsOk());
      if (!r.ok()) break;
      names.push_back(std::move(*r->mutable_name()));
    }
    EXPECT_THAT(names, Contains(Schema(project_id, schema_id).FullName()));
  }

  {
    google::pubsub::v1::ValidateSchemaRequest request;
    request.set_parent("projects/" + project_id);
    request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
    request.mutable_schema()->set_definition(kTestAvroSchema);
    auto result = schema_admin->ValidateSchema(request);
    EXPECT_THAT(result, IsOk());
  }

  {
    google::pubsub::v1::ValidateMessageRequest request;
    request.set_parent("projects/" + project_id);
    request.mutable_schema()->set_type(google::pubsub::v1::Schema::AVRO);
    request.mutable_schema()->set_definition(kTestAvroSchema);
    request.set_encoding(google::pubsub::v1::JSON);
    request.set_message("not-a-valid-message");
    auto result = schema_admin->ValidateMessage(request);
    EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument));
  }

  {
    google::pubsub::v1::DeleteSchemaRequest request;
    request.set_name(Schema(project_id, schema_id).FullName());
    auto result = schema_admin->DeleteSchema(request);
    EXPECT_THAT(result, IsOk());
  }
}

TEST(SchemaAdminIntegrationTest, CreateSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = pubsub_experimental::MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy());
  google::pubsub::v1::CreateSchemaRequest request;
  auto response = schema_admin->CreateSchema(request);
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, GetSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = pubsub_experimental::MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy());
  google::pubsub::v1::GetSchemaRequest request;
  auto response = schema_admin->GetSchema(request);
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, ListSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = pubsub_experimental::MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy());
  google::pubsub::v1::ListSchemasRequest request;
  auto response = schema_admin->ListSchemas(request);
  auto i = response.begin();
  ASSERT_FALSE(i == response.end());
  EXPECT_THAT(*i, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, DeleteSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = pubsub_experimental::MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy());
  google::pubsub::v1::DeleteSchemaRequest request;
  auto response = schema_admin->DeleteSchema(request);
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, ValidateSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = pubsub_experimental::MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy());
  google::pubsub::v1::ValidateSchemaRequest request;
  auto response = schema_admin->ValidateSchema(request);
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, ValidateMessage) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = pubsub_experimental::MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy());
  google::pubsub::v1::ValidateMessageRequest request;
  auto response = schema_admin->ValidateMessage(request);
  EXPECT_THAT(response, Not(IsOk()));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google
