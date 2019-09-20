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

/**
 * Implements the logging Decorator for DatabaseAdminStub.
 */
class InstanceAdminLogging : public InstanceAdminStub {
 public:
  explicit InstanceAdminLogging(std::shared_ptr<InstanceAdminStub> child)
      : child_(std::move(child)) {}

  ~InstanceAdminLogging() override = default;

  //@{
  /**
   * @name Override the functions from `DatabaseAdminStub`.
   *
   * Run the logging loop (if appropriate) for the child DatabaseAdminStub.
   */
  ///
  StatusOr<google::spanner::admin::instance::v1::Instance> GetInstance(
      grpc::ClientContext&,
      google::spanner::admin::instance::v1::GetInstanceRequest const&) override;

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
