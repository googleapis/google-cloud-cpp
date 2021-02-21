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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_PICK_RANDOM_INSTANCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_PICK_RANDOM_INSTANCE_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/status_or.h"
#include <google/spanner/admin/instance/v1/spanner_instance_admin.pb.h>
#include <functional>
#include <string>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

using InstancePredicate = std::function<bool(
    google::spanner::admin::instance::v1::Instance const&,
    google::spanner::admin::instance::v1::InstanceConfig const&)>;

/**
 * Select one of the instances in @p project_id to run tests on.
 *
 * Only returns instances that match the @p filter and satisfy the
 * @p predicate.
 */
StatusOr<std::string> PickRandomInstance(
    google::cloud::internal::DefaultPRNG& generator,
    std::string const& project_id, std::string const& filter = "",
    InstancePredicate predicate = nullptr);

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_PICK_RANDOM_INSTANCE_H
