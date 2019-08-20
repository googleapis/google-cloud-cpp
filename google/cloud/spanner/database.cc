// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/database.h"
#include <array>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

Database::Database(std::string const& project_id,
                   std::string const& instance_id,
                   std::string const& database_id)
    : full_name_("projects/" + project_id + "/instances/" + instance_id +
                 "/databases/" + database_id) {}

std::string Database::FullName() const { return full_name_; }

std::string Database::DatabaseId() const {
  auto pos = full_name_.rfind("/databases/");
  return full_name_.substr(pos + sizeof("/databases/") - 1);
}

std::string Database::ParentName() const {
  auto pos = full_name_.rfind("/databases/");
  return full_name_.substr(0, pos);
}

bool operator==(Database const& a, Database const& b) {
  return a.full_name_ == b.full_name_;
}

bool operator!=(Database const& a, Database const& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, Database const& dn) {
  return os << dn.full_name_;
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
