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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::IsEmpty;
using ::testing::Not;

class ObjectReadPreconditionsIntegrationTest
    : public ::google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  ObjectReadPreconditionsIntegrationTest() = default;

  void SetUp() override {
    bucket_name_ =
        GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");

    ASSERT_THAT(bucket_name_, Not(IsEmpty()));
  }

  std::string const& bucket_name() const { return bucket_name_; }

 private:
  std::string bucket_name_;
};

TEST_F(ObjectReadPreconditionsIntegrationTest, IfGenerationMatchSuccess) {
  auto client = MakeIntegrationTestClient();
  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();

  auto meta = client.InsertObject(bucket_name(), object_name, expected_text,
                                  IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());
  ScheduleForDelete(*meta);

  auto reader = client.ReadObject(bucket_name(), object_name,
                                  IfGenerationMatch(meta->generation()));
  reader.Close();
  EXPECT_THAT(reader.status(), IsOk());
}

TEST_F(ObjectReadPreconditionsIntegrationTest, IfGenerationMatchFailure) {
  auto client = MakeIntegrationTestClient();

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client.InsertObject(bucket_name(), object_name, expected_text,
                                  IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());
  ScheduleForDelete(*meta);

  auto reader = client.ReadObject(bucket_name(), object_name,
                                  IfGenerationMatch(meta->generation() + 1));
  reader.Close();
  EXPECT_THAT(reader.status(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST_F(ObjectReadPreconditionsIntegrationTest, IfGenerationNotMatchSuccess) {
  auto client = MakeIntegrationTestClient();

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client.InsertObject(bucket_name(), object_name, expected_text,
                                  IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());
  ScheduleForDelete(*meta);

  auto reader = client.ReadObject(bucket_name(), object_name,
                                  IfGenerationNotMatch(meta->generation() + 1));
  reader.Close();
  EXPECT_THAT(reader.status(), IsOk());
}

TEST_F(ObjectReadPreconditionsIntegrationTest, IfGenerationNotMatchFailure) {
  auto client = MakeIntegrationTestClient();

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client.InsertObject(bucket_name(), object_name, expected_text,
                                  IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());
  ScheduleForDelete(*meta);

  auto reader = client.ReadObject(bucket_name(), object_name,
                                  IfGenerationNotMatch(meta->generation()));
  reader.Close();
  // GCS returns different error codes depending on the API used by the client
  // library. This is a bit terrible, but in this context we just want to verify
  // that (a) the pre-condition was set, and (b) it prevented the action from
  // taking place.
  EXPECT_THAT(reader.status(), StatusIs(AnyOf(StatusCode::kFailedPrecondition,
                                              StatusCode::kAborted)));
}

TEST_F(ObjectReadPreconditionsIntegrationTest, IfMetagenerationMatchSuccess) {
  auto client = MakeIntegrationTestClient();

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client.InsertObject(bucket_name(), object_name, expected_text,
                                  IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());
  ScheduleForDelete(*meta);

  auto reader =
      client.ReadObject(bucket_name(), object_name,
                        IfMetagenerationMatch(meta->metageneration()));
  reader.Close();
  EXPECT_THAT(reader.status(), IsOk());
}

TEST_F(ObjectReadPreconditionsIntegrationTest, IfMetagenerationMatchFailure) {
  auto client = MakeIntegrationTestClient();

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client.InsertObject(bucket_name(), object_name, expected_text,
                                  IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());
  ScheduleForDelete(*meta);

  auto reader =
      client.ReadObject(bucket_name(), object_name,
                        IfMetagenerationMatch(meta->metageneration() + 1));
  reader.Close();
  EXPECT_THAT(reader.status(), StatusIs(StatusCode::kFailedPrecondition));
}

TEST_F(ObjectReadPreconditionsIntegrationTest,
       IfMetagenerationNotMatchSuccess) {
  auto client = MakeIntegrationTestClient();

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client.InsertObject(bucket_name(), object_name, expected_text,
                                  IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());
  ScheduleForDelete(*meta);

  auto reader =
      client.ReadObject(bucket_name(), object_name,
                        IfMetagenerationNotMatch(meta->generation() + 1));
  reader.Close();
  EXPECT_THAT(reader.status(), IsOk());
}

TEST_F(ObjectReadPreconditionsIntegrationTest,
       IfMetagenerationNotMatchFailure) {
  auto client = MakeIntegrationTestClient();

  auto const object_name = MakeRandomObjectName();
  auto const expected_text = LoremIpsum();
  auto meta = client.InsertObject(bucket_name(), object_name, expected_text,
                                  IfGenerationMatch(0));
  ASSERT_THAT(meta, IsOk());
  ScheduleForDelete(*meta);

  auto reader =
      client.ReadObject(bucket_name(), object_name,
                        IfMetagenerationNotMatch(meta->metageneration()));
  reader.Close();
  // GCS returns different error codes depending on the API used by the client
  // library. This is a bit terrible, but in this context we just want to verify
  // that (a) the pre-condition was set, and (b) it prevented the action from
  // taking place.
  EXPECT_THAT(reader.status(), StatusIs(AnyOf(StatusCode::kFailedPrecondition,
                                              StatusCode::kAborted)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
