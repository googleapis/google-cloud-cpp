// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_CLEANUP_STALE_INSTANCES_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_CLEANUP_STALE_INSTANCES_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/project.h"
#include "google/cloud/status.h"

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Deletes any instances (and their backups) within the @p project that
 * are named with a YYYY-MM-DD component prior to yesterday (in UTC).
 */
Status CleanupStaleInstances(Project const& project);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_CLEANUP_STALE_INSTANCES_H
