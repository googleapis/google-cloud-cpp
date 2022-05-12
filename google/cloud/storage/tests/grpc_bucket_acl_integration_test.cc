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
namespace internal {
namespace {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::storage::testing::AclEntityNames;
using ::google::cloud::testing_util::ContainsOnce;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;

// When GOOGLE_CLOUD_CPP_HAVE_GRPC is not set these tests compile, but they
// actually just run against the regular GCS REST API. That is fine.
class GrpcBucketAclIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

TEST_F(GrpcBucketAclIntegrationTest, AclCRUD) {
  ScopedEnvironment grpc_config("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                "metadata");
  // TODO(#7257) - restore gRPC integration tests against production
  if (!UsingEmulator()) GTEST_SKIP();

  auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  ASSERT_THAT(project_id, Not(IsEmpty())) << "GOOGLE_CLOUD_PROJECT is not set";

  std::string bucket_name = MakeRandomBucketName();
  auto client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  // Create a new bucket to run the test, with the "private" PredefinedAcl so
  // we know what the contents of the ACL will be.
  auto metadata = client->CreateBucketForProject(
      bucket_name, project_id, BucketMetadata(), PredefinedAcl("private"),
      Projection("full"));
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  // We always use "project-viewers-${project_id}" because it is known to exist.
  auto const viewers = "project-viewers-" + project_id;
  auto const owners = "project-owners-" + project_id;

  ASSERT_THAT(metadata->acl(), Not(IsEmpty()))
      << "Test aborted. Empty ACL returned from newly created bucket <"
      << bucket_name << "> even though we requested the <full> projection.";
  ASSERT_THAT(AclEntityNames(metadata->acl()), Not(Contains(viewers)))
      << "Test aborted. The bucket <" << bucket_name << "> has <" << viewers
      << "> in its ACL.  This is unexpected because the bucket was just"
      << " created with a predefine ACL which should preclude this result.";

  auto const existing_entity = metadata->acl().front();
  auto current_acl = client->ListBucketAcl(bucket_name);
  ASSERT_STATUS_OK(current_acl);
  // Search using the entity name returned by the request, because we use
  // 'project-editors-<project_id>' this different than the original entity
  // name, the server "translates" the project id to a project number.
  EXPECT_THAT(AclEntityNames(*current_acl),
              ContainsOnce(existing_entity.entity()));

  auto status = client->DeleteBucket(bucket_name);
  ASSERT_STATUS_OK(status);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
