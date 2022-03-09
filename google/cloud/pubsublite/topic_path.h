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

#include "google/cloud/pubsublite/cloud_region_or_zone.h"
#include "google/cloud/pubsublite/project_id_or_number.h"
#include "google/cloud/pubsublite/topic_name.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/version.h"
#include <utility>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class TopicPath {
 public:
  explicit TopicPath(ProjectIdOrNumber project, CloudRegionOrZone location,
                     TopicName name)
      : project_{std::move(project)},
        location_{std::move(location)},
        name_{std::move(name)} {}

  ProjectIdOrNumber const& GetProject() const { return project_; }

  CloudRegionOrZone const& GetLocation() const { return location_; }

  TopicName const& GetName() const { return name_; }

  std::string ToString() const {
    return absl::StrCat("projects/", project_.ToString(), "/locations/",
                        location_.ToString(), "/topics/", name_.GetName());
  }

 private:
  ProjectIdOrNumber project_;
  CloudRegionOrZone location_;
  TopicName name_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_TOPIC_PATH_H
