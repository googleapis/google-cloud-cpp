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
  ClientOptions client_options = ClientOptions(CreateInsecureCredentials());
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

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
