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
#include "google/cloud/testing_util/contains_once.h"
#include "google/cloud/testing_util/scoped_environment.h"
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
using ::google::cloud::testing_util::ContainsOnce;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;

class GrpcObjectAclIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(GrpcObjectAclIntegrationTest, AclCRUD) {
  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");
  // TODO(#9800) - restore gRPC integration tests against production
  if (!UsingEmulator()) GTEST_SKIP();

  auto const bucket_name =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value_or("");
  ASSERT_THAT(bucket_name, Not(IsEmpty()))
      << "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME is not set";

  auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_id, Not(IsEmpty())) << "GOOGLE_CLOUD_PROJECT is not set";

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new object to run the tests.
  auto object_name = MakeRandomObjectName();
  auto insert = client->InsertObject(bucket_name, object_name, LoremIpsum(),
                                     IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(insert);
  ScheduleForDelete(*insert);

  // We always use "project-viewers-${project_id}" because it is known to exist.
  auto const viewers = "project-viewers-" + project_id;
  auto const owners = "project-owners-" + project_id;

  ASSERT_THAT(insert->acl(), Not(IsEmpty()))
      << "Test aborted. Empty ACL returned from newly created object <"
      << object_name << "> even though we requested the <full> projection.";
  ASSERT_THAT(AclEntityNames(insert->acl()), Not(Contains(viewers)))
      << "Test aborted. The object <" << object_name << "> has <" << viewers
      << "> in its ACL.  This is unexpected because the object was just"
      << " created with a predefined ACL which should preclude this result.";

  auto const existing_entity = insert->acl().front();
  auto current_acl = client->ListObjectAcl(bucket_name, object_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(*current_acl),
              ContainsOnce(existing_entity.entity()));

  auto create_acl = client->CreateObjectAcl(bucket_name, object_name, viewers,
                                            BucketAccessControl::ROLE_READER());
  ASSERT_STATUS_OK(create_acl);

  current_acl = client->ListObjectAcl(bucket_name, object_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(*current_acl), ContainsOnce(create_acl->entity()));

  auto c2 = client->CreateObjectAcl(bucket_name, object_name, viewers,
                                    BucketAccessControl::ROLE_READER());
  ASSERT_STATUS_OK(c2);
  EXPECT_EQ(*create_acl, *c2);

  auto get_acl = client->GetObjectAcl(bucket_name, object_name, viewers);
  ASSERT_STATUS_OK(get_acl);
  EXPECT_EQ(*create_acl, *get_acl);

  auto not_found_acl =
      client->GetObjectAcl(bucket_name, object_name, "not-found-entity");
  EXPECT_THAT(not_found_acl, StatusIs(StatusCode::kNotFound));

  auto updated_acl = client->UpdateObjectAcl(
      bucket_name, object_name,
      ObjectAccessControl().set_entity(viewers).set_role(
          ObjectAccessControl::ROLE_OWNER()));
  ASSERT_STATUS_OK(updated_acl);
  EXPECT_EQ(updated_acl->entity(), create_acl->entity());
  EXPECT_EQ(updated_acl->role(), ObjectAccessControl::ROLE_OWNER());

  // "Updating" an entity that does not exist should create the entity
  auto delete_acl = client->DeleteObjectAcl(bucket_name, object_name, viewers);
  ASSERT_STATUS_OK(delete_acl);
  updated_acl = client->UpdateObjectAcl(
      bucket_name, object_name,
      ObjectAccessControl().set_entity(viewers).set_role(
          ObjectAccessControl::ROLE_OWNER()));
  ASSERT_STATUS_OK(updated_acl);
  EXPECT_EQ(updated_acl->entity(), create_acl->entity());
  EXPECT_EQ(updated_acl->role(), ObjectAccessControl::ROLE_OWNER());

  auto patched_acl =
      client->PatchObjectAcl(bucket_name, object_name, create_acl->entity(),
                             ObjectAccessControlPatchBuilder().set_role(
                                 ObjectAccessControl::ROLE_READER()));
  ASSERT_STATUS_OK(patched_acl);
  EXPECT_EQ(patched_acl->entity(), create_acl->entity());
  EXPECT_EQ(patched_acl->role(), ObjectAccessControl::ROLE_READER());

  // "Patching" an entity that does not exist should create the entity
  delete_acl = client->DeleteObjectAcl(bucket_name, object_name, viewers);
  ASSERT_STATUS_OK(delete_acl);
  patched_acl =
      client->PatchObjectAcl(bucket_name, object_name, create_acl->entity(),
                             ObjectAccessControlPatchBuilder().set_role(
                                 ObjectAccessControl::ROLE_READER()));
  ASSERT_STATUS_OK(patched_acl);
  EXPECT_EQ(patched_acl->entity(), create_acl->entity());
  EXPECT_EQ(patched_acl->role(), ObjectAccessControl::ROLE_READER());

  delete_acl = client->DeleteObjectAcl(bucket_name, object_name, viewers);
  ASSERT_STATUS_OK(delete_acl);

  current_acl = client->ListObjectAcl(bucket_name, object_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_THAT(AclEntityNames(*current_acl),
              Not(Contains(create_acl->entity())));

  auto status = client->DeleteObject(bucket_name, object_name);
  ASSERT_STATUS_OK(status);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
