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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_LOGGING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_LOGGING_H

#include "google/cloud/spanner/internal/instance_admin_stub.h"
#include "google/cloud/spanner/tracing_options.h"
#include "google/cloud/spanner/version.h"
#include <grpcpp/grpcpp.h>
#include <memory>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Implements the logging Decorator for InstanceAdminStub.
 */
class InstanceAdminLogging : public InstanceAdminStub {
 public:
  InstanceAdminLogging(std::shared_ptr<InstanceAdminStub> child,
                       TracingOptions tracing_options)
      : child_(std::move(child)),
        tracing_options_(std::move(tracing_options)) {}

  ~InstanceAdminLogging() override = default;

  //@{
  /**
   * @name Override the functions from `InstanceAdminStub`.
   *
   * Run the logging loop (if appropriate) for the child InstanceAdminStub.
   */
  ///
  StatusOr<google::spanner::admin::instance::v1::Instance> GetInstance(
      grpc::ClientContext&,
      google::spanner::admin::instance::v1::GetInstanceRequest const&) override;

  future<StatusOr<google::longrunning::Operation>> AsyncCreateInstance(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::spanner::admin::instance::v1::CreateInstanceRequest const&)
      override;

  future<StatusOr<google::longrunning::Operation>> AsyncUpdateInstance(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
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

  future<StatusOr<google::longrunning::Operation>> AsyncGetOperation(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::longrunning::GetOperationRequest const&) override;

  future<Status> AsyncCancelOperation(
      CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
      google::longrunning::CancelOperationRequest const&) override;
  //@}

 private:
  std::shared_ptr<InstanceAdminStub> child_;
  TracingOptions tracing_options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_LOGGING_H
