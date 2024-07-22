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

class DefaultObjectAclIntegrationTest
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

TEST_F(DefaultObjectAclIntegrationTest, AclCRUD) {
  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeIntegrationTestClient(MakeBucketTestOptions());

  // Create a new bucket to run the test, with the "private"
  // PredefinedDefaultObjectAcl, so we know what the contents of the ACL will
  // be.
  auto metadata = client.CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata(),
      PredefinedDefaultObjectAcl("authenticatedRead"), Projection("full"));
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  // We always use "project-viewers-${project_id}" because it is known to exist.
  auto const viewers = "project-viewers-" + project_id();

  ASSERT_THAT(metadata->default_acl(), Not(IsEmpty()))
      << "Test aborted. Empty default object ACL returned from newly created"
      << " bucket <" << bucket_name << "> even though we requested the <full>"
      << " projection.";
  ASSERT_THAT(AclEntityNames(metadata->default_acl()), Not(Contains(viewers)))
      << "Test aborted. The bucket <" << bucket_name << "> has <" << viewers
      << "> in its default object ACL.  This is unexpected because the bucket"
      << " was just created with a predefined object ACL which should preclude"
      << " this result.";

  auto const existing_entity = metadata->default_acl().front();
  auto current_acl = client.ListDefaultObjectAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(*current_acl),
              Contains(existing_entity.entity()).Times(1));

  auto get_acl =
      client.GetDefaultObjectAcl(bucket_name, existing_entity.entity());
  ASSERT_STATUS_OK(get_acl);
  EXPECT_EQ(*get_acl, existing_entity);

  auto create_acl = client.CreateDefaultObjectAcl(
      bucket_name, viewers, ObjectAccessControl::ROLE_READER());
  ASSERT_STATUS_OK(create_acl);

  current_acl = client.ListDefaultObjectAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(*current_acl),
              Contains(create_acl->entity()).Times(1));

  auto c2 = client.CreateDefaultObjectAcl(bucket_name, viewers,
                                          ObjectAccessControl::ROLE_READER());
  ASSERT_STATUS_OK(c2);
  // There is no guarantee that the ETag remains unchanged, even if the
  // operation has no effect.  Reset the one field that might change.
  create_acl->set_etag(c2->etag());
  EXPECT_EQ(*create_acl, *c2);

  auto updated_acl = client.UpdateDefaultObjectAcl(
      bucket_name, ObjectAccessControl().set_entity(viewers).set_role(
                       ObjectAccessControl::ROLE_OWNER()));
  ASSERT_STATUS_OK(updated_acl);
  EXPECT_EQ(updated_acl->entity(), create_acl->entity());
  EXPECT_EQ(updated_acl->role(), ObjectAccessControl::ROLE_OWNER());

  // "Updating" an entity that does not exist should create the entity
  auto delete_acl = client.DeleteDefaultObjectAcl(bucket_name, viewers);
  ASSERT_STATUS_OK(delete_acl);
  updated_acl = client.UpdateDefaultObjectAcl(
      bucket_name, ObjectAccessControl().set_entity(viewers).set_role(
                       ObjectAccessControl::ROLE_OWNER()));
  ASSERT_STATUS_OK(updated_acl);

  auto patched_acl =
      client.PatchDefaultObjectAcl(bucket_name, viewers,
                                   ObjectAccessControlPatchBuilder().set_role(
                                       ObjectAccessControl::ROLE_READER()));
  ASSERT_STATUS_OK(patched_acl);
  EXPECT_EQ(patched_acl->entity(), create_acl->entity());
  EXPECT_EQ(patched_acl->role(), ObjectAccessControl::ROLE_READER());

  // "Patching" an entity that does not exist should create the entity
  delete_acl = client.DeleteDefaultObjectAcl(bucket_name, viewers);
  ASSERT_STATUS_OK(delete_acl);
  patched_acl =
      client.PatchDefaultObjectAcl(bucket_name, viewers,
                                   ObjectAccessControlPatchBuilder().set_role(
                                       ObjectAccessControl::ROLE_READER()));
  ASSERT_STATUS_OK(patched_acl);
  EXPECT_EQ(patched_acl->entity(), create_acl->entity());
  EXPECT_EQ(patched_acl->role(), ObjectAccessControl::ROLE_READER());

  delete_acl = client.DeleteDefaultObjectAcl(bucket_name, viewers);
  ASSERT_STATUS_OK(delete_acl);

  current_acl = client.ListDefaultObjectAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(*current_acl),
              Not(Contains(create_acl->entity())));

  // With gRPC, this behavior is emulated by the library and thus needs testing.
  auto not_found_acl = client.GetDefaultObjectAcl(bucket_name, viewers);
  EXPECT_THAT(not_found_acl, StatusIs(StatusCode::kNotFound));

  auto status = client.DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(DefaultObjectAclIntegrationTest, CreatePredefinedDefaultObjectAcl) {
  std::vector<PredefinedDefaultObjectAcl> test_values{
      PredefinedDefaultObjectAcl::AuthenticatedRead(),
      PredefinedDefaultObjectAcl::BucketOwnerFullControl(),
      PredefinedDefaultObjectAcl::BucketOwnerRead(),
      PredefinedDefaultObjectAcl::Private(),
      PredefinedDefaultObjectAcl::ProjectPrivate(),
      PredefinedDefaultObjectAcl::PublicRead(),
  };

  auto client = MakeBucketIntegrationTestClient();
  for (auto const& acl : test_values) {
    SCOPED_TRACE(std::string("Testing with ") +
                 acl.well_known_parameter_name() + "=" + acl.value());
    std::string bucket_name = MakeRandomBucketName();

    auto metadata = client.CreateBucketForProject(
        bucket_name, project_id_, BucketMetadata(),
        PredefinedDefaultObjectAcl(acl));
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

TEST_F(DefaultObjectAclIntegrationTest, ListDefaultAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();

  // This operation should fail because the target bucket does not exist.
  auto status = client.ListDefaultObjectAcl(bucket_name).status();
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(DefaultObjectAclIntegrationTest, CreateDefaultAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status =
      client.CreateDefaultObjectAcl(bucket_name, entity_name, "READER")
          .status();
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(DefaultObjectAclIntegrationTest, GetDefaultAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status = client.GetDefaultObjectAcl(bucket_name, entity_name).status();
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(DefaultObjectAclIntegrationTest, UpdateDefaultAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status =
      client
          .UpdateDefaultObjectAcl(
              bucket_name,
              ObjectAccessControl().set_entity(entity_name).set_role("READER"))
          .status();
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(DefaultObjectAclIntegrationTest, PatchDefaultAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status =
      client
          .PatchDefaultObjectAcl(
              bucket_name, entity_name, ObjectAccessControl(),
              ObjectAccessControl().set_entity(entity_name).set_role("READER"))
          .status();
  EXPECT_THAT(status, Not(IsOk()));
}

TEST_F(DefaultObjectAclIntegrationTest, DeleteDefaultAccessControlFailure) {
  auto client = MakeIntegrationTestClient();
  std::string bucket_name = MakeRandomBucketName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the target bucket does not exist.
  auto status = client.DeleteDefaultObjectAcl(bucket_name, entity_name);
  EXPECT_THAT(status, Not(IsOk()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
