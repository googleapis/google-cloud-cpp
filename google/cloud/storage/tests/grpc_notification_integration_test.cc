// Copyright 2022 Google LLC
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

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;

// When GOOGLE_CLOUD_CPP_HAVE_GRPC is not set these tests compile, but they
// actually just run against the regular GCS REST API. That is fine.
class GrpcNotificationIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(GrpcNotificationIntegrationTest, NotificationCRUD) {
  // TODO(#5673) - enable in production
  if (!UsingEmulator()) GTEST_SKIP();
  auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_id, Not(IsEmpty())) << "GOOGLE_CLOUD_PROJECT is not set";
  auto const topic_name = google::cloud::internal::GetEnv(
                              "GOOGLE_CLOUD_CPP_STORAGE_TEST_TOPIC_NAME")
                              .value_or("");
  ASSERT_THAT(topic_name, Not(IsEmpty()));

  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto metadata =
      client->CreateBucketForProject(bucket_name, project_id, BucketMetadata());
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");

  auto marker = google::cloud::internal::Sample(
      generator_, 16, "abcdefghijklmnopqrstuvwxyz0123456789");
  auto create = client->CreateNotification(
      bucket_name, topic_name,
      NotificationMetadata().upsert_custom_attributes("test-key", marker));
  ASSERT_STATUS_OK(create);
  EXPECT_THAT(create->custom_attributes(), Contains(Pair("test-key", marker)));

  auto get = client->GetNotification(bucket_name, create->id());
  ASSERT_STATUS_OK(get);
  EXPECT_EQ(*create, *get);

  auto list = client->ListNotifications(bucket_name);
  ASSERT_STATUS_OK(list);
  EXPECT_THAT(*list, ElementsAre(*get));

  auto delete_status = client->DeleteNotification(bucket_name, create->id());
  ASSERT_STATUS_OK(delete_status);

  auto not_found = client->GetNotification(bucket_name, create->id());
  EXPECT_THAT(not_found, StatusIs(StatusCode::kNotFound));

  auto empty_list = client->ListNotifications(bucket_name);
  ASSERT_STATUS_OK(empty_list);
  EXPECT_THAT(*empty_list, IsEmpty());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
