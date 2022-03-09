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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_PROJECT_ID_OR_NUMBER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_PROJECT_ID_OR_NUMBER_H

#include "google/cloud/pubsublite/project_id.h"
#include "google/cloud/pubsublite/project_number.h"
#include "google/cloud/version.h"
#include "absl/types/variant.h"

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class ProjectIdOrNumber {
 public:
  explicit ProjectIdOrNumber(ProjectId project_id)
      : value_{std::move(project_id)} {}

  explicit ProjectIdOrNumber(ProjectNumber project_number)
      : value_{std::move(project_number)} {}

  bool HasProjectId() const {
    return absl::holds_alternative<ProjectId>(value_);
  }

  bool HasProjectNumber() const {
    return absl::holds_alternative<ProjectNumber>(value_);
  }

  ProjectId const& GetProjectId() const { return absl::get<ProjectId>(value_); }

  ProjectNumber const& GetProjectNumber() const {
    return absl::get<ProjectNumber>(value_);
  }

  std::string ToString() const {
    if (HasProjectId()) {
      return absl::get<ProjectId>(value_).GetId();
    }
    return std::to_string(absl::get<ProjectNumber>(value_).GetNumber());
  }

 private:
  absl::variant<ProjectId, ProjectNumber> const value_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_PROJECT_ID_OR_NUMBER_H
