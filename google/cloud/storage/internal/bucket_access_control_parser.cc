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

#include "google/cloud/storage/internal/bucket_access_control_parser.h"
#include "google/cloud/storage/internal/access_control_common_parser.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
StatusOr<BucketAccessControl> BucketAccessControlParser::FromJson(
    nlohmann::json const& json) {
  if (!json.is_object()) return Status(StatusCode::kInvalidArgument, __func__);

  BucketAccessControl result;
  result.set_bucket(json.value("bucket", ""));
  result.set_domain(json.value("domain", ""));
  result.set_email(json.value("email", ""));
  result.set_entity(json.value("entity", ""));
  result.set_entity_id(json.value("entityId", ""));
  result.set_etag(json.value("etag", ""));
  result.set_id(json.value("id", ""));
  result.set_kind(json.value("kind", ""));
  result.set_role(json.value("role", ""));
  result.set_self_link(json.value("selfLink", ""));
  auto team = json.find("projectTeam");
  if (team != json.end()) {
    auto const& tmp = *team;
    if (tmp.is_null()) return result;
    ProjectTeam p;
    p.project_number = tmp.value("projectNumber", "");
    p.team = tmp.value("team", "");
    result.set_project_team(std::move(p));
  }
  return result;
}

StatusOr<BucketAccessControl> BucketAccessControlParser::FromString(
    std::string const& payload) {
  auto json = nlohmann::json::parse(payload, nullptr, false);
  return FromJson(json);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
