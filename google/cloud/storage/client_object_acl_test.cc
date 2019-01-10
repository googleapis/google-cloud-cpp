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
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
namespace {
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ms = std::chrono::milliseconds;
using testing::canonical_errors::TransientError;

/**
 * Test the ObjectAccessControls-related functions in storage::Client.
 */
class ObjectAccessControlsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock = std::make_shared<testing::MockClient>();
    EXPECT_CALL(*mock, client_options())
        .WillRepeatedly(ReturnRef(client_options));
    client.reset(new Client{std::shared_ptr<internal::RawClient>(mock)});
  }
  void TearDown() override {
    client.reset();
    mock.reset();
  }

  std::shared_ptr<testing::MockClient> mock;
  std::unique_ptr<Client> client;
  ClientOptions client_options =
      ClientOptions(oauth2::CreateAnonymousCredentials());
};

TEST_F(ObjectAccessControlsTest, ListObjectAcl) {
  std::vector<ObjectAccessControl> expected{
      ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
          .value(),
      ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-2",
          "role": "READER"
      })""")
          .value(),
  };

  EXPECT_CALL(*mock, ListObjectAcl(_))
      .WillOnce(
          Return(StatusOr<internal::ListObjectAclResponse>(TransientError())))
      .WillOnce(Invoke([&expected](internal::ListObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());

        return make_status_or(internal::ListObjectAclResponse{expected});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  std::vector<ObjectAccessControl> actual =
      client.ListObjectAcl("test-bucket", "test-object");
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectAccessControlsTest, ListObjectAclTooManyFailures) {
  testing::TooManyFailuresTest<internal::ListObjectAclResponse>(
      mock, EXPECT_CALL(*mock, ListObjectAcl(_)),
      [](Client& client) {
        client.ListObjectAcl("test-bucket-name", "test-object-name");
      },
      "ListObjectAcl");
}

TEST_F(ObjectAccessControlsTest, ListObjectAclPermanentFailure) {
  testing::PermanentFailureTest<internal::ListObjectAclResponse>(
      *client, EXPECT_CALL(*mock, ListObjectAcl(_)),
      [](Client& client) {
        client.ListObjectAcl("test-bucket-name", "test-object-name");
      },
      "ListObjectAcl");
}

TEST_F(ObjectAccessControlsTest, CreateObjectAcl) {
  auto expected = ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-1",
          "role": "READER"
      })""").value();

  EXPECT_CALL(*mock, CreateObjectAcl(_))
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce(Invoke([&expected](internal::CreateObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        EXPECT_EQ("READER", r.role());

        return make_status_or(expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  ObjectAccessControl actual =
      client.CreateObjectAcl("test-bucket", "test-object", "user-test-user-1",
                             ObjectAccessControl::ROLE_READER());
  // Compare just a few fields because the values for most of the fields are
  // hard to predict when testing against the production environment.
  EXPECT_EQ(expected.bucket(), actual.bucket());
  EXPECT_EQ(expected.object(), actual.object());
  EXPECT_EQ(expected.entity(), actual.entity());
  EXPECT_EQ(expected.role(), actual.role());
}

TEST_F(ObjectAccessControlsTest, CreateObjectAclTooManyFailures) {
  testing::TooManyFailuresTest<ObjectAccessControl>(
      mock, EXPECT_CALL(*mock, CreateObjectAcl(_)),
      [](Client& client) {
        client.CreateObjectAcl("test-bucket-name", "test-object-name",
                               "user-test-user-1", "READER");
      },
      [](Client& client) {
        client.CreateObjectAcl("test-bucket-name", "test-object-name",
                               "user-test-user-1", "READER",
                               IfMatchEtag("ABC="));
      },
      "CreateObjectAcl");
}

TEST_F(ObjectAccessControlsTest, CreateObjectAclPermanentFailure) {
  testing::PermanentFailureTest<ObjectAccessControl>(
      *client, EXPECT_CALL(*mock, CreateObjectAcl(_)),
      [](Client& client) {
        client.CreateObjectAcl("test-bucket-name", "test-object-name",
                               "user-test-user", "READER");
      },
      "CreateObjectAcl");
}

TEST_F(ObjectAccessControlsTest, DeleteObjectAcl) {
  EXPECT_CALL(*mock, DeleteObjectAcl(_))
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce(Invoke([](internal::DeleteObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());
        EXPECT_EQ("user-test-user", r.entity());

        return make_status_or(internal::EmptyResponse{});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  client.DeleteObjectAcl("test-bucket", "test-object", "user-test-user");
  SUCCEED();
}

TEST_F(ObjectAccessControlsTest, DeleteObjectAclTooManyFailures) {
  testing::TooManyFailuresTest<internal::EmptyResponse>(
      mock, EXPECT_CALL(*mock, DeleteObjectAcl(_)),
      [](Client& client) {
        client.DeleteObjectAcl("test-bucket-name", "test-object-name",
                               "user-test-user-1");
      },
      [](Client& client) {
        client.DeleteObjectAcl("test-bucket-name", "test-object-name",
                               "user-test-user-1", IfMatchEtag("ABC="));
      },
      "DeleteObjectAcl");
}

TEST_F(ObjectAccessControlsTest, DeleteObjectAclPermanentFailure) {
  testing::PermanentFailureTest<internal::EmptyResponse>(
      *client, EXPECT_CALL(*mock, DeleteObjectAcl(_)),
      [](Client& client) {
        client.DeleteObjectAcl("test-bucket-name", "test-object-name",
                               "user-test-user-1");
      },
      "DeleteObjectAcl");
}

TEST_F(ObjectAccessControlsTest, GetObjectAcl) {
  auto expected = ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-1",
          "role": "READER"
      })""").value();

  EXPECT_CALL(*mock, GetObjectAcl(_))
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce(Invoke([&expected](internal::GetObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());
        EXPECT_EQ("user-test-user-1", r.entity());

        return make_status_or(expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  ObjectAccessControl actual =
      client.GetObjectAcl("test-bucket", "test-object", "user-test-user-1");
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectAccessControlsTest, GetObjectAclTooManyFailures) {
  testing::TooManyFailuresTest<ObjectAccessControl>(
      mock, EXPECT_CALL(*mock, GetObjectAcl(_)),
      [](Client& client) {
        client.GetObjectAcl("test-bucket-name", "test-object-name",
                            "user-test-user-1");
      },
      "GetObjectAcl");
}

TEST_F(ObjectAccessControlsTest, GetObjectAclPermanentFailure) {
  testing::PermanentFailureTest<ObjectAccessControl>(
      *client, EXPECT_CALL(*mock, GetObjectAcl(_)),
      [](Client& client) {
        client.GetObjectAcl("test-bucket-name", "test-object-name",
                            "user-test-user");
      },
      "GetObjectAcl");
}

TEST_F(ObjectAccessControlsTest, UpdateObjectAcl) {
  auto expected = ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""").value();
  EXPECT_CALL(*mock, UpdateObjectAcl(_))
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce(Invoke([expected](internal::UpdateObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());
        EXPECT_EQ("user-test-user", r.entity());
        EXPECT_EQ("OWNER", r.role());

        return make_status_or(expected);
      }));

  Client client{std::shared_ptr<internal::RawClient>(mock)};
  ObjectAccessControl acl =
      ObjectAccessControl().set_role("OWNER").set_entity("user-test-user");
  auto actual = client.UpdateObjectAcl("test-bucket", "test-object", acl);
  EXPECT_EQ(expected, actual);
}

TEST_F(ObjectAccessControlsTest, UpdateObjectAclTooManyFailures) {
  testing::TooManyFailuresTest<ObjectAccessControl>(
      mock, EXPECT_CALL(*mock, UpdateObjectAcl(_)),
      [](Client& client) {
        client.UpdateObjectAcl("test-bucket-name", "test-object-name",
                               ObjectAccessControl());
      },
      [](Client& client) {
        client.UpdateObjectAcl("test-bucket-name", "test-object-name",
                               ObjectAccessControl(), IfMatchEtag("ABC="));
      },
      "UpdateObjectAcl");
}

TEST_F(ObjectAccessControlsTest, UpdateObjectAclPermanentFailure) {
  testing::PermanentFailureTest<ObjectAccessControl>(
      *client, EXPECT_CALL(*mock, UpdateObjectAcl(_)),
      [](Client& client) {
        client.UpdateObjectAcl("test-bucket-name", "test-object-name",
                               ObjectAccessControl());
      },
      "UpdateObjectAcl");
}

TEST_F(ObjectAccessControlsTest, PatchObjectAcl) {
  auto result = ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""").value();
  EXPECT_CALL(*mock, PatchObjectAcl(_))
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce(Invoke([result](internal::PatchObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        internal::nl::json expected{{"role", "OWNER"}};
        auto payload = internal::nl::json::parse(r.payload());
        EXPECT_EQ(expected, payload);

        return make_status_or(result);
      }));

  Client client{std::shared_ptr<internal::RawClient>(mock)};
  auto actual = client.PatchObjectAcl(
      "test-bucket", "test-object", "user-test-user-1",
      ObjectAccessControlPatchBuilder().set_role("OWNER"));
  EXPECT_EQ(result, actual);
}

TEST_F(ObjectAccessControlsTest, PatchObjectAclTooManyFailures) {
  testing::TooManyFailuresTest<ObjectAccessControl>(
      mock, EXPECT_CALL(*mock, PatchObjectAcl(_)),
      [](Client& client) {
        client.PatchObjectAcl("test-bucket-name", "test-object-name",
                              "user-test-user-1",
                              ObjectAccessControlPatchBuilder());
      },
      [](Client& client) {
        client.PatchObjectAcl(
            "test-bucket-name", "test-object-name", "user-test-user-1",
            ObjectAccessControlPatchBuilder(), IfMatchEtag("ABC="));
      },
      "PatchObjectAcl");
}

TEST_F(ObjectAccessControlsTest, PatchObjectAclPermanentFailure) {
  testing::PermanentFailureTest<ObjectAccessControl>(
      *client, EXPECT_CALL(*mock, PatchObjectAcl(_)),
      [](Client& client) {
        client.PatchObjectAcl("test-bucket-name", "test-object-name",
                              "user-test-user-1",
                              ObjectAccessControlPatchBuilder());
      },
      "PatchObjectAcl");
}

}  // namespace
}  // namespace storage
}  // namespace cloud
}  // namespace google
