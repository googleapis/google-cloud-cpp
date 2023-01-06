// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/notification_requests.h"
#include "google/cloud/storage/internal/notification_metadata_parser.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
std::ostream& operator<<(std::ostream& os, ListNotificationsRequest const& r) {
  os << "ListNotificationsRequest={bucket_name=" << r.bucket_name();
  r.DumpOptions(os, ", ");
  return os << "}";
}

StatusOr<ListNotificationsResponse> ListNotificationsResponse::FromHttpResponse(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
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

StatusOr<ListNotificationsResponse> ListNotificationsResponse::FromHttpResponse(
    HttpResponse const& response) {
  return FromHttpResponse(response.payload);
}

std::ostream& operator<<(std::ostream& os, ListNotificationsResponse const& r) {
  os << "ListNotificationResponse={items={";
  os << absl::StrJoin(r.items, ", ", absl::StreamFormatter());
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
