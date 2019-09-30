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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_LOGGING_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_LOGGING_H_

#include "google/cloud/spanner/internal/instance_admin_stub.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace gcsa = google::spanner::admin::instance::v1;

/**
 * Implements the logging Decorator for InstanceAdminStub.
 */
class InstanceAdminLogging : public InstanceAdminStub {
 public:
  explicit InstanceAdminLogging(std::shared_ptr<InstanceAdminStub> child)
      : child_(std::move(child)) {}

  ~InstanceAdminLogging() override = default;

  //@{
  /**
   * @name Override the functions from `InstanceAdminStub`.
   *
   * Run the logging loop (if appropriate) for the child InstanceAdminStub.
   */
  ///
  StatusOr<gcsa::Instance> GetInstance(
      grpc::ClientContext&, gcsa::GetInstanceRequest const&) override;

  StatusOr<google::longrunning::Operation> CreateInstance(
      grpc::ClientContext&, gcsa::CreateInstanceRequest const&) override;

  StatusOr<google::longrunning::Operation> UpdateInstance(
      grpc::ClientContext&, gcsa::UpdateInstanceRequest const&) override;

  Status DeleteInstance(grpc::ClientContext&,
                        gcsa::DeleteInstanceRequest const&) override;

  StatusOr<gcsa::InstanceConfig> GetInstanceConfig(
      grpc::ClientContext&, gcsa::GetInstanceConfigRequest const&) override;

  StatusOr<gcsa::ListInstanceConfigsResponse> ListInstanceConfigs(
      grpc::ClientContext&, gcsa::ListInstanceConfigsRequest const&) override;

  StatusOr<gcsa::ListInstancesResponse> ListInstances(
      grpc::ClientContext&, gcsa::ListInstancesRequest const&) override;

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
  std::shared_ptr<InstanceAdminStub> child_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_LOGGING_H_
