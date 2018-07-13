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
using namespace testing::canonical_errors;
namespace {
using ms = std::chrono::milliseconds;
using namespace ::testing;

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
  ClientOptions client_options = ClientOptions(CreateInsecureCredentials());
};

TEST_F(ObjectAccessControlsTest, ListObjectAcl) {
  std::vector<ObjectAccessControl> expected{
      ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-1",
          "role": "OWNER"
      })"""),
      ObjectAccessControl::ParseFromString(R"""({
          "bucket": "test-bucket",
          "object": "test-object",
          "entity": "user-test-user-2",
          "role": "READER"
      })"""),
  };

  EXPECT_CALL(*mock, ListObjectAcl(_))
      .WillOnce(Return(
          std::make_pair(TransientError(), internal::ListObjectAclResponse{})))
      .WillOnce(Invoke([&expected](internal::ListObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());

        return std::make_pair(Status(),
                              internal::ListObjectAclResponse{expected});
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
      })""");

  EXPECT_CALL(*mock, CreateObjectAcl(_))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectAccessControl{})))
      .WillOnce(Invoke([&expected](internal::CreateObjectAclRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-object", r.object_name());
        EXPECT_EQ("user-test-user-1", r.entity());
        EXPECT_EQ("READER", r.role());

        return std::make_pair(Status(), expected);
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
  Client client{std::shared_ptr<internal::RawClient>(mock),
                LimitedErrorCountRetryPolicy(2)};

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, CreateObjectAcl(_))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectAccessControl{})))
      .WillOnce(Return(std::make_pair(TransientError(), ObjectAccessControl{})))
      .WillOnce(
          Return(std::make_pair(TransientError(), ObjectAccessControl{})));
  EXPECT_THROW(
      try {
        client.CreateObjectAcl("test-bucket", "test-object", "user-test-user",
                               "READER");
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("Retry policy exhausted"));
        EXPECT_THAT(ex.what(), HasSubstr("CreateObjectAcl"));
        throw;
      },
      std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, CreateObjectAcl(_))
      .WillRepeatedly(
          Return(std::make_pair(TransientError(), ObjectAccessControl{})));
  EXPECT_DEATH_IF_SUPPORTED(client.CreateObjectAcl("test-bucket", "test-object",
                                                   "user-test-user", "READER"),
                            "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST_F(ObjectAccessControlsTest, CreateObjectAclPermanentFailure) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_CALL(*mock, CreateObjectAcl(_))
      .WillOnce(
          Return(std::make_pair(PermanentError(), ObjectAccessControl{})));
  EXPECT_THROW(
      try {
        client->CreateObjectAcl("test-bucket", "test-object", "user-test-user",
                                "READER");
      } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), HasSubstr("Permanent error"));
        EXPECT_THAT(ex.what(), HasSubstr("CreateObjectAcl"));
        throw;
      },
      std::runtime_error);
#else
  // With EXPECT_DEATH*() the mocking framework cannot detect how many times the
  // operation is called.
  EXPECT_CALL(*mock, CreateObjectAcl(_))
      .WillRepeatedly(
          Return(std::make_pair(PermanentError(), ObjectAccessControl{})));
  EXPECT_DEATH_IF_SUPPORTED(
      client->CreateObjectAcl("test-bucket", "test-object", "user-test-user",
                              "READER"),
      "exceptions are disabled");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

}  // namespace
}  // namespace storage
}  // namespace cloud
}  // namespace google
