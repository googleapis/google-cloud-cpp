// Copyright 2018 Google LLC
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

#include "google/cloud/bigtable/instance_update_config.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// The names for these constants are taken from the proto, and follow the
// conventions from proto files.

constexpr InstanceUpdateConfig::InstanceType
    // NOLINTNEXTLINE(readability-identifier-naming)
    InstanceUpdateConfig::TYPE_UNSPECIFIED;

// NOLINTNEXTLINE(readability-identifier-naming)
constexpr InstanceUpdateConfig::InstanceType InstanceUpdateConfig::PRODUCTION;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr InstanceUpdateConfig::InstanceType InstanceUpdateConfig::DEVELOPMENT;

// NOLINTNEXTLINE(readability-identifier-naming)
constexpr InstanceUpdateConfig::StateType InstanceUpdateConfig::STATE_NOT_KNOWN;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr InstanceUpdateConfig::StateType InstanceUpdateConfig::READY;
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr InstanceUpdateConfig::StateType InstanceUpdateConfig::CREATING;

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
