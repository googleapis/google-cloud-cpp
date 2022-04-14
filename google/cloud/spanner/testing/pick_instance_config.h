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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_PICK_INSTANCE_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_PICK_INSTANCE_CONFIG_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/project.h"
#include <google/spanner/admin/instance/v1/spanner_instance_admin.pb.h>
#include <functional>
#include <string>

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Returns the name of one instance config that satisfies the given
 * predicate from amongst all those that exist within the given project.
 *
 * If multiple instance configs qualify, the one returned is chosen
 * at random using the PRNG. If none qualify, the first candidate is
 * returned. If there are no candidates, the empty string is returned.
 */
std::string PickInstanceConfig(
    Project const& project, google::cloud::internal::DefaultPRNG& generator,
    std::function<
        bool(google::spanner::admin::instance::v1::InstanceConfig const&)>
        predicate);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_PICK_INSTANCE_CONFIG_H
