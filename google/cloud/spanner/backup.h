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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BACKUP_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BACKUP_H

#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * This class identifies a Cloud Spanner Backup.
 *
 * A Cloud Spanner backup is identified by an `Instance` and a `backup_id`.
 *
 * @note This class makes no effort to validate the components of the
 *     backup name. It is the application's responsibility to provide valid
 *     project, instance, and backup ids. Passing invalid values will not be
 *     checked until the backup name is used in a RPC to spanner.
 */
class Backup {
 public:
  /*
   * Constructs a Backup object identified by the given @p instance and
   * @p backup_id.
   */
  Backup(Instance instance, std::string backup_id);

  /// @name Copy and move
  ///@{
  Backup(Backup const&) = default;
  Backup& operator=(Backup const&) = default;
  Backup(Backup&&) = default;
  Backup& operator=(Backup&&) = default;
  ///@}

  /// Returns the `Instance` containing this backup.
  Instance const& instance() const { return instance_; }

  /// Returns the Backup ID.
  std::string const& backup_id() const { return backup_id_; }

  /**
   * Returns the fully qualified backup name as a string of the form:
   * "projects/<project-id>/instances/<instance-id>/backups/<backup-id>"
   */
  std::string FullName() const;

  /// @name Equality operators
  ///@{
  friend bool operator==(Backup const& a, Backup const& b);
  friend bool operator!=(Backup const& a, Backup const& b);
  ///@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream&, Backup const&);

 private:
  Instance instance_;
  std::string backup_id_;
};

/**
 * Constructs a `Backup` from the given @p full_name.
 * Returns a non-OK Status if `full_name` is improperly formed.
 */
StatusOr<Backup> MakeBackup(std::string const& full_name);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_BACKUP_H
