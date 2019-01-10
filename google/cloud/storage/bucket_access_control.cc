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

#include "google/cloud/storage/bucket_access_control.h"
#include "google/cloud/storage/internal/nljson.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
StatusOr<BucketAccessControl> BucketAccessControl::ParseFromJson(
    internal::nl::json const& json) {
  if (not json.is_object()) {
    return Status(StatusCode::kInvalidArgument, __func__);
  }
  BucketAccessControl result{};
  auto status = AccessControlCommon::ParseFromJson(result, json);
  if (not status.ok()) {
    return status;
  }
  return result;
}

StatusOr<BucketAccessControl> BucketAccessControl::ParseFromString(
    std::string const& payload) {
  auto json = internal::nl::json::parse(payload, nullptr, false);
  return BucketAccessControl::ParseFromJson(json);
}

bool BucketAccessControl::operator==(BucketAccessControl const& rhs) const {
  return *static_cast<internal::AccessControlCommon const*>(this) == rhs;
}

std::ostream& operator<<(std::ostream& os, BucketAccessControl const& rhs) {
  os << "BucketAccessControl={bucket=" << rhs.bucket()
     << ", domain=" << rhs.domain() << ", email=" << rhs.email()
     << ", entity=" << rhs.entity() << ", entity_id=" << rhs.entity_id()
     << ", etag=" << rhs.etag() << ", id=" << rhs.id()
     << ", kind=" << rhs.kind();

  if (rhs.has_project_team()) {
    os << ", project_team.project_number=" << rhs.project_team().project_number
       << ", project_team.team=" << rhs.project_team().team;
  }

  return os << ", role=" << rhs.role() << ", self_link=" << rhs.self_link()
            << "}";
}
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
