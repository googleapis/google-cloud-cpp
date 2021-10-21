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

#include "google/cloud/spanner/backup.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

Backup::Backup(Instance instance, std::string backup_id)
    : instance_(std::move(instance)), backup_id_(std::move(backup_id)) {}

std::string Backup::FullName() const {
  return instance().FullName() + "/backups/" + backup_id_;
}

bool operator==(Backup const& a, Backup const& b) {
  return a.instance_ == b.instance_ && a.backup_id_ == b.backup_id_;
}

bool operator!=(Backup const& a, Backup const& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, Backup const& bn) {
  return os << bn.FullName();
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
