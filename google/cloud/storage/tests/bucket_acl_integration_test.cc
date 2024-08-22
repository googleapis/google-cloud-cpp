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
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <iterator>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::storage::testing::AclEntityNames;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;

class BucketAclIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    ASSERT_THAT(project_id_, Not(IsEmpty()))
        << "GOOGLE_CLOUD_PROJECT is not set";
  }

  std::string project_id() const { return project_id_; }

  std::string MakeEntityName() {
    // We always use the viewers for the project because it is known to exist.
    return "project-viewers-" + project_id_;
  }

  std::string project_id_ = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
};

TEST_F(BucketAclIntegrationTest, AclCRUD) {
  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeIntegrationTestClient(MakeBucketTestOptions());

  // Create a new bucket to run the test, with the "private" PredefinedAcl so
  // we know what the contents of the ACL will be.
  auto metadata = client.CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata(), PredefinedAcl("private"),
      Projection("full"));
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  // We always use "project-viewers-${project_id}" because it is known to exist.
  auto const viewers = "project-viewers-" + project_id();
  auto const owners = "project-owners-" + project_id();

  ASSERT_THAT(metadata->acl(), Not(IsEmpty()))
      << "Test aborted. Empty ACL returned from newly created bucket <"
      << bucket_name << "> even though we requested the <full> projection.";
  ASSERT_THAT(AclEntityNames(metadata->acl()), Not(Contains(viewers)))
      << "Test aborted. The bucket <" << bucket_name << "> has <" << viewers
      << "> in its ACL.  This is unexpected because the bucket was just"
      << " created with a predefine ACL which should preclude this result.";

  auto const existing_entity = metadata->acl().front();
  auto current_acl = client.ListBucketAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(*current_acl),
              Contains(existing_entity.entity()).Times(1));

  auto get_acl = client.GetBucketAcl(bucket_name, existing_entity.entity());
  ASSERT_STATUS_OK(get_acl);
  EXPECT_EQ(*get_acl, existing_entity);

  auto create_acl = client.CreateBucketAcl(bucket_name, viewers,
                                           BucketAccessControl::ROLE_READER());
  ASSERT_STATUS_OK(create_acl);

  current_acl = client.ListBucketAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(*current_acl),
              Contains(create_acl->entity()).Times(1));

  auto c2 = client.CreateBucketAcl(bucket_name, viewers,
                                   BucketAccessControl::ROLE_READER());
  ASSERT_STATUS_OK(c2);
  // There is no guarantee that the ETag remains unchanged, even if the
  // operation has no effect.  Reset the one field that might change.
  create_acl->set_etag(c2->etag());
  EXPECT_EQ(*create_acl, *c2);

  auto updated_acl = client.UpdateBucketAcl(
      bucket_name, BucketAccessControl().set_entity(viewers).set_role(
                       BucketAccessControl::ROLE_OWNER()));
  ASSERT_STATUS_OK(updated_acl);
  EXPECT_EQ(updated_acl->entity(), create_acl->entity());
  EXPECT_EQ(updated_acl->role(), BucketAccessControl::ROLE_OWNER());

  // "Updating" an entity that does not exist should create the entity
  auto delete_acl = client.DeleteBucketAcl(bucket_name, viewers);
  ASSERT_STATUS_OK(delete_acl);
  updated_acl = client.UpdateBucketAcl(
      bucket_name, BucketAccessControl().set_entity(viewers).set_role(
                       BucketAccessControl::ROLE_OWNER()));
  ASSERT_STATUS_OK(updated_acl);

  auto patched_acl =
      client.PatchBucketAcl(bucket_name, viewers,
                            BucketAccessControlPatchBuilder().set_role(
                                BucketAccessControl::ROLE_READER()));
  ASSERT_STATUS_OK(patched_acl);
  EXPECT_EQ(patched_acl->entity(), create_acl->entity());
  EXPECT_EQ(patched_acl->role(), BucketAccessControl::ROLE_READER());

  // "Patching" an entity that does not exist should create the entity
  delete_acl = client.DeleteBucketAcl(bucket_name, viewers);
  ASSERT_STATUS_OK(delete_acl);
  patched_acl =
      client.PatchBucketAcl(bucket_name, viewers,
                            BucketAccessControlPatchBuilder().set_role(
                                BucketAccessControl::ROLE_READER()));
  ASSERT_STATUS_OK(patched_acl);
  EXPECT_EQ(patched_acl->entity(), create_acl->entity());
  EXPECT_EQ(patched_acl->role(), BucketAccessControl::ROLE_READER());

  delete_acl = client.DeleteBucketAcl(bucket_name, viewers);
  ASSERT_STATUS_OK(delete_acl);

  current_acl = client.ListBucketAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(*current_acl),
              Not(Contains(create_acl->entity())));

  // With gRPC, this behavior is emulated by the library and thus needs testing.
  auto not_found_acl = client.GetBucketAcl(bucket_name, viewers);
  EXPECT_THAT(not_found_acl, StatusIs(StatusCode::kNotFound));

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(BucketAclIntegrationTest, CreatePredefinedAcl) {
  std::vector<PredefinedAcl> test_values{
      PredefinedAcl::AuthenticatedRead(), PredefinedAcl::Private(),
      PredefinedAcl::ProjectPrivate(),    PredefinedAcl::PublicRead(),
      PredefinedAcl::PublicReadWrite(),
  };

  auto client = MakeBucketIntegrationTestClient();
  for (auto const& acl : test_values) {
    SCOPED_TRACE(std::string("Testing with ") +
                 acl.well_known_parameter_name() + "=" + acl.value());
    std::string bucket_name = MakeRandomBucketName();

    auto metadata = client.CreateBucketForProject(
        bucket_name, project_id_, BucketMetadata(), PredefinedAcl(acl));
    ASSERT_STATUS_OK(metadata);
    EXPECT_EQ(bucket_name, metadata->name());

    // Wait at least 2 seconds before trying to create / delete another bucket.
    if (!UsingEmulator()) std::this_thread::sleep_for(std::chrono::seconds(2));

    auto status = client.DeleteBucket(bucket_name);
    ASSERT_STATUS_OK(status);

    // Wait at least 2 seconds before trying to create / delete another bucket.
    if (!UsingEmulator()) std::this_thread::sleep_for(std::chrono::seconds(2));
  }
}

