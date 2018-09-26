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

#include "google/cloud/storage/notification_metadata.h"
#include "google/cloud/storage/notification_event_type.h"
#include "google/cloud/storage/notification_payload_format.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
using ::testing::HasSubstr;

NotificationMetadata CreateNotificationMetadataForTest() {
  std::string text = R"""({
      "custom_attributes": {
          "test-ca-1": "value1",
          "test-ca-2": "value2"
      },
      "etag": "XYZ=",
      "event_types": [
          "OBJECT_FINALIZE",
          "OBJECT_METADATA_UPDATE",
          "OBJECT_DELETE",
          "OBJECT_ARCHIVE"
      ],
      "id": "test-id-123",
      "kind": "storage#notification",
      "object_name_prefix": "test-prefix-",
      "payload_format": "JSON_API_V1",
      "selfLink": "https://www.googleapis.com/storage/v1/b/test-bucket/notificationConfigs/test-id-123",
      "topic": "test-topic"
})""";
  return NotificationMetadata::ParseFromString(text);
}

/// @test Verify that we parse JSON objects into NotificationMetadata objects.
TEST(NotificationMetadataTest, Parse) {
  auto actual = CreateNotificationMetadataForTest();
  EXPECT_EQ(2U, actual.custom_attributes().size());
  EXPECT_TRUE(actual.has_custom_attribute("test-ca-1"));
  EXPECT_EQ("value1", actual.custom_attribute("test-ca-1"));
  EXPECT_TRUE(actual.has_custom_attribute("test-ca-2"));
  EXPECT_EQ("value2", actual.custom_attribute("test-ca-2"));

  EXPECT_EQ("XYZ=", actual.etag());
  EXPECT_EQ(4U, actual.event_type_size());
  EXPECT_EQ(4U, actual.event_types().size());
  EXPECT_EQ(event_type::ObjectFinalize(), actual.event_type(0));
  EXPECT_EQ(event_type::ObjectMetadataUpdate(), actual.event_type(1));
  EXPECT_EQ(event_type::ObjectDelete(), actual.event_type(2));
  EXPECT_EQ(event_type::ObjectArchive(), actual.event_type(3));

  EXPECT_EQ("test-id-123", actual.id());
  EXPECT_EQ("storage#notification", actual.kind());
  EXPECT_EQ(payload_format::JsonApiV1(), actual.payload_format());
  EXPECT_EQ(
      "https://www.googleapis.com/storage/v1/b/test-bucket/"
      "notificationConfigs/test-id-123",
      actual.self_link());
  EXPECT_EQ("test-topic", actual.topic());
}

/// @test Verifies NotificationMetadata iostream operator.
TEST(NotificationMetadataTest, IOStream) {
  auto notification = CreateNotificationMetadataForTest();
  std::ostringstream os;
  os << notification;
  auto actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-ca-1"));
  EXPECT_THAT(actual, HasSubstr("value1"));
  EXPECT_THAT(actual, HasSubstr("test-ca-2"));
  EXPECT_THAT(actual, HasSubstr("value2"));
  EXPECT_THAT(actual, HasSubstr("XYZ="));
  EXPECT_THAT(actual, HasSubstr(event_type::ObjectFinalize()));
  EXPECT_THAT(actual, HasSubstr(event_type::ObjectMetadataUpdate()));
  EXPECT_THAT(actual, HasSubstr(event_type::ObjectDelete()));
  EXPECT_THAT(actual, HasSubstr(event_type::ObjectArchive()));
  EXPECT_THAT(actual, HasSubstr("test-id-123"));
  EXPECT_THAT(actual, HasSubstr("storage#notification"));
  EXPECT_THAT(actual, HasSubstr(payload_format::JsonApiV1()));
  EXPECT_THAT(actual, HasSubstr("https://www.googleapis.com/"));
  EXPECT_THAT(actual, HasSubstr("test-topic"));
}

