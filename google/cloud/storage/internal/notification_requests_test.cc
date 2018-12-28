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

#include "google/cloud/storage/internal/notification_requests.h"
#include "google/cloud/storage/notification_payload_format.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;

TEST(NotificationRequestTest, List) {
  ListNotificationsRequest request("my-bucket");
  request.set_multiple_options(UserProject("project-for-billing"));
  EXPECT_EQ("my-bucket", request.bucket_name());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=project-for-billing"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
}

TEST(NotificationRequestTest, ListResponse) {
  std::string text = R"""({
      "items": [{
          "id": "test-notification-id-1",
          "topic": "test-topic-1"
      }, {
          "id": "test-notification-id-2",
          "topic": "test-topic-2"
      }]})""";

  auto actual =
      ListNotificationsResponse::FromHttpResponse(HttpResponse{200, text, {}})
          .value();
  ASSERT_EQ(2UL, actual.items.size());
  EXPECT_EQ("test-notification-id-1", actual.items.at(0).id());
  EXPECT_EQ("test-topic-1", actual.items.at(0).topic());
  EXPECT_EQ("test-notification-id-2", actual.items.at(1).id());
  EXPECT_EQ("test-topic-2", actual.items.at(1).topic());

  std::ostringstream os;
  os << actual;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("id=test-notification-id-1"));
  EXPECT_THAT(str, HasSubstr("id=test-notification-id-2"));
  EXPECT_THAT(str, HasSubstr("ListNotificationResponse={"));
  EXPECT_THAT(str, HasSubstr("NotificationMetadata={"));
}

TEST(NotificationRequestTest, ListResponseParseFailure) {
  std::string text = R"""({123)""";

  StatusOr<ListNotificationsResponse> actual =
      ListNotificationsResponse::FromHttpResponse(HttpResponse{200, text, {}});
  EXPECT_FALSE(actual.ok());
}

TEST(NotificationRequestTest, ListResponseParseFailureListElements) {
  std::string text = R"""({
      "items": [{
          "id": "test-notification-id-1",
          "topic": "test-topic-1"
      }, "invalid-element"]})""";

  StatusOr<ListNotificationsResponse> actual =
      ListNotificationsResponse::FromHttpResponse(HttpResponse{200, text, {}});
  EXPECT_FALSE(actual.ok());
}

TEST(CreateNotificationRequestTest, Create) {
  NotificationMetadata notification =
      NotificationMetadata()
          .set_topic("test-topic-1")
          .set_payload_format(payload_format::JsonApiV1())
          .set_object_name_prefix("test-object-prefix-");
  CreateNotificationRequest request("my-bucket", notification);
  request.set_multiple_options(UserProject("project-for-billing"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ(notification.JsonPayloadForInsert(), request.json_payload());

  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=project-for-billing"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr(notification.JsonPayloadForInsert()));
}

TEST(NotificationRequestTest, Get) {
  GetNotificationRequest request("my-bucket", "test-notification-id");
  request.set_multiple_options(UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("test-notification-id", request.notification_id());
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("test-notification-id"));
}

TEST(NotificationRequestTest, Delete) {
  DeleteNotificationRequest request("my-bucket", "test-notification-id");
  request.set_multiple_options(UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("test-notification-id", request.notification_id());
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("userProject=my-project"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("test-notification-id"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
