// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PROJECT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PROJECT_H

#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * This class identifies a Cloud Project.
 *
 * A Cloud project is identified by its `project_id`.
 *
 * @note This class makes no effort to validate the components of the
 *     project name. It is the application's responsibility to provide
 *     a valid project id. Passing invalid values will not be checked
 *     until the project name is used in an RPC.
 *
 * For more info about the `project_id` format, see
 * https://cloud.google.com/resource-manager/docs/creating-managing-projects
 */
class Project {
 public:
  /// Constructs a Project object identified by the given @p project_id.
  explicit Project(std::string project_id);

  /// @name Copy and move
  //@{
  Project(Project const&) = default;
  Project& operator=(Project const&) = default;
  Project(Project&&) = default;
  Project& operator=(Project&&) = default;
  //@}

  /// Returns the Project ID
  std::string const& project_id() const { return project_id_; }

  /**
   * Returns the fully qualified project name as a string of the form:
   * "projects/<project-id>"
   */
  std::string FullName() const;

  /// @name Equality operators
  //@{
  friend bool operator==(Project const& a, Project const& b);
  friend bool operator!=(Project const& a, Project const& b);
  //@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream&, Project const&);

 private:
  std::string project_id_;
};

/**
 * Constructs a `Project` from the given @p full_name.
 * Returns a non-OK Status if `full_name` is improperly formed.
 */
StatusOr<Project> MakeProject(std::string const& full_name);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PROJECT_H
