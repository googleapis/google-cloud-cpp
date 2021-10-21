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

Database::Database(Instance instance, std::string database_id)
    : instance_(std::move(instance)), database_id_(std::move(database_id)) {}

Database::Database(std::string project_id, std::string instance_id,
                   std::string database_id)
    : Database(Instance(std::move(project_id), std::move(instance_id)),
               std::move(database_id)) {}

std::string Database::FullName() const {
  return instance().FullName() + "/databases/" + database_id_;
}

bool operator==(Database const& a, Database const& b) {
  return a.instance_ == b.instance_ && a.database_id_ == b.database_id_;
}

bool operator!=(Database const& a, Database const& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, Database const& dn) {
  return os << dn.FullName();
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
