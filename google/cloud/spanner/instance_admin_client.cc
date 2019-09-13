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

#include "google/cloud/spanner/instance_admin_client.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
StatusOr<google::spanner::admin::instance::v1::Instance>
InstanceAdminClient::GetInstance(Instance const& in) {
  return conn_->GetInstance({in.FullName()});
}

ListInstancesRange InstanceAdminClient::ListInstances(std::string project_id,
                                                      std::string filter) {
  return conn_->ListInstances({std::move(project_id), std::move(filter)});
}

StatusOr<google::iam::v1::Policy> InstanceAdminClient::GetIamPolicy(
    Instance const& in) {
  return conn_->GetIamPolicy({in.FullName()});
}

StatusOr<google::iam::v1::Policy> InstanceAdminClient::SetIamPolicy(
    Instance const& in, google::iam::v1::Policy policy) {
  return conn_->SetIamPolicy({in.FullName(), std::move(policy)});
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
InstanceAdminClient::TestIamPermissions(Instance const& in,
                                        std::vector<std::string> permissions) {
  return conn_->TestIamPermissions({in.FullName(), std::move(permissions)});
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
