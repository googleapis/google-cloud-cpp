// Copyright 2020 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_PICK_INSTANCE_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_PICK_INSTANCE_CONFIG_H

#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/random.h"
#include <regex>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

/// Pick one instance_config from a list given a PRNG generator, project_id,
/// and a regex for filtering, if none matches the filter, it will return the
/// value for the first element.
std::string PickInstanceConfig(std::string const& project_id,
                               std::regex const& filter_regex,
                               google::cloud::internal::DefaultPRNG& generator);

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_PICK_INSTANCE_CONFIG_H
