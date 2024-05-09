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

#include "google/cloud/spanner/backup.h"
#include "google/cloud/internal/make_status.h"
#include <ostream>
#include <regex>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Backup::Backup(Instance instance, std::string backup_id)
    : instance_(std::move(instance)), backup_id_(std::move(backup_id)) {}

std::string Backup::FullName() const {
  return instance().FullName() + "/backups/" + backup_id_;
}

bool operator==(Backup const& a, Backup const& b) {
  return a.instance_ == b.instance_ && a.backup_id_ == b.backup_id_;
}

bool operator!=(Backup const& a, Backup const& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, Backup const& bu) {
  return os << bu.FullName();
}

StatusOr<Backup> MakeBackup(std::string const& full_name) {
  std::regex re("projects/([^/]+)/instances/([^/]+)/backups/([^/]+)");
  std::smatch matches;
  if (!std::regex_match(full_name, matches, re)) {
    return internal::InvalidArgumentError(
        "Improperly formatted Backup: " + full_name, GCP_ERROR_INFO());
  }
  return Backup(Instance(std::move(matches[1]), std::move(matches[2])),
                std::move(matches[3]));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
