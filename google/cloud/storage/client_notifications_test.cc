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
class NotificationsTest : public ::testing::Test {
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

TEST_F(NotificationsTest, ListNotifications) {
  std::vector<NotificationMetadata> expected{
      NotificationMetadata::ParseFromString(R"""({
          "id": "test-notification-1",
          "topic": "test-topic-1"
      })"""),
      NotificationMetadata::ParseFromString(R"""({
          "id": "test-notification-2",
          "topic": "test-topic-2"
      })"""),
  };

  EXPECT_CALL(*mock, ListNotifications(_))
      .WillOnce(Return(std::make_pair(TransientError(),
                                      internal::ListNotificationsResponse{})))
      .WillOnce(
          Invoke([&expected](internal::ListNotificationsRequest const& r) {
            EXPECT_EQ("test-bucket", r.bucket_name());

            return std::make_pair(
                Status(), internal::ListNotificationsResponse{expected});
          }));
  Client client{std::shared_ptr<internal::RawClient>(mock)};

  std::vector<NotificationMetadata> actual =
      client.ListNotifications("test-bucket");
  EXPECT_EQ(expected, actual);
}

TEST_F(NotificationsTest, ListNotificationsTooManyFailures) {
  testing::TooManyFailuresTest<internal::ListNotificationsResponse>(
      mock, EXPECT_CALL(*mock, ListNotifications(_)),
      [](Client& client) { client.ListNotifications("test-bucket-name"); },
      "ListNotifications");
}

TEST_F(NotificationsTest, ListNotificationsPermanentFailure) {
  testing::PermanentFailureTest<internal::ListNotificationsResponse>(
      *client, EXPECT_CALL(*mock, ListNotifications(_)),
      [](Client& client) { client.ListNotifications("test-bucket-name"); },
      "ListNotifications");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
