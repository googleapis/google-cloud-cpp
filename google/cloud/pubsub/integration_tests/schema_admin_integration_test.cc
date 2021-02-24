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
#include "google/cloud/pubsub/experimental/schema_admin_client.h"
#include "google/cloud/pubsub/testing/random_names.h"
#include "google/cloud/pubsub/testing/test_retry_policies.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
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
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::Not;

TEST(SchemaAdminIntegrationTest, SchemaCRUD) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_FALSE(project_id.empty());

  auto schema_admin =
      SchemaAdminClient(MakeSchemaAdminConnection(pubsub::ConnectionOptions{}));

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
  auto schema = Schema(project_id, schema_id);

  auto create = schema_admin.CreateAvroSchema(schema, kTestAvroSchema);
  ASSERT_THAT(create, IsOk());
  EXPECT_EQ(create->name(), schema.FullName());

  auto get = schema_admin.GetSchema(schema, google::pubsub::v1::FULL);
  ASSERT_THAT(get, IsOk());
  EXPECT_THAT(*get, IsProtoEqual(*create));

  std::vector<std::string> names;
  for (auto&& r : schema_admin.ListSchemas(project_id)) {
    EXPECT_THAT(r, IsOk());
    if (!r.ok()) break;
    names.push_back(std::move(*r->mutable_name()));
  }
  EXPECT_THAT(names, Contains(Schema(project_id, schema_id).FullName()));

  auto valid_schema =
      schema_admin.ValidateAvroSchema(project_id, kTestAvroSchema);
  EXPECT_THAT(valid_schema, IsOk());

  auto valid_message = schema_admin.ValidateMessageWithNamedSchema(
      google::pubsub::v1::JSON, "not-a-valid-message", schema);
  EXPECT_THAT(valid_message, StatusIs(StatusCode::kInvalidArgument));

  auto deleted = schema_admin.DeleteSchema(schema);
  EXPECT_THAT(deleted, IsOk());
}

TEST(SchemaAdminIntegrationTest, CreateSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = SchemaAdminClient(MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy()));
  google::pubsub::v1::CreateSchemaRequest request;
  auto response = schema_admin.CreateSchema(request);
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, GetSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = SchemaAdminClient(MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy()));
  auto response = schema_admin.GetSchema(
      Schema("--invalid-project--", "--invalid-schema--"));
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, ListSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = SchemaAdminClient(MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy()));
  auto response = schema_admin.ListSchemas("--invalid-project");
  auto i = response.begin();
  ASSERT_FALSE(i == response.end());
  EXPECT_THAT(*i, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, DeleteSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = SchemaAdminClient(MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy()));
  auto response = schema_admin.DeleteSchema(
      Schema("--invalid-project--", "--invalid-schema--"));
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, ValidateSchema) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = SchemaAdminClient(MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy()));
  auto response = schema_admin.ValidateSchema("--invalid-project--",
                                              google::pubsub::v1::Schema{});
  EXPECT_THAT(response, Not(IsOk()));
}

TEST(SchemaAdminIntegrationTest, ValidateMessage) {
  ScopedEnvironment env("PUBSUB_EMULATOR_HOST", "localhost:1");
  auto schema_admin = SchemaAdminClient(MakeSchemaAdminConnection(
      pubsub::ConnectionOptions{}, TestRetryPolicy(), TestBackoffPolicy()));
  google::pubsub::v1::ValidateMessageRequest request;
  auto response = schema_admin.ValidateMessage(request);
  EXPECT_THAT(response, Not(IsOk()));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_experimental
}  // namespace cloud
}  // namespace google
