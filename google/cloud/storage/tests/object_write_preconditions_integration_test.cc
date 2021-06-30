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

class ObjectWritePreconditionsIntegrationTest
    : public ::google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  ObjectWritePreconditionsIntegrationTest() = default;

  void SetUp() override {
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");

    ASSERT_THAT(bucket_name_, Not(IsEmpty()));
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

TEST_F(ObjectWritePreconditionsIntegrationTest, IfGenerationMatchSuccess) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto os = client->WriteObject(bucket_name(), object_name,
                                IfGenerationMatch(meta->generation()));
  os << expected_text;
  os.Close();
  ASSERT_THAT(os.metadata(), IsOk());

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(os.metadata()->generation()));
  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_F(ObjectWritePreconditionsIntegrationTest, IfGenerationMatchFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto os = client->WriteObject(bucket_name(), object_name,
                                IfGenerationMatch(meta->generation() + 1));
  os << expected_text;
  os.Close();
  ASSERT_THAT(os.metadata(), StatusIs(StatusCode::kFailedPrecondition));

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_F(ObjectWritePreconditionsIntegrationTest, IfGenerationNotMatchSuccess) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto os = client->WriteObject(bucket_name(), object_name,
                                IfGenerationNotMatch(meta->generation() + 1));
  os << expected_text;
  os.Close();
  ASSERT_THAT(os.metadata(), IsOk());

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(os.metadata()->generation()));
  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_F(ObjectWritePreconditionsIntegrationTest, IfGenerationNotMatchFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto os = client->WriteObject(bucket_name(), object_name,
                                IfGenerationNotMatch(meta->generation()));
  os << expected_text;
  os.Close();
  ASSERT_THAT(os.metadata(), StatusIs(AnyOf(StatusCode::kFailedPrecondition,
                                            StatusCode::kAborted)));

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_F(ObjectWritePreconditionsIntegrationTest, IfMetagenerationMatchSuccess) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto os = client->WriteObject(bucket_name(), object_name,
                                IfMetagenerationMatch(meta->metageneration()));
  os << expected_text;
  os.Close();
  ASSERT_THAT(os.metadata(), IsOk());

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(os.metadata()->generation()));
  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_F(ObjectWritePreconditionsIntegrationTest, IfMetagenerationMatchFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto os =
      client->WriteObject(bucket_name(), object_name,
                          IfMetagenerationMatch(meta->metageneration() + 1));
  os << expected_text;
  os.Close();
  ASSERT_THAT(os.metadata(), StatusIs(StatusCode::kFailedPrecondition));

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_F(ObjectWritePreconditionsIntegrationTest,
       IfMetagenerationNotMatchSuccess) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto os =
      client->WriteObject(bucket_name(), object_name,
                          IfMetagenerationNotMatch(meta->metageneration() + 1));
  os << expected_text;
  os.Close();
  ASSERT_THAT(os.metadata(), IsOk());

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(os.metadata()->generation()));
  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

TEST_F(ObjectWritePreconditionsIntegrationTest,
       IfMetagenerationNotMatchFailure) {
  StatusOr<Client> client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client->InsertObject(bucket_name(), object_name, expected_text,
                                   IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());

  auto os =
      client->WriteObject(bucket_name(), object_name,
                          IfMetagenerationNotMatch(meta->metageneration()));
  os << expected_text;
  os.Close();
  ASSERT_THAT(os.metadata(), StatusIs(AnyOf(StatusCode::kFailedPrecondition,
                                            StatusCode::kAborted)));

  (void)client->DeleteObject(bucket_name(), object_name,
                             Generation(meta->generation()));
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
