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
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
Status AccessControlCommon::ParseFromJson(AccessControlCommon& result,
                                          nlohmann::json const& json) {
  if (!json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
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
  if (json.count("projectTeam") != 0) {
    auto tmp = json["projectTeam"];
    ProjectTeam p;
    p.project_number = tmp.value("projectNumber", "");
    p.team = tmp.value("team", "");
    result.project_team_ = std::move(p);
  }
  return Status();
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
