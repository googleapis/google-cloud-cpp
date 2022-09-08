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

#include "google/cloud/storage/internal/grpc_notification_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_bucket_name.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

NotificationMetadata FromProto(google::storage::v2::Notification const& rhs) {
  std::vector<absl::string_view> components = absl::StrSplit(rhs.name(), '/');
  auto id = components.empty() ? std::string{} : std::string{components.back()};

  NotificationMetadata result(std::move(id), rhs.etag());
  absl::string_view topic = rhs.topic();
  absl::ConsumePrefix(&topic, "//pubsub.googleapis.com/");
  result.set_topic(std::string{topic});
  for (auto const& e : rhs.event_types()) {
    result.append_event_type(e);
  }
  for (auto const& kv : rhs.custom_attributes()) {
    result.upsert_custom_attributes(kv.first, kv.second);
  }
  result.set_object_name_prefix(rhs.object_name_prefix());
  result.set_payload_format(rhs.payload_format());
  return result;
}

google::storage::v2::Notification ToProto(NotificationMetadata const& rhs) {
  google::storage::v2::Notification result;
  result.set_topic("//pubsub.googleapis.com/" + rhs.topic());
  result.set_etag(rhs.etag());
  for (auto const& e : rhs.event_types()) {
    result.add_event_types(e);
  }
  for (auto const& kv : rhs.custom_attributes()) {
    (*result.mutable_custom_attributes())[kv.first] = kv.second;
  }
  result.set_object_name_prefix(rhs.object_name_prefix());
  result.set_payload_format(rhs.payload_format());
  return result;
}

google::storage::v2::Notification ToProto(NotificationMetadata const& rhs,
                                          std::string const& bucket_name) {
  auto result = ToProto(rhs);
  result.set_name(BucketNameToProto(bucket_name) + "/notificationConfigs/" +
                  rhs.id());
  return result;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
