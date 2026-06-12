// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_UPDATER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_UPDATER_H

#include "google/cloud/version.h"
#include "absl/types/optional.h"
#include "google/iam/v1/policy.pb.h"
#include <functional>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Used in the `SetIamPolicy()` read-modify-write cycles where an `etag` helps
 * prevent simultaneous updates of a policy from overwriting each other.
 *
 * The updater is called with a recently fetched policy, and should either
 * return an empty optional if no changes are required, or return a new policy
 * to be set. In the latter case the updater should either (1) set the `etag`
 * of the returned policy to that of the recently fetched policy (strongly
 * suggested), or (2) not set the `etag` at all.
 *
 * In case (1) if the `etag` does not match the existing policy (i.e., there
 * has been an intermediate update), this update is dropped and a new cycle is
 * initiated. In case (2) the existing policy is overwritten blindly.
 */
using IamUpdater = std::function<absl::optional<google::iam::v1::Policy>(
    google::iam::v1::Policy)>;

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_UPDATER_H
