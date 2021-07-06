// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMMON_METADATA_PARSER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMMON_METADATA_PARSER_H

#include "google/cloud/storage/internal/common_metadata.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/status.h"
#include <nlohmann/json.hpp>
#include <string>
namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
template <typename Derived>
struct CommonMetadataParser {
  static Status FromJson(CommonMetadata<Derived>& result,
                         nlohmann::json const& json) {
    if (!json.is_object()) {
      return Status(StatusCode::kInvalidArgument, __func__);
    }
    result.etag_ = json.value("etag", "");
    result.id_ = json.value("id", "");
    result.kind_ = json.value("kind", "");
    auto metageneration = ParseLongField(json, "metageneration");
    if (!metageneration) return std::move(metageneration).status();
    result.metageneration_ = *metageneration;
    result.name_ = json.value("name", "");
    if (json.count("owner") != 0) {
      Owner o;
      o.entity = json["owner"].value("entity", "");
      o.entity_id = json["owner"].value("entityId", "");
      result.owner_ = std::move(o);
    }
    result.self_link_ = json.value("selfLink", "");
    result.storage_class_ = json.value("storageClass", "");
    auto time_created = ParseTimestampField(json, "timeCreated");
    if (!time_created) return std::move(time_created).status();
    result.time_created_ = *time_created;
    auto updated = ParseTimestampField(json, "updated");
    if (!updated) return std::move(updated).status();
    result.updated_ = *updated;
    return Status();
  }
  static StatusOr<CommonMetadata<Derived>> FromString(
      std::string const& payload) {
    auto json = nlohmann::json::parse(payload);
    return FromJson(json);
  }
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_COMMON_METADATA_PARSER_H
