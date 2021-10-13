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
#include "google/cloud/project.h"
#include "google/cloud/status_or.h"
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * This class identifies a Cloud Spanner Instance.
 *
 * A Cloud Spanner instance is identified by its `project_id` and `instance_id`.
 *
 * @note This class makes no effort to validate the components of the
 *     database name. It is the application's responsibility to provide valid
 *     project, and instance ids. Passing invalid values will not be checked
 *     until the instance name is used in a RPC to spanner.
 *
 * For more info about the `instance_id` format, see
 * https://cloud.google.com/spanner/docs/reference/rpc/google.spanner.admin.instance.v1#createinstancerequest
 */
class Instance {
 public:
  /**
   * Constructs an Instance object identified by the given @p project and
   * @p instance_id.
   */
  Instance(Project project, std::string instance_id);

  /**
   * Constructs an Instance object identified by the given IDs.
   *
   * This is equivalent to first constructing a `Project` from the given
   * @p project_id and then calling the `Instance(Project, std::string)`
   * constructor.
   */
  Instance(std::string project_id, std::string instance_id);

  /// @name Copy and move
  //@{
  Instance(Instance const&) = default;
  Instance& operator=(Instance const&) = default;
  Instance(Instance&&) = default;
  Instance& operator=(Instance&&) = default;
  //@}

  /// Returns the `Project` containing this instance.
  Project const& project() const { return project_; }
  std::string const& project_id() const { return project_.project_id(); }

  /// Returns the Instance ID.
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
  friend std::ostream& operator<<(std::ostream&, Instance const&);

 private:
  Project project_;
  std::string instance_id_;
};

/**
 * Constructs an `Instance` from the given @p full_name.
 * Returns a non-OK Status if `full_name` is improperly formed.
 */
StatusOr<Instance> MakeInstance(std::string const& full_name);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INSTANCE_H
