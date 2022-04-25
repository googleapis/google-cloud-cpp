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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_TOPIC_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_TOPIC_H

#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/version.h"
#include <utility>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Objects of this class identify a Cloud Pub/Sub Lite topic.
 *
 * @note
 * This class makes no effort to validate the ids provided.
 */
class Topic {
 public:
  Topic(std::string project_id, std::string location, std::string topic_id)
      : project_id_(std::move(project_id)),
        location_{std::move(location)},
        topic_id_(std::move(topic_id)) {}

  std::string const& project_id() const { return project_id_; }

  std::string const& location() const { return location_; }

  std::string const& topic_id() const { return topic_id_; }

  /**
   * Returns the fully qualified topic name as a string of the form:
   * "projects/<project-id>/locations/<location>/topics/<topic-id>"
   */
  std::string FullName() const {
    return absl::StrCat("projects/", project_id_, "/locations/", location_,
                        "/topics/", topic_id_);
  }

 private:
  std::string project_id_;
  std::string location_;
  std::string topic_id_;
};

inline bool operator==(Topic const& a, Topic const& b) {
  return a.project_id() == b.project_id() && a.location() == b.location() &&
         a.topic_id() == b.topic_id();
}

inline bool operator!=(Topic const& a, Topic const& b) { return !(a == b); }

inline std::ostream& operator<<(std::ostream& os, Topic const& rhs) {
  return os << rhs.FullName();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_TOPIC_H
