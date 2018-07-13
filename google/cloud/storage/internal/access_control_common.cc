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

#include "google/cloud/storage/internal/access_control_common.h"
#include "google/cloud/storage/internal/nljson.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
AccessControlCommon AccessControlCommon::ParseFromJson(nl::json const& json) {
  AccessControlCommon result{};
  result.bucket_ = json.value("bucket", "");
  result.domain_ = json.value("domain", "");
  result.email_ = json.value("email", "");
  result.entity_ = json.value("entity", "");
  result.entity_id_ = json.value("entityId", "");
  result.etag_ = json.value("etag", "");
  result.id_ = json.value("id", "");
  result.kind_ = json.value("kind", "");
  result.role_ = json.value("role", "");
  result.self_link_ = json.value("selfLink", "");
  if (json.count("projectTeam") != 0U) {
    auto tmp = json["projectTeam"];
    result.project_team_.project_number = tmp.value("projectNumber", "");
    result.project_team_.team = tmp.value("team", "");
  }
  return result;
}

AccessControlCommon AccessControlCommon::ParseFromString(
    std::string const& payload) {
  auto json = nl::json::parse(payload);
  return AccessControlCommon::ParseFromJson(json);
}

bool AccessControlCommon::operator==(AccessControlCommon const& rhs) const {
  // Start with id, bucket, etag because they should fail early, then
  // alphabetical for readability.
  return id_ == rhs.id_ and bucket_ == rhs.bucket_ and etag_ == rhs.etag_ and
         domain_ == rhs.domain_ and email_ == rhs.email_ and
         entity_ == rhs.entity_ and entity_id_ == rhs.entity_id_ and
         kind_ == rhs.kind_ and
         project_team_.project_number == rhs.project_team_.project_number and
         project_team_.team == rhs.project_team_.team and role_ == rhs.role_ and
         self_link_ == rhs.self_link_;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
