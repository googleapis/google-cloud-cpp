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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXPERIMENTAL_SCHEMA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXPERIMENTAL_SCHEMA_H

#include "google/cloud/pubsub/version.h"
#include <grpcpp/grpcpp.h>
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
namespace pubsub_experimental {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Objects of this class identify a Cloud Pub/Sub schema.
 *
 * @note
 * This class makes no effort to validate the ids provided. The application
 * should verify that any ids passed to this application conform to the
 * Cloud Pub/Sub [resource name][name-link] restrictions.
 *
 * [name-link]: https://cloud.google.com/pubsub/docs/admin#resource_names
 */
class Schema {
 public:
  Schema(std::string project_id, std::string schema_id)
      : project_id_(std::move(project_id)), schema_id_(std::move(schema_id)) {}

  /// @name Copy and move
  //@{
  Schema(Schema const&) = default;
  Schema& operator=(Schema const&) = default;
  Schema(Schema&&) = default;
  Schema& operator=(Schema&&) = default;
  //@}

  /// Returns the Project ID
  std::string const& project_id() const { return project_id_; }

  /// Returns the Schema ID
  std::string const& schema_id() const { return schema_id_; }

  /**
   * Returns the fully qualified schema name as a string of the form:
   * "projects/<project-id>/schemas/<schema-id>"
   */
  std::string FullName() const;

  //@{
  /// @name Equality operators
  friend bool operator==(Schema const& a, Schema const& b);
  friend bool operator!=(Schema const& a, Schema const& b) { return !(a == b); }
  //@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream& os, Schema const& rhs);

 private:
  std::string project_id_;
  std::string schema_id_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_EXPERIMENTAL_SCHEMA_H
