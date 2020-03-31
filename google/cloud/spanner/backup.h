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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BACKUP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BACKUP_H

#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/version.h"
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * This class identifies a Cloud Spanner Backup
 *
 * A Cloud Spanner backup is identified by an `Instance` and a `backup_id`.
 *
 * @note this class makes no effort to validate the components of the
 *     backup name. It is the application's responsibility to provide valid
 *     project, instance, and backup ids. Passing invalid values will not be
 *     checked until the backup name is used in a RPC to spanner.
 */
class Backup {
 public:
  /// Constructs a Backup object identified by the given @p backup_id and
  /// @p instance.
  Backup(Instance instance, std::string backup_id);

  /// @name Copy and move
  //@{
  Backup(Backup const&) = default;
  Backup& operator=(Backup const&) = default;
  Backup(Backup&&) = default;
  Backup& operator=(Backup&&) = default;
  //@}

  /**
   * Returns the fully qualified backup name as a string of the form:
   * "projects/<project-id>/instances/<instance-id>/backups/<backup-id>"
   */
  std::string FullName() const;

  /// Returns the `Instance` containing this backup.
  Instance const& instance() const { return instance_; }
  std::string const& backup_id() const { return backup_id_; }

  /// @name Equality operators
  //@{
  friend bool operator==(Backup const& a, Backup const& b);
  friend bool operator!=(Backup const& a, Backup const& b);
  //@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream& os, Backup const& bn);

 private:
  Instance instance_;
  std::string backup_id_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BACKUP_H
