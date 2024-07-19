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

#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <iterator>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Not;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class GrpcBucketMetadataIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(GrpcBucketMetadataIntegrationTest, BucketMetadataCRUD) {
  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");

  auto const project_name = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_name, Not(IsEmpty()))
      << "GOOGLE_CLOUD_PROJECT is not set";

  auto client = MakeIntegrationTestClient();

  auto bucket_name = MakeRandomBucketName();
  auto insert = client.CreateBucketForProject(bucket_name, project_name,
                                              BucketMetadata());
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);
  EXPECT_EQ(insert->name(), bucket_name);

  auto get = client.GetBucketMetadata(bucket_name);
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

  // We need to set the retention policy or the request to lock the retention
  // policy (see below) will fail.
  auto patch = client.PatchBucket(
      bucket_name, BucketMetadataPatchBuilder{}
                       .SetLabel("l0", "k0")
                       .SetRetentionPolicy(std::chrono::seconds(30)));
  ASSERT_STATUS_OK(patch);
  EXPECT_THAT(patch->labels(), ElementsAre(Pair("l0", "k0")));

  auto updated = client.UpdateBucket(
      patch->name(), BucketMetadata(*patch).upsert_label("l1", "test-value"));
  ASSERT_STATUS_OK(updated);
  EXPECT_THAT(updated->labels(),
              UnorderedElementsAre(Pair("l0", "k0"), Pair("l1", "test-value")));

  auto locked =
      client.LockBucketRetentionPolicy(bucket_name, updated->metageneration());
  ASSERT_STATUS_OK(locked);
  ASSERT_TRUE(updated->has_retention_policy());
  ASSERT_TRUE(locked->has_retention_policy());
  EXPECT_FALSE(updated->retention_policy().is_locked);
  EXPECT_TRUE(locked->retention_policy().is_locked);

  // Create a second bucket to make the list more interesting.
  auto bucket_name_2 = MakeRandomBucketName();
  auto insert_2 = client.CreateBucketForProject(bucket_name_2, project_name,
                                                BucketMetadata());
  ASSERT_STATUS_OK(insert_2);
  ScheduleForDelete(*insert_2);

  std::vector<std::string> names;
  for (auto const& b : client.ListBucketsForProject(project_name)) {
    ASSERT_STATUS_OK(b);
    names.push_back(b->name());
  }
  EXPECT_THAT(names, IsSupersetOf({bucket_name, bucket_name_2}));

  auto policy = client.GetNativeBucketIamPolicy(bucket_name);
  ASSERT_STATUS_OK(policy);

  std::vector<std::string> roles;
  std::transform(policy->bindings().begin(), policy->bindings().end(),
                 std::back_inserter(roles),
                 [](NativeIamBinding const& b) { return b.role(); });
  EXPECT_THAT(roles, IsSupersetOf({"roles/storage.legacyBucketOwner",
                                   "roles/storage.legacyBucketReader"}));

  auto new_policy = *policy;
  policy = client.SetNativeBucketIamPolicy(bucket_name, new_policy);
  ASSERT_STATUS_OK(policy);

  auto permissions = client.TestBucketIamPermissions(
      bucket_name, {"storage.objects.list", "storage.buckets.update"});
  ASSERT_STATUS_OK(permissions);
  EXPECT_THAT(*permissions, Contains("storage.buckets.update"));

  auto delete_status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(delete_status);

  auto post_delete = client.GetBucketMetadata(bucket_name);
  EXPECT_THAT(post_delete, StatusIs(StatusCode::kNotFound));
}

TEST_F(GrpcBucketMetadataIntegrationTest, PatchLabels) {
  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");

  auto const project_name = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_name, Not(IsEmpty()))
      << "GOOGLE_CLOUD_PROJECT is not set";

  auto client = MakeIntegrationTestClient();
  auto bucket_name = MakeRandomBucketName();

  auto insert = client.CreateBucketForProject(bucket_name, project_name,
                                              BucketMetadata());
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);
  EXPECT_EQ(insert->name(), bucket_name);

  auto patch =
      client.PatchBucket(bucket_name, BucketMetadataPatchBuilder{}
                                          .SetLabel("test-key0", "v0")
                                          .SetLabel("test-key1", "v1")
                                          .SetLabel("test-key2", "v2"));
  ASSERT_STATUS_OK(patch);
  EXPECT_THAT(patch->labels(), AllOf(Contains(Pair("test-key0", "v0")),
                                     Contains(Pair("test-key1", "v1")),
                                     Contains(Pair("test-key2", "v2"))));

  patch = client.PatchBucket(bucket_name, BucketMetadataPatchBuilder{}
                                              .SetLabel("test-key0", "new-v0")
                                              .ResetLabel("test-key1")
                                              .SetLabel("test-key3", "v3"));
  ASSERT_STATUS_OK(patch);
  EXPECT_THAT(patch->labels(), AllOf(Contains(Pair("test-key0", "new-v0")),
                                     Not(Contains(Pair("test-key1", _))),
                                     Contains(Pair("test-key2", "v2")),
                                     Contains(Pair("test-key3", "v3"))));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
