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
std::ostream& operator<<(std::ostream& os, ListNotificationsRequest const& r) {
  os << "ListNotificationsRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<ListNotificationsResponse> ListNotificationsResponse::FromHttpResponse(
    HttpResponse&& response) {
  auto json = nl::json::parse(response.payload, nullptr, false);
  if (not json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  ListNotificationsResponse result;
  for (auto const& kv : json["items"].items()) {
    auto parsed = NotificationMetadata::ParseFromJson(kv.value());
    if (not parsed.ok()) {
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
