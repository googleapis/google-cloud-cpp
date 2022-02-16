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
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::IsEmpty;
using ::testing::Not;

// When GOOGLE_CLOUD_CPP_HAVE_GRPC is not set these tests compile, but they
// actually just run against the regular GCS REST API. That is fine.
class GrpcBucketMetadataIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(GrpcBucketMetadataIntegrationTest, ObjectMetadataCRUD) {
  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");
  // TODO(#7257) - restore gRPC integration tests against production
  if (!UsingEmulator()) GTEST_SKIP();

  auto const project_name = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_name, Not(IsEmpty()))
      << "GOOGLE_CLOUD_PROJECT is not set";

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto insert = client->CreateBucketForProject(bucket_name, project_name,
                                               BucketMetadata());
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);
  EXPECT_EQ(insert->name(), bucket_name);

  auto get = client->GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(get);

  // There are too many fields with missing values in the testbench, just test
  // some interesting ones:
  EXPECT_EQ(get->name(), insert->name());
  EXPECT_EQ(get->metageneration(), insert->metageneration());
  EXPECT_EQ(get->time_created(), insert->time_created());
  EXPECT_EQ(get->updated(), insert->updated());
  EXPECT_EQ(get->rpo(), insert->rpo());
  EXPECT_EQ(get->location(), insert->location());
  EXPECT_EQ(get->location_type(), insert->location_type());
  EXPECT_EQ(get->storage_class(), insert->storage_class());

  auto delete_status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(delete_status);

  auto post_delete = client->GetBucketMetadata(bucket_name);
  EXPECT_THAT(post_delete, StatusIs(StatusCode::kNotFound));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
