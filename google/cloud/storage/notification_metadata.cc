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
#include "google/cloud/storage/internal/notification_requests.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
std::string NotificationMetadata::JsonPayloadForInsert() const {
  // Required fields, always include them, even if empty.
  nlohmann::json json{
      {"topic", topic()},
      {"payload_format", payload_format()},
  };

  // custom_attributes()
  if (!custom_attributes().empty()) {
    nlohmann::json ca;
    for (auto const& kv : custom_attributes()) {
      ca[kv.first] = kv.second;
    }
    json["custom_attributes"] = ca;
  }

  // event_types()
  if (!event_types().empty()) {
    nlohmann::json events;
    for (auto const& v : event_types()) {
      events.push_back(v);
    }
    json["event_types"] = events;
  }

  if (!object_name_prefix().empty()) {
    json["object_name_prefix"] = object_name_prefix();
  }

  return json.dump();
}

std::ostream& operator<<(std::ostream& os, NotificationMetadata const& rhs) {
  os << "NotificationMetadata={id=" << rhs.id();

  // custom_attributes()
  char const* sep = "custom_attributes.";
  for (auto const& kv : rhs.custom_attributes()) {
    os << sep << kv.first << "=" << kv.second;
    sep = ", custom_attributes.";
  }

  os << ", etag=" << rhs.etag();

  // event_types()
  os << ", event_types=[";
  sep = "";
  for (auto const& v : rhs.event_types()) {
    os << sep << v;
    sep = ", ";
  }
  os << "]";

  return os << ", kind=" << rhs.kind()
            << ", object_name_prefix=" << rhs.object_name_prefix()
            << ", payload_format=" << rhs.payload_format()
            << ", self_link=" << rhs.self_link() << ", topic=" << rhs.topic()
            << "}";
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
