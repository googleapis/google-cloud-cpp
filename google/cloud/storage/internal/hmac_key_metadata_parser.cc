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

#include "google/cloud/storage/internal/hmac_key_metadata_parser.h"
#include "google/cloud/storage/internal/metadata_parser.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
StatusOr<HmacKeyMetadata> HmacKeyMetadataParser::FromJson(
    nlohmann::json const& json) {
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  HmacKeyMetadata result{};
  result.set_access_id(json.value("accessId", ""));
  result.set_etag(json.value("etag", ""));
  result.set_id(json.value("id", ""));
  result.set_kind(json.value("kind", ""));
  result.set_project_id(json.value("projectId", ""));
  result.set_service_account_email(json.value("serviceAccountEmail", ""));
  result.set_state(json.value("state", ""));
  auto time_created = ParseTimestampField(json, "timeCreated");
  if (!time_created) return std::move(time_created).status();
  result.set_time_created(*time_created);
  auto updated = ParseTimestampField(json, "updated");
  if (!updated) return std::move(updated).status();
  result.set_updated(*updated);
  return result;
}

StatusOr<HmacKeyMetadata> HmacKeyMetadataParser::FromString(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  return FromJson(json);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
