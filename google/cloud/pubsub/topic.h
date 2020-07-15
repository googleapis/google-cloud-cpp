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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_H

#include "google/cloud/pubsub/version.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/**
 * Objects of this class identify a Cloud Pub/Sub topic.
 *
 * @note
 * This class makes no effort to validate the ids provided. The application
 * should verify that any ids passed to this application conform to the
 * Cloud Pub/Sub [resource name][name-link] restrictions.
 *
 * [name-link]: https://cloud.google.com/pubsub/docs/admin#resource_names
 */
class Topic {
 public:
  Topic(std::string project_id, std::string topic_id)
      : project_id_(std::move(project_id)), topic_id_(std::move(topic_id)) {}

  /// @name Copy and move
  //@{
  Topic(Topic const&) = default;
  Topic& operator=(Topic const&) = default;
  Topic(Topic&&) = default;
  Topic& operator=(Topic&&) = default;
  //@}

  /// Returns the Project ID
  std::string const& project_id() const { return project_id_; }

  /// Returns the Topic ID
  std::string const& topic_id() const { return topic_id_; }

  /**
   * Returns the fully qualified topic name as a string of the form:
   * "projects/<project-id>/topics/<topic-id>"
   */
  std::string FullName() const;

  /// @name Equality operators
  //@{
  friend bool operator==(Topic const& a, Topic const& b);
  friend bool operator!=(Topic const& a, Topic const& b) { return !(a == b); }
  //@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream& os, Topic const& rhs);

 private:
  std::string project_id_;
  std::string topic_id_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_TOPIC_H
