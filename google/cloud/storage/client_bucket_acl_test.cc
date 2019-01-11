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
class BucketAccessControlsTest : public ::testing::Test {
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

TEST_F(BucketAccessControlsTest, ListBucketAcl) {
  std::vector<BucketAccessControl> expected{
      BucketAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
          .value(),
      BucketAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-2",
          "role": "READER"
      })""")
          .value(),
  };

  EXPECT_CALL(*mock, ListBucketAcl(_))
      .WillOnce(
          Return(StatusOr<internal::ListBucketAclResponse>(TransientError())))
      .WillOnce(Invoke([&expected](internal::ListBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());

        return make_status_or(internal::ListBucketAclResponse{expected});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<std::vector<BucketAccessControl>> actual =
      client.ListBucketAcl("test-bucket");
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();
  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketAccessControlsTest, ListBucketAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::ListBucketAclResponse>(
      mock, EXPECT_CALL(*mock, ListBucketAcl(_)),
      [](Client& client) {
        return client.ListBucketAcl("test-bucket-name").status();
      },
      "ListBucketAcl");
}

TEST_F(BucketAccessControlsTest, ListBucketAclPermanentFailure) {
  testing::PermanentFailureStatusTest<internal::ListBucketAclResponse>(
      *client, EXPECT_CALL(*mock, ListBucketAcl(_)),
      [](Client& client) {
        return client.ListBucketAcl("test-bucket-name").status();
      },
      "ListBucketAcl");
}

TEST_F(BucketAccessControlsTest, CreateBucketAcl) {
  auto expected = BucketAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "READER"
      })""")
                      .value();

  EXPECT_CALL(*mock, CreateBucketAcl(_))
      .WillOnce(Return(StatusOr<BucketAccessControl>(TransientError())))
      .WillOnce(Invoke([&expected](internal::CreateBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        EXPECT_EQ("READER", r.role());

        return make_status_or(expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<BucketAccessControl> actual = client.CreateBucketAcl(
      "test-bucket", "user-test-user-1", BucketAccessControl::ROLE_READER());
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();

  // Compare just a few fields because the values for most of the fields are
  // hard to predict when testing against the production environment.
  EXPECT_EQ(expected.bucket(), actual->bucket());
  EXPECT_EQ(expected.entity(), actual->entity());
  EXPECT_EQ(expected.role(), actual->role());
}

TEST_F(BucketAccessControlsTest, CreateBucketAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<BucketAccessControl>(
      mock, EXPECT_CALL(*mock, CreateBucketAcl(_)),
      [](Client& client) {
        return client
            .CreateBucketAcl("test-bucket-name", "user-test-user-1", "READER")
            .status();
      },
      [](Client& client) {
        return client
            .CreateBucketAcl("test-bucket-name", "user-test-user-1", "READER",
                             IfMatchEtag("ABC="))
            .status();
      },
      "CreateBucketAcl");
}

TEST_F(BucketAccessControlsTest, CreateBucketAclPermanentFailure) {
  testing::PermanentFailureStatusTest<BucketAccessControl>(
      *client, EXPECT_CALL(*mock, CreateBucketAcl(_)),
      [](Client& client) {
        return client
            .CreateBucketAcl("test-bucket-name", "user-test-user", "READER")
            .status();
      },
      "CreateBucketAcl");
}

TEST_F(BucketAccessControlsTest, DeleteBucketAcl) {
  EXPECT_CALL(*mock, DeleteBucketAcl(_))
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce(Invoke([](internal::DeleteBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());

        return make_status_or(internal::EmptyResponse{});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<void> actual =
      client.DeleteBucketAcl("test-bucket", "user-test-user-1");
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();
}

TEST_F(BucketAccessControlsTest, DeleteBucketAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::EmptyResponse>(
      mock, EXPECT_CALL(*mock, DeleteBucketAcl(_)),
      [](Client& client) {
        return client.DeleteBucketAcl("test-bucket-name", "user-test-user-1")
            .status();
      },
      [](Client& client) {
        return client
            .DeleteBucketAcl("test-bucket-name", "user-test-user-1",
                             IfMatchEtag("ABC="))
            .status();
      },
      "DeleteBucketAcl");
}

TEST_F(BucketAccessControlsTest, DeleteBucketAclPermanentFailure) {
  testing::PermanentFailureStatusTest<internal::EmptyResponse>(
      *client, EXPECT_CALL(*mock, DeleteBucketAcl(_)),
      [](Client& client) {
        return client.DeleteBucketAcl("test-bucket-name", "user-test-user")
            .status();
      },
      "DeleteBucketAcl");
}

TEST_F(BucketAccessControlsTest, GetBucketAcl) {
  BucketAccessControl expected = BucketAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
                                     .value();

  EXPECT_CALL(*mock, GetBucketAcl(_))
      .WillOnce(Return(StatusOr<BucketAccessControl>(TransientError())))
      .WillOnce(Invoke([&expected](internal::GetBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());

        return make_status_or(expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<BucketAccessControl> actual =
      client.GetBucketAcl("test-bucket", "user-test-user-1");
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();

  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketAccessControlsTest, GetBucketAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<BucketAccessControl>(
      mock, EXPECT_CALL(*mock, GetBucketAcl(_)),
      [](Client& client) {
        return client.GetBucketAcl("test-bucket-name", "user-test-user-1")
            .status();
      },
      "GetBucketAcl");
}

TEST_F(BucketAccessControlsTest, GetBucketAclPermanentFailure) {
  testing::PermanentFailureStatusTest<BucketAccessControl>(
      *client, EXPECT_CALL(*mock, GetBucketAcl(_)),
      [](Client& client) {
        return client.GetBucketAcl("test-bucket-name", "user-test-user-1")
            .status();
      },
      "GetBucketAcl");
}

TEST_F(BucketAccessControlsTest, UpdateBucketAcl) {
  BucketAccessControl expected = BucketAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
                                     .value();

  EXPECT_CALL(*mock, UpdateBucketAcl(_))
      .WillOnce(Return(StatusOr<BucketAccessControl>(TransientError())))
      .WillOnce(Invoke([&expected](internal::UpdateBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        EXPECT_EQ("OWNER", r.role());

        return make_status_or(expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<BucketAccessControl> actual = client.UpdateBucketAcl(
      "test-bucket",
      BucketAccessControl().set_entity("user-test-user-1").set_role("OWNER"));
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();

  EXPECT_EQ(expected, *actual);
}

TEST_F(BucketAccessControlsTest, UpdateBucketAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<BucketAccessControl>(
      mock, EXPECT_CALL(*mock, UpdateBucketAcl(_)),
      [](Client& client) {
        return client
            .UpdateBucketAcl("test-bucket", BucketAccessControl()
                                                .set_entity("user-test-user-1")
                                                .set_role("OWNER"))
            .status();
      },
      [](Client& client) {
        return client
            .UpdateBucketAcl("test-bucket",
                             BucketAccessControl()
                                 .set_entity("user-test-user-1")
                                 .set_role("OWNER"),
                             IfMatchEtag("ABC="))
            .status();
      },
      "UpdateBucketAcl");
}

TEST_F(BucketAccessControlsTest, UpdateBucketAclPermanentFailure) {
  testing::PermanentFailureStatusTest<BucketAccessControl>(
      *client, EXPECT_CALL(*mock, UpdateBucketAcl(_)),
      [](Client& client) {
        return client
            .UpdateBucketAcl("test-bucket", BucketAccessControl()
                                                .set_entity("user-test-user-1")
                                                .set_role("OWNER"))
            .status();
      },
      "UpdateBucketAcl");
}

TEST_F(BucketAccessControlsTest, PatchBucketAcl) {
  BucketAccessControl result = BucketAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""")
                                   .value();

  EXPECT_CALL(*mock, PatchBucketAcl(_))
      .WillOnce(Return(StatusOr<BucketAccessControl>(TransientError())))
      .WillOnce(Invoke([&result](internal::PatchBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        internal::nl::json expected{{"role", "OWNER"}};
        auto payload = internal::nl::json::parse(r.payload());
        EXPECT_EQ(expected, payload);

        return make_status_or(result);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  StatusOr<BucketAccessControl> actual = client.PatchBucketAcl(
      "test-bucket", "user-test-user-1",
      BucketAccessControlPatchBuilder().set_role("OWNER"));
  ASSERT_TRUE(actual.ok()) << "status=" << actual.status();

  EXPECT_EQ(result, *actual);
}

TEST_F(BucketAccessControlsTest, PatchBucketAclTooManyFailures) {
  testing::TooManyFailuresStatusTest<BucketAccessControl>(
      mock, EXPECT_CALL(*mock, PatchBucketAcl(_)),
      [](Client& client) {
        return client
            .PatchBucketAcl("test-bucket", "user-test-user-1",
                            BucketAccessControlPatchBuilder())
            .status();
      },
      [](Client& client) {
        return client
            .PatchBucketAcl("test-bucket", "user-test-user-1",
                            BucketAccessControlPatchBuilder(),
                            IfMatchEtag("ABC="))
            .status();
      },
      "PatchBucketAcl");
}

TEST_F(BucketAccessControlsTest, PatchBucketAclPermanentFailure) {
  testing::PermanentFailureStatusTest<BucketAccessControl>(
      *client, EXPECT_CALL(*mock, PatchBucketAcl(_)),
      [](Client& client) {
        return client
            .PatchBucketAcl("test-bucket", "user-test-user-1",
                            BucketAccessControlPatchBuilder())
            .status();
      },
      "PatchBucketAcl");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
