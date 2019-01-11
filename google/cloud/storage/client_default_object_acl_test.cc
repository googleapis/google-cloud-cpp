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
inline namespace STORAGE_CLIENT_NS {
namespace {
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ms = std::chrono::milliseconds;
using testing::canonical_errors::TransientError;

/**
 * Test the BucketAccessControls-related functions in storage::Client.
 */
class DefaultObjectAccessControlsTest : public ::testing::Test {
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

TEST_F(DefaultObjectAccessControlsTest, ListDefaultObjectAcl) {
  std::vector<ObjectAccessControl> expected{
      ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
          .value(),
      ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-2",
          "role": "READER"
      })""")
          .value(),
  };

  EXPECT_CALL(*mock, ListDefaultObjectAcl(_))
      .WillOnce(Return(
          StatusOr<internal::ListDefaultObjectAclResponse>(TransientError())))
      .WillOnce(Invoke([&expected](
                           internal::ListDefaultObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());

        return make_status_or(internal::ListDefaultObjectAclResponse{expected});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<std::vector<ObjectAccessControl>> actual =
      client.ListDefaultObjectAcl("test-bucket");
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();
  EXPECT_EQ(expected, *actual);
}

TEST_F(DefaultObjectAccessControlsTest, ListDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::ListDefaultObjectAclResponse>(
      mock, EXPECT_CALL(*mock, ListDefaultObjectAcl(_)),
      [](Client& client) {
        return client.ListDefaultObjectAcl("test-bucket-name").status();
      },
      "ListDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest, ListDefaultObjectAclPermanentFailure) {
  testing::PermanentFailureStatusTest<internal::ListDefaultObjectAclResponse>(
      *client, EXPECT_CALL(*mock, ListDefaultObjectAcl(_)),
      [](Client& client) {
        return client.ListDefaultObjectAcl("test-bucket-name").status();
      },
      "ListDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest, CreateDefaultObjectAcl) {
  auto expected = ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "READER"
      })""")
                      .value();

  EXPECT_CALL(*mock, CreateDefaultObjectAcl(_))
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce(
          Invoke([&expected](internal::CreateDefaultObjectAclRequest const& r) {
            EXPECT_EQ("test-bucket", r.bucket_name());
            EXPECT_EQ("user-test-user-1", r.entity());
            EXPECT_EQ("READER", r.role());

            return make_status_or(expected);
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<ObjectAccessControl> actual = client.CreateDefaultObjectAcl(
      "test-bucket", "user-test-user-1", ObjectAccessControl::ROLE_READER());
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();
  // Compare just a few fields because the values for most of the fields are
  // hard to predict when testing against the production environment.
  EXPECT_EQ(expected.bucket(), actual->bucket());
  EXPECT_EQ(expected.entity(), actual->entity());
  EXPECT_EQ(expected.role(), actual->role());
}

TEST_F(DefaultObjectAccessControlsTest, CreateDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock, EXPECT_CALL(*mock, CreateDefaultObjectAcl(_)),
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
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      *client, EXPECT_CALL(*mock, CreateDefaultObjectAcl(_)),
      [](Client& client) {
        return client
            .CreateDefaultObjectAcl("test-bucket-name", "user-test-user",
                                    "READER")
            .status();
      },
      "CreateDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest, DeleteDefaultObjectAcl) {
  EXPECT_CALL(*mock, DeleteDefaultObjectAcl(_))
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce(Invoke([](internal::DeleteDefaultObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user", r.entity());

        return make_status_or(internal::EmptyResponse{});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  auto actual = client.DeleteDefaultObjectAcl("test-bucket", "user-test-user");
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();
}

TEST_F(DefaultObjectAccessControlsTest, DeleteDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::EmptyResponse>(
      mock, EXPECT_CALL(*mock, DeleteDefaultObjectAcl(_)),
      [](Client& client) {
        return client
            .DeleteDefaultObjectAcl("test-bucket-name", "user-test-user-1")
            .status();
      },
      [](Client& client) {
        return client
            .DeleteDefaultObjectAcl("test-bucket-name", "user-test-user-1",
                                    IfMatchEtag("ABC="))
            .status();
      },
      "DeleteDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest,
       DeleteDefaultObjectAclPermanentFailure) {
  testing::PermanentFailureStatusTest<internal::EmptyResponse>(
      *client, EXPECT_CALL(*mock, DeleteDefaultObjectAcl(_)),
      [](Client& client) {
        return client
            .DeleteDefaultObjectAcl("test-bucket-name", "user-test-user-1")
            .status();
      },
      "DeleteDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest, GetDefaultObjectAcl) {
  ObjectAccessControl expected = ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
                                     .value();

  EXPECT_CALL(*mock, GetDefaultObjectAcl(_))
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce(
          Invoke([&expected](internal::GetDefaultObjectAclRequest const& r) {
            EXPECT_EQ("test-bucket", r.bucket_name());
            EXPECT_EQ("user-test-user-1", r.entity());

            return make_status_or(expected);
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<ObjectAccessControl> actual =
      client.GetDefaultObjectAcl("test-bucket", "user-test-user-1");
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();
  EXPECT_EQ(expected, *actual);
}

TEST_F(DefaultObjectAccessControlsTest, GetDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock, EXPECT_CALL(*mock, GetDefaultObjectAcl(_)),
      [](Client& client) {
        return client
            .GetDefaultObjectAcl("test-bucket-name", "user-test-user-1")
            .status();
      },
      "GetDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest, GetDefaultObjectAclPermanentFailure) {
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      *client, EXPECT_CALL(*mock, GetDefaultObjectAcl(_)),
      [](Client& client) {
        return client
            .GetDefaultObjectAcl("test-bucket-name", "user-test-user-1")
            .status();
      },
      "GetDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest, UpdateDefaultObjectAcl) {
  auto expected = ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "READER"
      })""")
                      .value();

  EXPECT_CALL(*mock, UpdateDefaultObjectAcl(_))
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce(
          Invoke([&expected](internal::UpdateDefaultObjectAclRequest const& r) {
            EXPECT_EQ("test-bucket", r.bucket_name());
            EXPECT_EQ("user-test-user-1", r.entity());
            EXPECT_EQ("READER", r.role());

            return make_status_or(expected);
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<ObjectAccessControl> actual = client.UpdateDefaultObjectAcl(
      "test-bucket", ObjectAccessControl()
                         .set_entity("user-test-user-1")
                         .set_role(ObjectAccessControl::ROLE_READER()));
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();
  // Compare just a few fields because the values for most of the fields are
  // hard to predict when testing against the production environment.
  EXPECT_EQ(expected.bucket(), actual->bucket());
  EXPECT_EQ(expected.entity(), actual->entity());
  EXPECT_EQ(expected.role(), actual->role());
}

TEST_F(DefaultObjectAccessControlsTest, UpdateDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock, EXPECT_CALL(*mock, UpdateDefaultObjectAcl(_)),
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
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      *client, EXPECT_CALL(*mock, UpdateDefaultObjectAcl(_)),
      [](Client& client) {
        return client
            .UpdateDefaultObjectAcl("test-bucket-name", ObjectAccessControl())
            .status();
      },
      "UpdateDefaultObjectAcl");
}

TEST_F(DefaultObjectAccessControlsTest, PatchDefaultObjectAcl) {
  auto result = ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
                    .value();

  EXPECT_CALL(*mock, PatchDefaultObjectAcl(_))
      .WillOnce(Return(StatusOr<ObjectAccessControl>(TransientError())))
      .WillOnce(
          Invoke([result](internal::PatchDefaultObjectAclRequest const& r) {
            EXPECT_EQ("test-bucket", r.bucket_name());
            EXPECT_EQ("user-test-user-1", r.entity());
            internal::nl::json expected{{"role", "OWNER"}};
            auto payload = internal::nl::json::parse(r.payload());
            EXPECT_EQ(expected, payload);

            return make_status_or(result);
          }));

  Client client{std::shared_ptr<internal::RawClient>(mock)};
  auto actual = client.PatchDefaultObjectAcl(
      "test-bucket", "user-test-user-1",
      ObjectAccessControlPatchBuilder().set_role("OWNER"));
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();
  EXPECT_EQ(result, *actual);
}

TEST_F(DefaultObjectAccessControlsTest, PatchDefaultObjectAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectAccessControl>(
      mock, EXPECT_CALL(*mock, PatchDefaultObjectAcl(_)),
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
  testing::PermanentFailureStatusTest<ObjectAccessControl>(
      *client, EXPECT_CALL(*mock, PatchDefaultObjectAcl(_)),
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
