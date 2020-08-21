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

#include "google/cloud/spanner/internal/instance_admin_metadata.h"
#include "google/cloud/internal/api_client_header.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace gcsa = ::google::spanner::admin::instance::v1;

InstanceAdminMetadata::InstanceAdminMetadata(
    std::shared_ptr<InstanceAdminStub> child)
    : child_(std::move(child)),
      api_client_header_(google::cloud::internal::ApiClientHeader()) {}

StatusOr<gcsa::Instance> InstanceAdminMetadata::GetInstance(
    grpc::ClientContext& context, gcsa::GetInstanceRequest const& request) {
  SetMetadata(context, "name=" + request.name());
  return child_->GetInstance(context, request);
}

StatusOr<google::longrunning::Operation> InstanceAdminMetadata::CreateInstance(
    grpc::ClientContext& context, gcsa::CreateInstanceRequest const& request) {
  SetMetadata(context, "parent=" + request.parent());
  return child_->CreateInstance(context, request);
}

StatusOr<google::longrunning::Operation> InstanceAdminMetadata::UpdateInstance(
    grpc::ClientContext& context, gcsa::UpdateInstanceRequest const& request) {
  SetMetadata(context, "instance.name=" + request.instance().name());
  return child_->UpdateInstance(context, request);
}

Status InstanceAdminMetadata::DeleteInstance(
    grpc::ClientContext& context, gcsa::DeleteInstanceRequest const& request) {
  SetMetadata(context, "name=" + request.name());
  return child_->DeleteInstance(context, request);
}

StatusOr<gcsa::InstanceConfig> InstanceAdminMetadata::GetInstanceConfig(
    grpc::ClientContext& context,
    gcsa::GetInstanceConfigRequest const& request) {
  SetMetadata(context, "name=" + request.name());
  return child_->GetInstanceConfig(context, request);
}

StatusOr<gcsa::ListInstanceConfigsResponse>
InstanceAdminMetadata::ListInstanceConfigs(
    grpc::ClientContext& context,
    gcsa::ListInstanceConfigsRequest const& request) {
  SetMetadata(context, "parent=" + request.parent());
  return child_->ListInstanceConfigs(context, request);
}

StatusOr<gcsa::ListInstancesResponse> InstanceAdminMetadata::ListInstances(
    grpc::ClientContext& context, gcsa::ListInstancesRequest const& request) {
  SetMetadata(context, "parent=" + request.parent());
  return child_->ListInstances(context, request);
}

StatusOr<google::iam::v1::Policy> InstanceAdminMetadata::GetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::GetIamPolicyRequest const& request) {
  SetMetadata(context, "resource=" + request.resource());
  return child_->GetIamPolicy(context, request);
}

StatusOr<google::iam::v1::Policy> InstanceAdminMetadata::SetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::SetIamPolicyRequest const& request) {
  SetMetadata(context, "resource=" + request.resource());
  return child_->SetIamPolicy(context, request);
}

StatusOr<google::iam::v1::TestIamPermissionsResponse>
InstanceAdminMetadata::TestIamPermissions(
    grpc::ClientContext& context,
    google::iam::v1::TestIamPermissionsRequest const& request) {
  SetMetadata(context, "resource=" + request.resource());
  return child_->TestIamPermissions(context, request);
}

StatusOr<google::longrunning::Operation> InstanceAdminMetadata::GetOperation(
    grpc::ClientContext& context,
    google::longrunning::GetOperationRequest const& request) {
  SetMetadata(context, "name=" + request.name());
  return child_->GetOperation(context, request);
}

void InstanceAdminMetadata::SetMetadata(grpc::ClientContext& context,
                                        std::string const& request_params) {
  context.AddMetadata("x-goog-request-params", request_params);
  context.AddMetadata("x-goog-api-client", api_client_header_);
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
