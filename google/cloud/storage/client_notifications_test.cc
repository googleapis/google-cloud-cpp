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
#include "google/cloud/storage/internal/notification_metadata_parser.h"
#include "google/cloud/storage/notification_event_type.h"
#include "google/cloud/storage/notification_payload_format.h"
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
using ::testing::HasSubstr;
using ::testing::Return;
using ms = std::chrono::milliseconds;

/**
 * Test the BucketAccessControls-related functions in storage::Client.
 */
class NotificationsTest
    : public ::google::cloud::storage::testing::ClientUnitTest {};

TEST_F(NotificationsTest, ListNotifications) {
  std::vector<NotificationMetadata> expected{
      internal::NotificationMetadataParser::FromString(R"""({
          "id": "test-notification-1",
          "topic": "test-topic-1"
      })""")
          .value(),
      internal::NotificationMetadataParser::FromString(R"""({
          "id": "test-notification-2",
          "topic": "test-topic-2"
      })""")
          .value(),
  };

  EXPECT_CALL(*mock_, ListNotifications)
      .WillOnce(Return(
          StatusOr<internal::ListNotificationsResponse>(TransientError())))
      .WillOnce([&expected](internal::ListNotificationsRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());

        return make_status_or(internal::ListNotificationsResponse{expected});
      });
  auto client = ClientForMock();
  StatusOr<std::vector<NotificationMetadata>> actual =
      client.ListNotifications("test-bucket");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, actual.value());
}

TEST_F(NotificationsTest, ListNotificationsTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::ListNotificationsResponse>(
      mock_, EXPECT_CALL(*mock_, ListNotifications),
      [](Client& client) {
        return client.ListNotifications("test-bucket-name").status();
      },
      "ListNotifications");
}

TEST_F(NotificationsTest, ListNotificationsPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<internal::ListNotificationsResponse>(
      client, EXPECT_CALL(*mock_, ListNotifications),
      [](Client& client) {
        return client.ListNotifications("test-bucket-name").status();
      },
      "ListNotifications");
}

TEST_F(NotificationsTest, CreateNotification) {
  NotificationMetadata expected =
      internal::NotificationMetadataParser::FromString(R"""({
          "id": "test-notification-1",
          "topic": "test-topic-1",
          "payload_format": "JSON_API_V1",
          "object_prefix": "test-object-prefix-",
          "event_type": [ "OBJECT_FINALIZE" ]
      })""")
          .value();

  EXPECT_CALL(*mock_, CreateNotification)
      .WillOnce(Return(StatusOr<NotificationMetadata>(TransientError())))
      .WillOnce([&expected](internal::CreateNotificationRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_THAT(r.json_payload(), HasSubstr("test-topic-1"));
        EXPECT_THAT(r.json_payload(), HasSubstr("JSON_API_V1"));
        EXPECT_THAT(r.json_payload(), HasSubstr("test-object-prefix-"));
        EXPECT_THAT(r.json_payload(), HasSubstr("OBJECT_FINALIZE"));

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<NotificationMetadata> actual = client.CreateNotification(
      "test-bucket", "test-topic-1", payload_format::JsonApiV1(),
      NotificationMetadata()
          .set_object_name_prefix("test-object-prefix-")
          .append_event_type(event_type::ObjectFinalize()));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, actual.value());
}

TEST_F(NotificationsTest, CreateNotificationTooManyFailures) {
  testing::TooManyFailuresStatusTest<NotificationMetadata>(
      mock_, EXPECT_CALL(*mock_, CreateNotification),
      [](Client& client) {
        return client
            .CreateNotification("test-bucket-name", "test-topic-1",
                                payload_format::JsonApiV1(),
                                NotificationMetadata())
            .status();
      },
      "CreateNotification");
}

TEST_F(NotificationsTest, CreateNotificationPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<NotificationMetadata>(
      client, EXPECT_CALL(*mock_, CreateNotification),
      [](Client& client) {
        return client
            .CreateNotification("test-bucket-name", "test-topic-1",
                                payload_format::JsonApiV1(),
                                NotificationMetadata())
            .status();
      },
      "CreateNotification");
}

TEST_F(NotificationsTest, GetNotification) {
  NotificationMetadata expected =
      internal::NotificationMetadataParser::FromString(R"""({
          "id": "test-notification-1",
          "topic": "test-topic-1",
          "payload_format": "JSON_API_V1",
          "object_prefix": "test-object-prefix-",
          "event_type": [ "OBJECT_FINALIZE" ]
      })""")
          .value();

  EXPECT_CALL(*mock_, GetNotification)
      .WillOnce(Return(StatusOr<NotificationMetadata>(TransientError())))
      .WillOnce([&expected](internal::GetNotificationRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-notification-1", r.notification_id());

        return make_status_or(expected);
      });
  auto client = ClientForMock();
  StatusOr<NotificationMetadata> actual =
      client.GetNotification("test-bucket", "test-notification-1");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, actual.value());
}

TEST_F(NotificationsTest, GetNotificationTooManyFailures) {
  testing::TooManyFailuresStatusTest<NotificationMetadata>(
      mock_, EXPECT_CALL(*mock_, GetNotification),
      [](Client& client) {
        return client.GetNotification("test-bucket-name", "test-notification-1")
            .status();
      },
      "GetNotification");
}

TEST_F(NotificationsTest, GetNotificationPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<NotificationMetadata>(
      client, EXPECT_CALL(*mock_, GetNotification),
      [](Client& client) {
        return client.GetNotification("test-bucket-name", "test-notification-1")
            .status();
      },
      "GetNotification");
}

TEST_F(NotificationsTest, DeleteNotification) {
  EXPECT_CALL(*mock_, DeleteNotification)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce([](internal::DeleteNotificationRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("test-notification-1", r.notification_id());

        return make_status_or(internal::EmptyResponse{});
      });
  auto client = ClientForMock();
  auto status = client.DeleteNotification("test-bucket", "test-notification-1");
  ASSERT_STATUS_OK(status);
}

TEST_F(NotificationsTest, DeleteNotificationTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::EmptyResponse>(
      mock_, EXPECT_CALL(*mock_, DeleteNotification),
      [](Client& client) {
        return client.DeleteNotification("test-bucket-name",
                                         "test-notification-1");
      },
      "DeleteNotification");
}

TEST_F(NotificationsTest, DeleteNotificationPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<internal::EmptyResponse>(
      client, EXPECT_CALL(*mock_, DeleteNotification),
      [](Client& client) {
        return client.DeleteNotification("test-bucket-name",
                                         "test-notification-1");
      },
      "DeleteNotification");
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
