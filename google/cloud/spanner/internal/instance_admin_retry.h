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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_RETRY_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_RETRY_H_

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/internal/instance_admin_stub.h"
#include "google/cloud/spanner/polling_policy.h"
#include "google/cloud/spanner/retry_policy.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
/**
 * Implements the retry Decorator for InstanceAdminStub.
 */
class InstanceAdminRetry : public InstanceAdminStub {
 public:
  template <typename... PolicyOverrides>
  explicit InstanceAdminRetry(std::shared_ptr<InstanceAdminStub> child,
                              PolicyOverrides&&... policies)
      : InstanceAdminRetry(PrivateConstructorTag{}, std::move(child)) {
    OverridePolicies(std::forward<PolicyOverrides>(policies)...);
  }

  ~InstanceAdminRetry() override = default;

  //@{
  /**
   * @name Override the functions from `InstanceAdminStub`.
   *
   * Run the retry loop (if appropriate) for the child InstanceAdminStub.
   */
  StatusOr<google::spanner::admin::instance::v1::Instance> GetInstance(
      grpc::ClientContext&,
      google::spanner::admin::instance::v1::GetInstanceRequest const&) override;

  StatusOr<google::spanner::admin::instance::v1::ListInstancesResponse>
  ListInstances(
      grpc::ClientContext&,
      google::spanner::admin::instance::v1::ListInstancesRequest const&)
      override;
  StatusOr<google::iam::v1::TestIamPermissionsResponse> TestIamPermissions(
      grpc::ClientContext&,
      google::iam::v1::TestIamPermissionsRequest const&) override;
  //@}

 private:
  void OverridePolicy(RetryPolicy const& p) { retry_policy_ = p.clone(); }
  void OverridePolicy(BackoffPolicy const& p) { backoff_policy_ = p.clone(); }
  void OverridePolicies() {}
  template <typename Policy, typename... Policies>
  void OverridePolicies(Policy&& p, Policies&&... policies) {
    OverridePolicy(std::forward<Policy>(p));
    OverridePolicies(std::forward<Policies>(policies)...);
  }

  struct PrivateConstructorTag {};

  explicit InstanceAdminRetry(PrivateConstructorTag,
                              std::shared_ptr<InstanceAdminStub> child);

  std::shared_ptr<InstanceAdminStub> child_;
  std::unique_ptr<RetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INTERNAL_INSTANCE_ADMIN_RETRY_H_
