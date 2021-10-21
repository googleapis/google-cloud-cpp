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

#include "google/cloud/storage/object_access_control.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
bool operator==(ObjectAccessControl const& lhs,
                ObjectAccessControl const& rhs) {
  // Start with id, generation, object, bucket, etag because they should fail
  // early, then alphabetical for readability.
  return lhs.object_ == rhs.object_ && lhs.generation_ == rhs.generation_ &&
         *static_cast<internal::AccessControlCommon const*>(&lhs) == rhs;
}

std::ostream& operator<<(std::ostream& os, ObjectAccessControl const& rhs) {
  os << "ObjectAccessControl={bucket=" << rhs.bucket()
     << ", domain=" << rhs.domain() << ", email=" << rhs.email()
     << ", entity=" << rhs.entity() << ", entity_id=" << rhs.entity_id()
     << ", etag=" << rhs.etag() << ", generation=" << rhs.generation()
     << ", id=" << rhs.id() << ", kind=" << rhs.kind()
     << ", object=" << rhs.object();

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
