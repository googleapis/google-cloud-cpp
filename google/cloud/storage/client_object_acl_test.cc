// Copyright 2018 Google LLC
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
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/client_unit_test.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace {

using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::Return;
using ms = std::chrono::milliseconds;

/**
 * Test the ObjectAccessControls-related functions in storage::Client.
 */
class ObjectAccessControlsTest
    : public ::google::cloud::storage::testing::ClientUnitTest {};

TEST_F(ObjectAccessControlsTest, ListObjectAcl) {
  std::vector<ObjectAccessControl> expected{
      internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
          .value(),
      internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-2",
          "role": "READER"
      })""")
          .value(),
  };

  EXPECT_CALL(*mock_, ListObjectAcl)
      .WillOnce(
          Return(StatusOr<internal::ListObjectAclResponse>(TransientError())))
      .WillOnce([&expected](internal::ListObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());

        return make_status_or(internal::ListObjectAclResponse{expected});
      });
  auto client = ClientForMock();
  StatusOr<std::vector<ObjectAccessControl>> actual =
      client.ListObjectAcl("test-bucket", "test-object");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectAccessControlsTest, ListObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::ListObjectAclResponse>(
      mock_, EXPECT_CALL(*mock_, ListObjectAcl),
      [](Client& client) {
        return client.ListObjectAcl("test-bucket-name", "test-object-name")
            .status();
      },
      "ListObjectAcl");
}

TEST_F(ObjectAccessControlsTest, ListObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<internal::ListObjectAclResponse>(
      client, EXPECT_CALL(*mock_, ListObjectAcl),
      [](Client& client) {
        return client.ListObjectAcl("test-bucket-name", "test-object-name")
            .status();
      },
      "ListObjectAcl");
}

TEST_F(ObjectAccessControlsTest, CreateObjectAcl) {
  auto expected = internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-1",
          "role": "READER"
      })""")
                      .value();

  EXPECT_CALL(*mock_, CreateObjectAcl)
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce([&expected](internal::CreateObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        EXPECT_EQ("READER", r.role());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<ObjectAccessControl> actual =
      client.CreateObjectAcl("test-bucket", "test-object", "user-test-user-1",
                             ObjectAccessControl::ROLE_READER());
  ASSERT_STATUS_OK(actual);
  // Compare just a few fields because the values for most of the fields are
  // hard to predict when testing against the production environment.
  EXPECT_EQ(expected.bucket(), actual->bucket());
  EXPECT_EQ(expected.object(), actual->object());
  EXPECT_EQ(expected.entity(), actual->entity());
  EXPECT_EQ(expected.role(), actual->role());
}

TEST_F(ObjectAccessControlsTest, CreateObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock_, EXPECT_CALL(*mock_, CreateObjectAcl),
      [](Client& client) {
        return client
            .CreateObjectAcl("test-bucket-name", "test-object-name",
                             "user-test-user-1", "READER")
            .status();
      },
      [](Client& client) {
        return client
            .CreateObjectAcl("test-bucket-name", "test-object-name",
                             "user-test-user-1", "READER", IfMatchEtag("ABC="))
            .status();
      },
      "CreateObjectAcl");
}

TEST_F(ObjectAccessControlsTest, CreateObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      client, EXPECT_CALL(*mock_, CreateObjectAcl),
      [](Client& client) {
        return client
            .CreateObjectAcl("test-bucket-name", "test-object-name",
                             "user-test-user", "READER")
            .status();
      },
      "CreateObjectAcl");
}

TEST_F(ObjectAccessControlsTest, DeleteObjectAcl) {
  EXPECT_CALL(*mock_, DeleteObjectAcl)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce([](internal::DeleteObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());
        EXPECT_EQ("user-test-user", r.entity());

        return make_status_or(internal::EmptyResponse{});
      });
  auto client = ClientForMock();
  client.DeleteObjectAcl("test-bucket", "test-object", "user-test-user");
  SUCCEED();
}

TEST_F(ObjectAccessControlsTest, DeleteObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::EmptyResponse>(
      mock_, EXPECT_CALL(*mock_, DeleteObjectAcl),
      [](Client& client) {
        return client.DeleteObjectAcl("test-bucket-name", "test-object-name",
                                      "user-test-user-1");
      },
      [](Client& client) {
        return client.DeleteObjectAcl("test-bucket-name", "test-object-name",
                                      "user-test-user-1", IfMatchEtag("ABC="));
      },
      "DeleteObjectAcl");
}

