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

#include "google/cloud/spanner/testing/instance_location.h"
#include "google/cloud/spanner/admin/instance_admin_client.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace spanner_testing {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<std::string> InstanceLocation(spanner::Instance const& in) {
  spanner_admin::InstanceAdminClient client(
      spanner_admin::MakeInstanceAdminConnection());
  auto instance = client.GetInstance(in.FullName());
  if (!instance) return std::move(instance).status();
  auto instance_config = client.GetInstanceConfig(instance->config());
  if (!instance_config) return std::move(instance_config).status();
  for (auto const& replica : instance_config->replicas()) {
    if (replica.default_leader_location()) return replica.location();
  }
  return internal::UnavailableError(
      in.FullName() + ": No default_leader_location for replicas",
      GCP_ERROR_INFO());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google
