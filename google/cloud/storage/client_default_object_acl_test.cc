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
inline namespace STORAGE_CLIENT_NS {
namespace {

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
        EXPECT_EQ("test-bucket", r.bucket_name());

        return make_status_or(internal::ListDefaultObjectAclResponse{expected});
      });

  auto client = ClientForMock();
  StatusOr<std::vector<ObjectAccessControl>> actual =
      client.ListDefaultObjectAcl("test-bucket");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(DefaultObjectAccessControlsTest, ListDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::ListDefaultObjectAclResponse>(
      mock_, EXPECT_CALL(*mock_, ListDefaultObjectAcl),
      [](Client& client) {
        return client.ListDefaultObjectAcl("test-bucket-name").status();
      },
      "ListDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest, ListDefaultObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<internal::ListDefaultObjectAclResponse>(
      client, EXPECT_CALL(*mock_, ListDefaultObjectAcl),
      [](Client& client) {
        return client.ListDefaultObjectAcl("test-bucket-name").status();
      },
      "ListDefaultObjectAcl");
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
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        EXPECT_EQ("READER", r.role());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<ObjectAccessControl> actual = client.CreateDefaultObjectAcl(
      "test-bucket", "user-test-user-1", ObjectAccessControl::ROLE_READER());
  ASSERT_STATUS_OK(actual);
  // Compare just a few fields because the values for most of the fields are
  // hard to predict when testing against the production environment.
  EXPECT_EQ(expected.bucket(), actual->bucket());
  EXPECT_EQ(expected.entity(), actual->entity());
  EXPECT_EQ(expected.role(), actual->role());
}

TEST_F(DefaultObjectAccessControlsTest, CreateDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock_, EXPECT_CALL(*mock_, CreateDefaultObjectAcl),
      [](Client& client) {
        return client
            .CreateDefaultObjectAcl("test-bucket-name", "user-test-user-1",
                                    "READER")
            .status();
      },
      [](Client& client) {
        return client
            .CreateDefaultObjectAcl("test-bucket-name", "user-test-user-1",
                                    "READER", IfMatchEtag("ABC="))
            .status();
      },
      "CreateDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest,
       CreateDefaultObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      client, EXPECT_CALL(*mock_, CreateDefaultObjectAcl),
      [](Client& client) {
        return client
            .CreateDefaultObjectAcl("test-bucket-name", "user-test-user",
                                    "READER")
            .status();
      },
      "CreateDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest, DeleteDefaultObjectAcl) {
  EXPECT_CALL(*mock_, DeleteDefaultObjectAcl)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce([](internal::DeleteDefaultObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user", r.entity());

        return make_status_or(internal::EmptyResponse{});
      });
  auto client = ClientForMock();
  auto status = client.DeleteDefaultObjectAcl("test-bucket", "user-test-user");
  ASSERT_STATUS_OK(status);
}

TEST_F(DefaultObjectAccessControlsTest, DeleteDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::EmptyResponse>(
      mock_, EXPECT_CALL(*mock_, DeleteDefaultObjectAcl),
      [](Client& client) {
        return client.DeleteDefaultObjectAcl("test-bucket-name",
                                             "user-test-user-1");
      },
      [](Client& client) {
        return client.DeleteDefaultObjectAcl(
            "test-bucket-name", "user-test-user-1", IfMatchEtag("ABC="));
      },
      "DeleteDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest,
       DeleteDefaultObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<internal::EmptyResponse>(
      client, EXPECT_CALL(*mock_, DeleteDefaultObjectAcl),
      [](Client& client) {
        return client.DeleteDefaultObjectAcl("test-bucket-name",
                                             "user-test-user-1");
      },
      "DeleteDefaultObjectAcl");
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
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<ObjectAccessControl> actual =
      client.GetDefaultObjectAcl("test-bucket", "user-test-user-1");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(DefaultObjectAccessControlsTest, GetDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock_, EXPECT_CALL(*mock_, GetDefaultObjectAcl),
      [](Client& client) {
        return client
            .GetDefaultObjectAcl("test-bucket-name", "user-test-user-1")
            .status();
      },
      "GetDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest, GetDefaultObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      client, EXPECT_CALL(*mock_, GetDefaultObjectAcl),
      [](Client& client) {
        return client
            .GetDefaultObjectAcl("test-bucket-name", "user-test-user-1")
            .status();
      },
      "GetDefaultObjectAcl");
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
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        EXPECT_EQ("READER", r.role());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<ObjectAccessControl> actual = client.UpdateDefaultObjectAcl(
      "test-bucket", ObjectAccessControl()
                         .set_entity("user-test-user-1")
                         .set_role(ObjectAccessControl::ROLE_READER()));
  ASSERT_STATUS_OK(actual);
  // Compare just a few fields because the values for most of the fields are
  // hard to predict when testing against the production environment.
  EXPECT_EQ(expected.bucket(), actual->bucket());
  EXPECT_EQ(expected.entity(), actual->entity());
  EXPECT_EQ(expected.role(), actual->role());
}

TEST_F(DefaultObjectAccessControlsTest, UpdateDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock_, EXPECT_CALL(*mock_, UpdateDefaultObjectAcl),
      [](Client& client) {
        return client
            .UpdateDefaultObjectAcl("test-bucket-name", ObjectAccessControl())
            .status();
      },
      [](Client& client) {
        return client
            .UpdateDefaultObjectAcl("test-bucket-name", ObjectAccessControl(),
                                    IfMatchEtag("ABC="))
            .status();
      },
      "UpdateDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest,
       UpdateDefaultObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      client, EXPECT_CALL(*mock_, UpdateDefaultObjectAcl),
      [](Client& client) {
        return client
            .UpdateDefaultObjectAcl("test-bucket-name", ObjectAccessControl())
            .status();
      },
      "UpdateDefaultObjectAcl");
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
      ObjectAccessControlPatchBuilder().set_role("OWNER"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(result, *actual);
}

TEST_F(DefaultObjectAccessControlsTest, PatchDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock_, EXPECT_CALL(*mock_, PatchDefaultObjectAcl),
      [](Client& client) {
        return client
            .PatchDefaultObjectAcl("test-bucket-name", "user-test-user-1",
                                   ObjectAccessControlPatchBuilder())
            .status();
      },
      [](Client& client) {
        return client
            .PatchDefaultObjectAcl("test-bucket-name", "user-test-user-1",
                                   ObjectAccessControlPatchBuilder(),
                                   IfMatchEtag("ABC="))
            .status();
      },
      "PatchDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest, PatchDefaultObjectAclPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      client, EXPECT_CALL(*mock_, PatchDefaultObjectAcl),
      [](Client& client) {
        return client
            .PatchDefaultObjectAcl("test-bucket-name", "user-test-user-1",
                                   ObjectAccessControlPatchBuilder())
            .status();
      },
      "PatchDefaultObjectAcl");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
