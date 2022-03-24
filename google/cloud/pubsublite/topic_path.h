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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_TOPIC_PATH_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_TOPIC_PATH_H

#include "google/cloud/pubsublite/location.h"
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
 * A string wrapper representing a topic.
 */
class TopicPath {
 public:
  explicit TopicPath(ProjectIdOrNumber const& project, Location const& location,
                     TopicName const& name)
      : project_{project.ToString()},
        location_{location.ToString()},
        name_{name.GetName()} {}

  std::string ToString() const {
    return absl::StrCat("projects/", project_, "/locations/", location_,
                        "/topics/", name_);
  }

 private:
  std::string const project_;
  std::string const location_;
  std::string const name_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_TOPIC_PATH_H
