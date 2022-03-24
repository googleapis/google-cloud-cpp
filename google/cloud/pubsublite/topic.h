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

#include "google/cloud/pubsublite/internal/location.h"
#include "google/cloud/pubsublite/project_id_or_number.h"
#include "google/cloud/pubsublite/topic_name.h"
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
  Topic(std::string project, std::string location, std::string topic_name)
      : project_(std::move(project)),
        location_{std::move(location)},
        topic_name_(std::move(topic_name)) {}

  Topic(Topic const&) = default;
  Topic& operator=(Topic const&) = default;
  Topic(Topic&&) = default;
  Topic& operator=(Topic&&) = default;

  std::string const& project() const { return project_; }

  std::string const& location() const { return location_; }

  std::string const& topic_name() const { return topic_name_; }

  /**
   * Returns the fully qualified topic name as a string of the form:
   * "projects/<project-id>/locations/<location>/topics/<topic-id>"
   */
  std::string FullName() const {
    return absl::StrCat("projects/", project_, "/locations/", location_,
                        "/topics/", topic_name_);
  }

  friend bool operator==(Topic const& a, Topic const& b);
  friend bool operator!=(Topic const& a, Topic const& b) { return !(a == b); }

  friend std::ostream& operator<<(std::ostream& os, Topic const& rhs);

 private:
  std::string project_;
  std::string location_;
  std::string topic_name_;
};

bool operator==(Topic const& a, Topic const& b) {
  return a.project_id() == b.project_id() && a.location() == b.location() &&
         a.topic_id() == b.topic_id();
}

std::ostream& operator<<(std::ostream& os, Topic const& rhs) {
  return os << rhs.FullName();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_TOPIC_H
