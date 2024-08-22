// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/notification_metadata_parser.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/internal/make_status.h"
#include <iostream>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
StatusOr<NotificationMetadata> NotificationMetadataParser::FromJson(
    nlohmann::json const& json) {
  if (!json.is_object()) return NotJsonObject(json, GCP_ERROR_INFO());
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
  auto json = nlohmann::json::parse(payload, nullptr, false);
  return FromJson(json);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
