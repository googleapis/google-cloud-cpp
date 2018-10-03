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
      })"""),
      BucketAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-2",
          "role": "READER"
      })"""),
  };

  EXPECT_CALL(*mock, ListBucketAcl(_))
      .WillOnce(Return(
          std::make_pair(TransientError(), internal::ListBucketAclResponse{})))
      .WillOnce(Invoke([&expected](internal::ListBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());

        return std::make_pair(Status(),
                              internal::ListBucketAclResponse{expected});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  std::vector<BucketAccessControl> actual = client.ListBucketAcl("test-bucket");
  EXPECT_EQ(expected, actual);
}

TEST_F(BucketAccessControlsTest, ListBucketAclTooManyFailures) {
  testing::TooManyFailuresTest<internal::ListBucketAclResponse>(
      mock, EXPECT_CALL(*mock, ListBucketAcl(_)),
      [](Client& client) { client.ListBucketAcl("test-bucket-name"); },
      "ListBucketAcl");
}

TEST_F(BucketAccessControlsTest, ListBucketAclPermanentFailure) {
  testing::PermanentFailureTest<internal::ListBucketAclResponse>(
      *client, EXPECT_CALL(*mock, ListBucketAcl(_)),
      [](Client& client) { client.ListBucketAcl("test-bucket-name"); },
      "ListBucketAcl");
}

TEST_F(BucketAccessControlsTest, CreateBucketAcl) {
  auto expected = BucketAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "READER"
      })""");

  EXPECT_CALL(*mock, CreateBucketAcl(_))
      .WillOnce(Return(std::make_pair(TransientError(), BucketAccessControl{})))
      .WillOnce(Invoke([&expected](internal::CreateBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        EXPECT_EQ("READER", r.role());

        return std::make_pair(Status(), expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  BucketAccessControl actual = client.CreateBucketAcl(
      "test-bucket", "user-test-user-1", BucketAccessControl::ROLE_READER());
  // Compare just a few fields because the values for most of the fields are
  // hard to predict when testing against the production environment.
  EXPECT_EQ(expected.bucket(), actual.bucket());
  EXPECT_EQ(expected.entity(), actual.entity());
  EXPECT_EQ(expected.role(), actual.role());
}

TEST_F(BucketAccessControlsTest, CreateBucketAclTooManyFailures) {
  testing::TooManyFailuresTest<BucketAccessControl>(
      mock, EXPECT_CALL(*mock, CreateBucketAcl(_)),
      [](Client& client) {
        client.CreateBucketAcl("test-bucket-name", "user-test-user-1",
                               "READER");
      },
      "CreateBucketAcl");
}

TEST_F(BucketAccessControlsTest, CreateBucketAclPermanentFailure) {
  testing::PermanentFailureTest<BucketAccessControl>(
      *client, EXPECT_CALL(*mock, CreateBucketAcl(_)),
      [](Client& client) {
        client.CreateBucketAcl("test-bucket-name", "user-test-user", "READER");
      },
      "CreateBucketAcl");
}

TEST_F(BucketAccessControlsTest, DeleteBucketAcl) {
  EXPECT_CALL(*mock, DeleteBucketAcl(_))
      .WillOnce(
          Return(std::make_pair(TransientError(), internal::EmptyResponse{})))
      .WillOnce(Invoke([](internal::DeleteBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());

        return std::make_pair(Status(), internal::EmptyResponse{});
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  client.DeleteBucketAcl("test-bucket", "user-test-user-1");
}

TEST_F(BucketAccessControlsTest, DeleteBucketAclTooManyFailures) {
  testing::TooManyFailuresTest<internal::EmptyResponse>(
      mock, EXPECT_CALL(*mock, DeleteBucketAcl(_)),
      [](Client& client) {
        client.DeleteBucketAcl("test-bucket-name", "user-test-user-1");
      },
      "DeleteBucketAcl");
}

TEST_F(BucketAccessControlsTest, DeleteBucketAclPermanentFailure) {
  testing::PermanentFailureTest<internal::EmptyResponse>(
      *client, EXPECT_CALL(*mock, DeleteBucketAcl(_)),
      [](Client& client) {
        client.DeleteBucketAcl("test-bucket-name", "user-test-user");
      },
      "DeleteBucketAcl");
}

TEST_F(BucketAccessControlsTest, GetBucketAcl) {
  BucketAccessControl expected = BucketAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""");

  EXPECT_CALL(*mock, GetBucketAcl(_))
      .WillOnce(Return(std::make_pair(TransientError(), BucketAccessControl{})))
      .WillOnce(Invoke([&expected](internal::GetBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());

        return std::make_pair(Status(), expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  BucketAccessControl actual =
      client.GetBucketAcl("test-bucket", "user-test-user-1");
  EXPECT_EQ(expected, actual);
}

TEST_F(BucketAccessControlsTest, GetBucketAclTooManyFailures) {
  testing::TooManyFailuresTest<BucketAccessControl>(
      mock, EXPECT_CALL(*mock, GetBucketAcl(_)),
      [](Client& client) {
        client.GetBucketAcl("test-bucket-name", "user-test-user-1");
      },
      "GetBucketAcl");
}

TEST_F(BucketAccessControlsTest, GetBucketAclPermanentFailure) {
  testing::PermanentFailureTest<BucketAccessControl>(
      *client, EXPECT_CALL(*mock, GetBucketAcl(_)),
      [](Client& client) {
        client.GetBucketAcl("test-bucket-name", "user-test-user-1");
      },
      "GetBucketAcl");
}

TEST_F(BucketAccessControlsTest, UpdateBucketAcl) {
  BucketAccessControl expected = BucketAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""");

  EXPECT_CALL(*mock, UpdateBucketAcl(_))
      .WillOnce(Return(std::make_pair(TransientError(), BucketAccessControl{})))
      .WillOnce(Invoke([&expected](internal::UpdateBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        EXPECT_EQ("OWNER", r.role());

        return std::make_pair(Status(), expected);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  BucketAccessControl actual = client.UpdateBucketAcl(
      "test-bucket",
      BucketAccessControl().set_entity("user-test-user-1").set_role("OWNER"));
  EXPECT_EQ(expected, actual);
}

TEST_F(BucketAccessControlsTest, UpdateBucketAclTooManyFailures) {
  testing::TooManyFailuresTest<BucketAccessControl>(
      mock, EXPECT_CALL(*mock, UpdateBucketAcl(_)),
      [](Client& client) {
        client.UpdateBucketAcl("test-bucket",
                               BucketAccessControl()
                                   .set_entity("user-test-user-1")
                                   .set_role("OWNER"));
      },
      "UpdateBucketAcl");
}

TEST_F(BucketAccessControlsTest, UpdateBucketAclPermanentFailure) {
  testing::PermanentFailureTest<BucketAccessControl>(
      *client, EXPECT_CALL(*mock, UpdateBucketAcl(_)),
      [](Client& client) {
        client.UpdateBucketAcl("test-bucket",
                               BucketAccessControl()
                                   .set_entity("user-test-user-1")
                                   .set_role("OWNER"));
      },
      "UpdateBucketAcl");
}

TEST_F(BucketAccessControlsTest, PatchBucketAcl) {
  BucketAccessControl result = BucketAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })""");

  EXPECT_CALL(*mock, PatchBucketAcl(_))
      .WillOnce(Return(std::make_pair(TransientError(), BucketAccessControl{})))
      .WillOnce(Invoke([&result](internal::PatchBucketAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        internal::nl::json expected{{"role", "OWNER"}};
        auto payload = internal::nl::json::parse(r.payload());
        EXPECT_EQ(expected, payload);

        return std::make_pair(Status(), result);
      }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  BucketAccessControl actual = client.PatchBucketAcl(
      "test-bucket", "user-test-user-1",
      BucketAccessControlPatchBuilder().set_role("OWNER"));
  EXPECT_EQ(result, actual);
}

TEST_F(BucketAccessControlsTest, PatchBucketAclTooManyFailures) {
  testing::TooManyFailuresTest<BucketAccessControl>(
      mock, EXPECT_CALL(*mock, PatchBucketAcl(_)),
      [](Client& client) {
        client.PatchBucketAcl("test-bucket", "user-test-user-1",
                              BucketAccessControlPatchBuilder());
      },
      "PatchBucketAcl");
}

TEST_F(BucketAccessControlsTest, PatchBucketAclPermanentFailure) {
  testing::PermanentFailureTest<BucketAccessControl>(
      *client, EXPECT_CALL(*mock, PatchBucketAcl(_)),
      [](Client& client) {
        client.PatchBucketAcl("test-bucket", "user-test-user-1",
                              BucketAccessControlPatchBuilder());
      },
      "PatchBucketAcl");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
