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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_METADATA_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_METADATA_H

#include "google/cloud/spanner/internal/instance_admin_stub.h"
#include "google/cloud/spanner/version.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/**
 * Implements the metadata Decorator for InstanceAdminStub.
 */
class InstanceAdminMetadata : public InstanceAdminStub {
 public:
  explicit InstanceAdminMetadata(std::shared_ptr<InstanceAdminStub> child);

  ~InstanceAdminMetadata() override = default;

  //@{
  /**
   * @name Override the functions from `InstanceAdminStub`.
   */
  ///
  StatusOr<google::spanner::admin::instance::v1::Instance> GetInstance(
      grpc::ClientContext&,
      google::spanner::admin::instance::v1::GetInstanceRequest const&) override;

  StatusOr<google::longrunning::Operation> CreateInstance(
      grpc::ClientContext&,
      google::spanner::admin::instance::v1::CreateInstanceRequest const&)
      override;

  StatusOr<google::longrunning::Operation> UpdateInstance(
      grpc::ClientContext&,
      google::spanner::admin::instance::v1::UpdateInstanceRequest const&)
      override;

  Status DeleteInstance(
      grpc::ClientContext&,
      google::spanner::admin::instance::v1::DeleteInstanceRequest const&)
      override;

  StatusOr<google::spanner::admin::instance::v1::InstanceConfig>
  GetInstanceConfig(
      grpc::ClientContext&,
      google::spanner::admin::instance::v1::GetInstanceConfigRequest const&)
      override;

  StatusOr<google::spanner::admin::instance::v1::ListInstanceConfigsResponse>
  ListInstanceConfigs(
      grpc::ClientContext&,
      google::spanner::admin::instance::v1::ListInstanceConfigsRequest const&)
      override;

  StatusOr<google::spanner::admin::instance::v1::ListInstancesResponse>
  ListInstances(
      grpc::ClientContext&,
      google::spanner::admin::instance::v1::ListInstancesRequest const&)
      override;

  StatusOr<google::iam::v1::Policy> GetIamPolicy(
      grpc::ClientContext&,
      google::iam::v1::GetIamPolicyRequest const&) override;

  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      grpc::ClientContext&,
      google::iam::v1::SetIamPolicyRequest const&) override;

  StatusOr<google::iam::v1::TestIamPermissionsResponse> TestIamPermissions(
      grpc::ClientContext&,
      google::iam::v1::TestIamPermissionsRequest const&) override;

  StatusOr<google::longrunning::Operation> GetOperation(
      grpc::ClientContext& context,
      google::longrunning::GetOperationRequest const& request) override;
  //@}

 private:
  void SetMetadata(grpc::ClientContext& context,
                   std::string const& request_params);
  std::shared_ptr<InstanceAdminStub> child_;
  std::string api_client_header_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_METADATA_H
