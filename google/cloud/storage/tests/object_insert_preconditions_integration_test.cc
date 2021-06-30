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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::IsEmpty;
using ::testing::Not;

struct TestParam {
  absl::optional<std::string> rest_config;
  Fields fields;
};

class ObjectInsertPreconditionsIntegrationTest
    : public ::google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<TestParam> {
 protected:
  ObjectInsertPreconditionsIntegrationTest()
      : config_("GOOGLE_CLOUD_CPP_STORAGE_REST_CONFIG",
                GetParam().rest_config) {}

  void SetUp() override {
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");

    ASSERT_THAT(bucket_name_, Not(IsEmpty()));
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
  google::cloud::testing_util::ScopedEnvironment config_;
};

TEST_P(ObjectInsertPreconditionsIntegrationTest, IfGenerationMatchSuccess) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto insert = client->InsertObject(bucket_name(), object_name, expected_text,
                                     GetParam().fields,
                                     IfGenerationMatch(meta->generation()));
  ASSERT_THAT(insert, IsOk());

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(insert->generation()));
  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_P(ObjectInsertPreconditionsIntegrationTest, IfGenerationMatchFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto insert = client->InsertObject(bucket_name(), object_name, expected_text,
                                     GetParam().fields,
                                     IfGenerationMatch(meta->generation() + 1));
  ASSERT_THAT(insert, StatusIs(StatusCode::kFailedPrecondition));

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_P(ObjectInsertPreconditionsIntegrationTest, IfGenerationNotMatchSuccess) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto insert = client->InsertObject(
      bucket_name(), object_name, expected_text, GetParam().fields,
      IfGenerationNotMatch(meta->generation() + 1));
  ASSERT_THAT(insert, IsOk());

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(insert->generation()));
  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_P(ObjectInsertPreconditionsIntegrationTest, IfGenerationNotMatchFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto insert = client->InsertObject(bucket_name(), object_name, expected_text,
                                     GetParam().fields,
                                     IfGenerationNotMatch(meta->generation()));
  ASSERT_THAT(insert, StatusIs(AnyOf(StatusCode::kFailedPrecondition,
                                     StatusCode::kAborted)));

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_P(ObjectInsertPreconditionsIntegrationTest, IfMetagenerationMatchSuccess) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto insert = client->InsertObject(
      bucket_name(), object_name, expected_text, GetParam().fields,
      IfMetagenerationMatch(meta->metageneration()));
  ASSERT_THAT(insert, IsOk());

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(insert->generation()));
  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_P(ObjectInsertPreconditionsIntegrationTest, IfMetagenerationMatchFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto insert = client->InsertObject(
      bucket_name(), object_name, expected_text, GetParam().fields,
      IfMetagenerationMatch(meta->metageneration() + 1));
  ASSERT_THAT(insert, StatusIs(StatusCode::kFailedPrecondition));

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_P(ObjectInsertPreconditionsIntegrationTest,
       IfMetagenerationNotMatchSuccess) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto insert = client->InsertObject(
      bucket_name(), object_name, expected_text, GetParam().fields,
      IfMetagenerationNotMatch(meta->metageneration() + 1));
  ASSERT_THAT(insert, IsOk());

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(insert->generation()));
  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_P(ObjectInsertPreconditionsIntegrationTest,
       IfMetagenerationNotMatchFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto insert = client->InsertObject(
      bucket_name(), object_name, expected_text, GetParam().fields,
      IfMetagenerationNotMatch(meta->metageneration()));
  ASSERT_THAT(insert, StatusIs(AnyOf(StatusCode::kFailedPrecondition,
                                     StatusCode::kAborted)));

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

INSTANTIATE_TEST_SUITE_P(XmlEnabledAndUsed,
                         ObjectInsertPreconditionsIntegrationTest,
                         ::testing::Values(TestParam{absl::nullopt,
                                                     Fields("")}));
INSTANTIATE_TEST_SUITE_P(XmlEnabledNotUsed,
                         ObjectInsertPreconditionsIntegrationTest,
                         ::testing::Values(TestParam{absl::nullopt, Fields()}));
INSTANTIATE_TEST_SUITE_P(XmlDisabled, ObjectInsertPreconditionsIntegrationTest,
                         ::testing::Values(TestParam{"disable-xml", Fields()}));

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
