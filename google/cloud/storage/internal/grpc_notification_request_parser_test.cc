// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/grpc_notification_request_parser.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

namespace v2 = ::google::storage::v2;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::ElementsAre;

TEST(GrpcNotificationRequestParser, DeleteNotification) {
  v2::DeleteNotificationRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "projects/_/buckets/test-bucket-name/notificationConfigs/test-notification-id"
      )pb",
      &expected));

  DeleteNotificationRequest req("test-bucket-name", "test-notification-id");

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcNotificationRequestParser, GetNotification) {
  v2::GetNotificationRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "projects/_/buckets/test-bucket-name/notificationConfigs/test-notification-id"
      )pb",
      &expected));

  GetNotificationRequest req("test-bucket-name", "test-notification-id");

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcNotificationRequestParser, CreateNotification) {
  v2::CreateNotificationRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parent: "projects/_/buckets/test-bucket-name"
        notification {
          topic: "//pubsub.googleapis.com/projects/test-topic-project/topics/test-topic-id"
          event_types: "OBJECT_DELETE"
          event_types: "OBJECT_ARCHIVE"
          custom_attributes { key: "test-key-0" value: "test-value-0" }
          custom_attributes { key: "test-key-1" value: "test-value-1" }
          object_name_prefix: "test-object-name-prefix/"
          payload_format: "JSON_API_V1"
        }
      )pb",
      &expected));

  CreateNotificationRequest req(
      "test-bucket-name",
      NotificationMetadata()
          .set_topic("projects/test-topic-project/topics/test-topic-id")
          .append_event_type("OBJECT_DELETE")
          .append_event_type("OBJECT_ARCHIVE")
          .upsert_custom_attributes("test-key-0", "test-value-0")
          .upsert_custom_attributes("test-key-1", "test-value-1")
          .set_object_name_prefix("test-object-name-prefix/")
          .set_payload_format("JSON_API_V1"));
  req.set_multiple_options(UserProject("test-user-project"));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcNotificationRequestParser, ListNotifications) {
  v2::ListNotificationsRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parent: "projects/_/buckets/test-bucket-name"
      )pb",
      &expected));

  ListNotificationsRequest req("test-bucket-name");

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcNotificationRequestParser, ListNotificationsResponse) {
  v2::ListNotificationsResponse input;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        notifications {
          name: "projects/_/buckets/test-bucket-name/notificationConfigs/test-id-1"
          etag: "test-etag-1"
          topic: "//pubsub.googleapis.com/projects/test-topic-project/topics/test-topic-id-1"
          event_types: "OBJECT_DELETE"
          event_types: "OBJECT_ARCHIVE"
          custom_attributes { key: "test-key-0" value: "test-value-0" }
          custom_attributes { key: "test-key-1" value: "test-value-1" }
          object_name_prefix: "test-object-name-prefix/"
          payload_format: "JSON_API_V1"
        }
        notifications {
          name: "projects/_/buckets/test-bucket-name/notificationConfigs/test-id-2"
          etag: "test-etag-2"
          topic: "//pubsub.googleapis.com/projects/test-topic-project/topics/test-topic-id-2"
          payload_format: "JSON_API_V1"
        }
      )pb",
      &input));

  auto const actual = FromProto(input);
  EXPECT_THAT(
      actual.items,
      ElementsAre(
          NotificationMetadata("test-id-1", "test-etag-1")
              .set_topic("projects/test-topic-project/topics/test-topic-id-1")
              .append_event_type("OBJECT_DELETE")
              .append_event_type("OBJECT_ARCHIVE")
              .upsert_custom_attributes("test-key-0", "test-value-0")
              .upsert_custom_attributes("test-key-1", "test-value-1")
              .set_object_name_prefix("test-object-name-prefix/")
              .set_payload_format("JSON_API_V1"),
          NotificationMetadata("test-id-2", "test-etag-2")
              .set_topic("projects/test-topic-project/topics/test-topic-id-2")
              .set_payload_format("JSON_API_V1")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
