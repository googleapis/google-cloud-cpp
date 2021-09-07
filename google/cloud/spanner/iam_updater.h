// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_IAM_UPDATER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_IAM_UPDATER_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/iam_updater.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Type alias for google::cloud::IamUpdater.
 *
 * Used in the `SetIamPolicy()` read-modify-write cycle of the Spanner admin
 * clients, `DatabaseAdminClient` and `InstanceAdminClient`, in order to avoid
 * race conditions.
 *
 * The updater is called with a recently fetched policy, and should either
 * return an empty optional if no changes are required, or return a new policy
 * to be set. In the latter case the control loop will always set the `etag`
 * of the new policy to that of the recently fetched one. A failure to update
 * then indicates a race, and the process will repeat.
 */
using IamUpdater = ::google::cloud::IamUpdater;

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_IAM_UPDATER_H