TEST_F(BucketAclIntegrationTest, ListAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();

  // This operation should fail because the target bucket does not exist.
  auto list = client.ListBucketAcl(bucket_name);
  EXPECT_THAT(list, Not(IsOk())) << "list[0]=" << list.value().front();
}

TEST_F(BucketAclIntegrationTest, CreateAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client.CreateBucketAcl(bucket_name, entity_name, "READER");
  EXPECT_THAT(acl, Not(IsOk())) << "value=" << acl.value();
}

TEST_F(BucketAclIntegrationTest, GetAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client.GetBucketAcl(bucket_name, entity_name);
  EXPECT_THAT(acl, Not(IsOk())) << "value=" << acl.value();
}

TEST_F(BucketAclIntegrationTest, UpdateAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client.UpdateBucketAcl(
      bucket_name,
      BucketAccessControl().set_entity(entity_name).set_role("READER"));
  EXPECT_THAT(acl, Not(IsOk())) << "value=" << acl.value();
}

TEST_F(BucketAclIntegrationTest, PatchAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto acl = client.PatchBucketAcl(
      bucket_name, entity_name, BucketAccessControl(),
      BucketAccessControl().set_entity(entity_name).set_role("READER"));
  EXPECT_THAT(acl, Not(IsOk())) << "value=" << acl.value();
}

TEST_F(BucketAclIntegrationTest, DeleteAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status = client.DeleteBucketAcl(bucket_name, entity_name);
  EXPECT_THAT(status, Not(IsOk()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
