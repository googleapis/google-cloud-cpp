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

#include "google/cloud/storage/internal/hmac_key_metadata_parser.h"
#include "google/cloud/internal/parse_rfc3339.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
StatusOr<HmacKeyMetadata> HmacKeyMetadataParser::FromJson(
    nlohmann::json const& json) {
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  HmacKeyMetadata result{};
  result.access_id_ = json.value("accessId", "");
  result.etag_ = json.value("etag", "");
  result.id_ = json.value("id", "");
  result.kind_ = json.value("kind", "");
  result.project_id_ = json.value("projectId", "");
  result.service_account_email_ = json.value("serviceAccountEmail", "");
  result.state_ = json.value("state", "");
  if (json.count("timeCreated") != 0) {
    result.time_created_ =
        google::cloud::internal::ParseRfc3339(json.value("timeCreated", ""));
  }
  if (json.count("updated") != 0) {
    result.updated_ =
        google::cloud::internal::ParseRfc3339(json.value("updated", ""));
  }
  return result;
}

StatusOr<HmacKeyMetadata> HmacKeyMetadataParser::FromString(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  return FromJson(json);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
