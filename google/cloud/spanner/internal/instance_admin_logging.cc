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

#include "google/cloud/spanner/internal/instance_admin_logging.h"
#include "google/cloud/spanner/internal/log_wrapper.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace gcsa = google::spanner::admin::instance::v1;

StatusOr<gcsa::Instance> InstanceAdminLogging::GetInstance(
    grpc::ClientContext& context, gcsa::GetInstanceRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::GetInstanceRequest const& request) {
        return child_->GetInstance(context, request);
      },
      context, request, __func__);
}

StatusOr<gcsa::InstanceConfig> InstanceAdminLogging::GetInstanceConfig(
    grpc::ClientContext& context,
    gcsa::GetInstanceConfigRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::GetInstanceConfigRequest const& request) {
        return child_->GetInstanceConfig(context, request);
      },
      context, request, __func__);
}

StatusOr<gcsa::ListInstancesResponse> InstanceAdminLogging::ListInstances(
    grpc::ClientContext& context, gcsa::ListInstancesRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gcsa::ListInstancesRequest const& request) {
        return child_->ListInstances(context, request);
      },
      context, request, __func__);
}

StatusOr<google::iam::v1::Policy> InstanceAdminLogging::GetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::GetIamPolicyRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::iam::v1::GetIamPolicyRequest const& request) {
        return child_->GetIamPolicy(context, request);
      },
      context, request, __func__);
}

StatusOr<google::iam::v1::Policy> InstanceAdminLogging::SetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::SetIamPolicyRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::iam::v1::SetIamPolicyRequest const& request) {
        return child_->SetIamPolicy(context, request);
      },
      context, request, __func__);
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
InstanceAdminLogging::TestIamPermissions(
    grpc::ClientContext& context,
    google::iam::v1::TestIamPermissionsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::iam::v1::TestIamPermissionsRequest const& request) {
        return child_->TestIamPermissions(context, request);
      },
      context, request, __func__);
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
