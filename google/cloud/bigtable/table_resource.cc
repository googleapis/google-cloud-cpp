// Copyright 2022 Google LLC
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

#include "google/cloud/bigtable/table_resource.h"
#include <ostream>
#include <regex>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TableResource::TableResource(InstanceResource instance, std::string table_id)
    : instance_(std::move(instance)), table_id_(std::move(table_id)) {}

TableResource::TableResource(std::string project_id, std::string instance_id,
                             std::string table_id)
    : instance_(Project(std::move(project_id)), std::move(instance_id)),
      table_id_(std::move(table_id)) {}

std::string TableResource::FullName() const {
  return instance_.FullName() + "/tables/" + table_id_;
}

bool operator==(TableResource const& a, TableResource const& b) {
  return a.instance_ == b.instance_ && a.table_id_ == b.table_id_;
}

bool operator!=(TableResource const& a, TableResource const& b) {
  return !(a == b);
}

std::ostream& operator<<(std::ostream& os, TableResource const& db) {
  return os << db.FullName();
}

StatusOr<TableResource> MakeTableResource(std::string const& full_name) {
  std::regex re("projects/([^/]+)/instances/([^/]+)/tables/([^/]+)");
  std::smatch matches;
  if (!std::regex_match(full_name, matches, re)) {
    return Status(StatusCode::kInvalidArgument,
                  "Improperly formatted TableResource: " + full_name);
  }
  return TableResource(
      InstanceResource(Project(std::move(matches[1])), std::move(matches[2])),
      std::move(matches[3]));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