/// @test Verifies NotificationMetadata::JsonPayloadForInsert.
TEST(NotificationMetadataTest, JsonPayloadForInsert) {
  auto notification = CreateNotificationMetadataForTest();
  auto text = notification.JsonPayloadForInsert();
  internal::nl::json actual = internal::nl::json::parse(text);

  internal::nl::json expected_attributes{
      {"test-ca-1", "value1"},
      {"test-ca-2", "value2"},
  };
  std::vector<std::string> expected_event_types{
      "OBJECT_FINALIZE",
      "OBJECT_METADATA_UPDATE",
      "OBJECT_DELETE",
      "OBJECT_ARCHIVE",
  };
  internal::nl::json expected{
      {"custom_attributes", expected_attributes},
      {"topic", "test-topic"},
      {"payload_format", "JSON_API_V1"},
      {"event_types", expected_event_types},
      {"object_name_prefix", "test-prefix-"},
  };

  auto diff = internal::nl::json::diff(expected, actual);
  EXPECT_EQ("[]", diff.dump()) << " text=" << text;
}

/// @test Verify we can make changes to the custom attributes.
TEST(NotificationMetadataTest, MutableCustomAttributes) {
  auto expected = CreateNotificationMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  copy.mutable_custom_attributes().emplace("test-ca-3", "value3");
  EXPECT_TRUE(copy.has_custom_attribute("test-ca-3"));
  EXPECT_EQ("value3", copy.custom_attribute("test-ca-3"));
  EXPECT_NE(expected, copy);
}

/// @test Verify we can delete custom attributes.
TEST(NotificationMetadataTest, DeleteCustomAttribute) {
  auto expected = CreateNotificationMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  copy.delete_custom_attribute("test-ca-1");
  EXPECT_FALSE(copy.has_custom_attribute("test-ca-1"));
  EXPECT_NE(expected, copy);
}

/// @test Verify we can update and insert custom attributes.
TEST(NotificationMetadataTest, UpsertCustomAttribute) {
  auto expected = CreateNotificationMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  EXPECT_TRUE(copy.has_custom_attribute("test-ca-1"));
  EXPECT_EQ("value1", copy.custom_attribute("test-ca-1"));
  copy.upsert_custom_attributes("test-ca-3", "value3");
  copy.upsert_custom_attributes("test-ca-1", "value1-updated");
  EXPECT_EQ("value1-updated", copy.custom_attribute("test-ca-1"));
  EXPECT_EQ("value3", copy.custom_attribute("test-ca-3"));
  EXPECT_NE(expected, copy);
}

/// @test Verify we can make changes to the event types.
TEST(NotificationMetadataTest, MutableEventTypes) {
  auto expected = CreateNotificationMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  copy.mutable_event_types().pop_back();
  EXPECT_EQ(3U, copy.event_type_size());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can make changes to the event types.
TEST(NotificationMetadataTest, AppendEventTypes) {
  auto expected = CreateNotificationMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  copy.mutable_event_types().clear();
  EXPECT_EQ(0U, copy.event_type_size());
  copy.append_event_type(event_type::ObjectFinalize());
  EXPECT_EQ(1U, copy.event_type_size());
  EXPECT_EQ("OBJECT_FINALIZE", copy.event_type(0));
  EXPECT_NE(expected, copy);
}

/// @test Verify we can make changes to the object name prefix.
TEST(NotificationMetadataTest, SetObjectNamePrefix) {
  auto expected = CreateNotificationMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  EXPECT_EQ("test-prefix-", copy.object_name_prefix());
  copy.set_object_name_prefix("another-prefix/");
  EXPECT_EQ("another-prefix/", copy.object_name_prefix());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can make changes to the payload format.
TEST(NotificationMetadataTest, SetPayloadFormat) {
  auto expected = CreateNotificationMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  EXPECT_EQ("JSON_API_V1", copy.payload_format());
  copy.set_payload_format(payload_format::None());
  EXPECT_EQ("NONE", copy.payload_format());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can make changes to the topic.
TEST(NotificationMetadataTest, SetTopic) {
  auto expected = CreateNotificationMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  EXPECT_EQ("test-topic", copy.topic());
  copy.set_topic("another-topic");
  EXPECT_EQ("another-topic", copy.topic());
  EXPECT_NE(expected, copy);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
