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
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
StatusOr<NotificationMetadata> NotificationMetadataParser::FromJson(
    internal::nl::json const& json) {
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  NotificationMetadata result{};

  if (json.count("custom_attributes") != 0) {
    for (auto const& kv : json["custom_attributes"].items()) {
      result.custom_attributes_.emplace(kv.key(),
                                        kv.value().get<std::string>());
    }
  }
  result.etag_ = json.value("etag", "");

  if (json.count("event_types") != 0) {
    for (auto const& kv : json["event_types"].items()) {
      result.event_types_.emplace_back(kv.value().get<std::string>());
    }
  }

  result.id_ = json.value("id", "");
  result.kind_ = json.value("kind", "");
  result.object_name_prefix_ = json.value("object_name_prefix", "");
  result.payload_format_ = json.value("payload_format", "");
  result.self_link_ = json.value("selfLink", "");
  result.topic_ = json.value("topic", "");

  return result;
}

StatusOr<NotificationMetadata> NotificationMetadataParser::FromString(
    std::string const& payload) {
  internal::nl::json json = internal::nl::json::parse(payload, nullptr, false);
  return FromJson(json);
}

std::ostream& operator<<(std::ostream& os, ListNotificationsRequest const& r) {
  os << "ListNotificationsRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<ListNotificationsResponse> ListNotificationsResponse::FromHttpResponse(
    std::string const& payload) {
  auto json = nl::json::parse(payload, nullptr, false);
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  ListNotificationsResponse result;
  for (auto const& kv : json["items"].items()) {
    auto parsed = internal::NotificationMetadataParser::FromJson(kv.value());
    if (!parsed.ok()) {
      return std::move(parsed).status();
    }
    result.items.emplace_back(std::move(*parsed));
  }
  return result;
}

std::ostream& operator<<(std::ostream& os, ListNotificationsResponse const& r) {
  os << "ListNotificationResponse={items={";
  char const* sep = "";
  for (auto const& acl : r.items) {
    os << sep << acl;
    sep = ", ";
  }
  return os << "}}";
}

std::ostream& operator<<(std::ostream& os, CreateNotificationRequest const& r) {
  os << "CreateNotificationRequest={bucket_name=" << r.bucket_name()
     << ", json_payload=" << r.json_payload();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, GetNotificationRequest const& r) {
  os << "GetNotificationRequest={bucket_name=" << r.bucket_name()
     << ", notification_id" << r.notification_id();
  r.DumpOptions(os, ", ");
  return os << "}";
}

std::ostream& operator<<(std::ostream& os, DeleteNotificationRequest const& r) {
  os << "DeleteNotificationRequest={bucket_name=" << r.bucket_name()
     << ", notification_id" << r.notification_id();
  r.DumpOptions(os, ", ");
  return os << "}";
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