TEST_F(ObjectAccessControlsTest, DeleteObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<internal::EmptyResponse>(
      client, EXPECT_CALL(*mock_, DeleteObjectAcl),
      [](Client& client) {
        return client.DeleteObjectAcl("test-bucket-name", "test-object-name",
                                      "user-test-user-1");
      },
      "DeleteObjectAcl");
}

TEST_F(ObjectAccessControlsTest, GetObjectAcl) {
  auto expected = internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-1",
          "role": "READER"
      })""")
                      .value();

  EXPECT_CALL(*mock_, GetObjectAcl)
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce([&expected](internal::GetObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());
        EXPECT_EQ("user-test-user-1", r.entity());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<ObjectAccessControl> actual =
      client.GetObjectAcl("test-bucket", "test-object", "user-test-user-1");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectAccessControlsTest, GetObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock_, EXPECT_CALL(*mock_, GetObjectAcl),
      [](Client& client) {
        return client
            .GetObjectAcl("test-bucket-name", "test-object-name",
                          "user-test-user-1")
            .status();
      },
      "GetObjectAcl");
}

TEST_F(ObjectAccessControlsTest, GetObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      client, EXPECT_CALL(*mock_, GetObjectAcl),
      [](Client& client) {
        return client
            .GetObjectAcl("test-bucket-name", "test-object-name",
                          "user-test-user")
            .status();
      },
      "GetObjectAcl");
}

TEST_F(ObjectAccessControlsTest, UpdateObjectAcl) {
  auto expected = internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
                      .value();
  EXPECT_CALL(*mock_, UpdateObjectAcl)
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce([expected](internal::UpdateObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());
        EXPECT_EQ("user-test-user", r.entity());
        EXPECT_EQ("OWNER", r.role());

        return make_status_or(expected);
      });
  ObjectAccessControl acl =
      ObjectAccessControl().set_role("OWNER").set_entity("user-test-user");
  auto client = ClientForMock();
  auto actual = client.UpdateObjectAcl("test-bucket", "test-object", acl);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectAccessControlsTest, UpdateObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock_, EXPECT_CALL(*mock_, UpdateObjectAcl),
      [](Client& client) {
        return client
            .UpdateObjectAcl("test-bucket-name", "test-object-name",
                             ObjectAccessControl())
            .status();
      },
      [](Client& client) {
        return client
            .UpdateObjectAcl("test-bucket-name", "test-object-name",
                             ObjectAccessControl(), IfMatchEtag("ABC="))
            .status();
      },
      "UpdateObjectAcl");
}

TEST_F(ObjectAccessControlsTest, UpdateObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      client, EXPECT_CALL(*mock_, UpdateObjectAcl),
      [](Client& client) {
        return client
            .UpdateObjectAcl("test-bucket-name", "test-object-name",
                             ObjectAccessControl())
            .status();
      },
      "UpdateObjectAcl");
}

TEST_F(ObjectAccessControlsTest, PatchObjectAcl) {
  auto result = internal::ObjectAccessControlParser::FromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
                    .value();
  EXPECT_CALL(*mock_, PatchObjectAcl)
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce([result](internal::PatchObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        nlohmann::json expected{{"role", "OWNER"}};
        auto payload = nlohmann::json::parse(r.payload());
        EXPECT_EQ(expected, payload);

        return make_status_or(result);
      });
  auto client = ClientForMock();
  auto actual = client.PatchObjectAcl(
      "test-bucket", "test-object", "user-test-user-1",
      ObjectAccessControlPatchBuilder().set_role("OWNER"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(result, *actual);
}

TEST_F(ObjectAccessControlsTest, PatchObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock_, EXPECT_CALL(*mock_, PatchObjectAcl),
      [](Client& client) {
        return client
            .PatchObjectAcl("test-bucket-name", "test-object-name",
                            "user-test-user-1",
                            ObjectAccessControlPatchBuilder())
            .status();
      },
      [](Client& client) {
        return client
            .PatchObjectAcl(
                "test-bucket-name", "test-object-name", "user-test-user-1",
                ObjectAccessControlPatchBuilder(), IfMatchEtag("ABC="))
            .status();
      },
      "PatchObjectAcl");
}

TEST_F(ObjectAccessControlsTest, PatchObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      client, EXPECT_CALL(*mock_, PatchObjectAcl),
      [](Client& client) {
        return client
            .PatchObjectAcl("test-bucket-name", "test-object-name",
                            "user-test-user-1",
                            ObjectAccessControlPatchBuilder())
            .status();
      },
      "PatchObjectAcl");
}

}  // namespace
}  // namespace storage
}  // namespace cloud
}  // namespace google
