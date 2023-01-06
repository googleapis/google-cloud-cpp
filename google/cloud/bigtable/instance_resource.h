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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_RESOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_RESOURCE_H

#include "google/cloud/bigtable/version.h"
#include "google/cloud/project.h"
#include "google/cloud/status_or.h"
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * This class identifies a Cloud Bigtable Instance.
 *
 * To use Cloud Bigtable, you create instances, which contain clusters that your
 * applications can connect to. Each cluster contains nodes, the compute units
 * that manage your data and perform maintenance tasks. A Cloud Bigtable
 * instance is identified by its `project_id` and `instance_id`.
 *
 * @note This class makes no effort to validate the components of the
 *     instance name. It is the application's responsibility to provide valid
 *     project, and instance ids. Passing invalid values will not be checked
 *     until the instance name is used in an RPC to Bigtable.
 *
 * @see https://cloud.google.com/bigtable/docs/instances-clusters-nodes for an
 *     overview of Cloud Bigtable instances, clusters, and nodes.
 */
class InstanceResource {
 public:
  /**
   * Constructs an InstanceResource object identified by the given @p project
   * and @p instance_id.
   */
  InstanceResource(Project project, std::string instance_id);

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
  friend bool operator==(InstanceResource const& a, InstanceResource const& b);
  friend bool operator!=(InstanceResource const& a, InstanceResource const& b);
  //@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream&, InstanceResource const&);

 private:
  Project project_;
  std::string instance_id_;
};

/**
 * Constructs an `InstanceResource` from the given @p full_name.
 * Returns a non-OK Status if `full_name` is improperly formed.
 */
StatusOr<InstanceResource> MakeInstanceResource(std::string const& full_name);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_RESOURCE_H
