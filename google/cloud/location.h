// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_LOCATION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_LOCATION_H

#include "google/cloud/project.h"
#include "google/cloud/status_or.h"
#include "google/cloud/version.h"
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A representation of a Cloud Location.
 *
 * A Cloud location is identified by its `project_id` and `location_id`.
 *
 * @note This class makes no effort to validate the components of the
 *     location name. It is the application's responsibility to provide
 *     valid project and location ids. Passing invalid values will not be
 *     checked until the location name is used in an RPC.
 *
 * For more info about locations, see https://cloud.google.com/about/locations
 */
class Location {
 public:
  /**
   * Constructs a Location object identified by the given @p project and
   * @p location_id.
   */
  Location(Project project, std::string location_id);

  /**
   * Constructs a Location_id object identified by the given IDs.
   *
   * This is equivalent to first constructing a `Project` from the given
   * @p project_id and then calling the `Location(Project, std::string)`
   * constructor.
   */
  Location(std::string project_id, std::string location_id);

  /// @name Copy and move
  ///@{
  Location(Location const&) = default;
  Location& operator=(Location const&) = default;
  Location(Location&&) = default;
  Location& operator=(Location&&) = default;
  ///@}

  /// Returns the `Project` containing this location.
  Project const& project() const { return project_; }
  std::string const& project_id() const { return project_.project_id(); }

  /// Returns the Location ID.
  std::string const& location_id() const { return location_id_; }

  /**
   * Returns the fully qualified location name as a string of the form:
   * "projects/<project-id>/locations/<location-id>"
   */
  std::string FullName() const;

  /// @name Equality operators
  ///@{
  friend bool operator==(Location const& a, Location const& b);
  friend bool operator!=(Location const& a, Location const& b);
  ///@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream&, Location const&);

 private:
  Project project_;
  std::string location_id_;
};

/**
 * Constructs a `Location` from the given @p full_name.
 * Returns a non-OK Status if `full_name` is improperly formed.
 */
StatusOr<Location> MakeLocation(std::string const& full_name);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_LOCATION_H
