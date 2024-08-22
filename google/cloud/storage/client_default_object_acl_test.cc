// Copyright 2018 Google LLC
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
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/client_unit_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::CurrentOptions;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::Return;
using ms = std::chrono::milliseconds;

/**
 * Test the BucketAccessControls-related functions in storage::Client.
 */
class DefaultObjectAccessControlsTest
    : public ::google::cloud::storage::testing::ClientUnitTest {};

TEST_F(DefaultObjectAccessControlsTest, ListDefaultObjectAcl) {
  std::vector<ObjectAccessControl> expected{
      internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
          .value(),
      internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-2",
          "role": "READER"
      })""")
          .value(),
  };

  EXPECT_CALL(*mock_, ListDefaultObjectAcl)
      .WillOnce(Return(
          StatusOr<internal::ListDefaultObjectAclResponse>(TransientError())))
      .WillOnce([&expected](internal::ListDefaultObjectAclRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket", r.bucket_name());

        return make_status_or(internal::ListDefaultObjectAclResponse{expected});
      });

  auto client = ClientForMock();
  StatusOr<std::vector<ObjectAccessControl>> actual =
      client.ListDefaultObjectAcl("test-bucket",
                                  Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(DefaultObjectAccessControlsTest, CreateDefaultObjectAcl) {
  auto expected = internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "READER"
      })""")
                      .value();

  EXPECT_CALL(*mock_, CreateDefaultObjectAcl)
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce([&expected](internal::CreateDefaultObjectAclRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        EXPECT_EQ("READER", r.role());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<ObjectAccessControl> actual = client.CreateDefaultObjectAcl(
      "test-bucket", "user-test-user-1", ObjectAccessControl::ROLE_READER(),
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  // Compare just a few fields because the values for most of the fields are
  // hard to predict when testing against the production environment.
  EXPECT_EQ(expected.bucket(), actual->bucket());
  EXPECT_EQ(expected.entity(), actual->entity());
  EXPECT_EQ(expected.role(), actual->role());
}

TEST_F(DefaultObjectAccessControlsTest, DeleteDefaultObjectAcl) {
  EXPECT_CALL(*mock_, DeleteDefaultObjectAcl)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce([](internal::DeleteDefaultObjectAclRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user", r.entity());

        return make_status_or(internal::EmptyResponse{});
      });
  auto client = ClientForMock();
  auto status = client.DeleteDefaultObjectAcl(
      "test-bucket", "user-test-user",
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(status);
}

TEST_F(DefaultObjectAccessControlsTest, GetDefaultObjectAcl) {
  ObjectAccessControl expected =
      internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
          .value();

  EXPECT_CALL(*mock_, GetDefaultObjectAcl)
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce([&expected](internal::GetDefaultObjectAclRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<ObjectAccessControl> actual =
      client.GetDefaultObjectAcl("test-bucket", "user-test-user-1",
                                 Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(DefaultObjectAccessControlsTest, UpdateDefaultObjectAcl) {
  auto expected = internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "READER"
      })""")
                      .value();

  EXPECT_CALL(*mock_, UpdateDefaultObjectAcl)
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce([&expected](internal::UpdateDefaultObjectAclRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        EXPECT_EQ("READER", r.role());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<ObjectAccessControl> actual = client.UpdateDefaultObjectAcl(
      "test-bucket",
      ObjectAccessControl()
          .set_entity("user-test-user-1")
          .set_role(ObjectAccessControl::ROLE_READER()),
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  // Compare just a few fields because the values for most of the fields are
  // hard to predict when testing against the production environment.
  EXPECT_EQ(expected.bucket(), actual->bucket());
  EXPECT_EQ(expected.entity(), actual->entity());
  EXPECT_EQ(expected.role(), actual->role());
}

TEST_F(DefaultObjectAccessControlsTest, PatchDefaultObjectAcl) {
  auto result = internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
                    .value();

  EXPECT_CALL(*mock_, PatchDefaultObjectAcl)
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce([result](internal::PatchDefaultObjectAclRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        nlohmann::json expected{{"role", "OWNER"}};
        auto payload = nlohmann::json::parse(r.payload());
        EXPECT_EQ(expected, payload);

        return make_status_or(result);
      });
  auto client = ClientForMock();
  auto actual = client.PatchDefaultObjectAcl(
      "test-bucket", "user-test-user-1",
      ObjectAccessControlPatchBuilder().set_role("OWNER"),
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(result, *actual);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
