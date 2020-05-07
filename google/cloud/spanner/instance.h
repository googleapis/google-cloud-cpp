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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INSTANCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INSTANCE_H

#include "google/cloud/spanner/version.h"
#include <ostream>
#include <string>
#include <tuple>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * This class identifies a Cloud Spanner Instance
 *
 * A Cloud Spanner instance is identified by its `project_id` and `instance_id`.
 *
 * @note this class makes no effort to validate the components of the
 *     database name. It is the application's responsibility to provide valid
 *     project, and instance ids. Passing invalid values will not be checked
 *     until the instance name is used in a RPC to spanner.
 *
 * For more info about the `instance_id` format, see
 * https://cloud.google.com/spanner/docs/reference/rpc/google.spanner.admin.instance.v1#createinstancerequest
 */
class Instance {
 public:
  /// Constructs a Instance object identified by the given IDs.
  Instance(std::string project_id, std::string instance_id);

  /// @name Copy and move
  //@{
  Instance(Instance const&) = default;
  Instance& operator=(Instance const&) = default;
  Instance(Instance&&) = default;
  Instance& operator=(Instance&&) = default;
  //@}

  /// Returns the Project ID
  std::string const& project_id() const { return project_id_; }

  /// Returns the Instance ID
  std::string const& instance_id() const { return instance_id_; }

  /**
   * Returns the fully qualified instance name as a string of the form:
   * "projects/<project-id>/instances/<instance-id>"
   */
  std::string FullName() const;

  /// @name Equality operators
  //@{
  friend bool operator==(Instance const& a, Instance const& b);
  friend bool operator!=(Instance const& a, Instance const& b);
  //@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream& os, Instance const& dn);

 private:
  std::string project_id_;
  std::string instance_id_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INSTANCE_H
