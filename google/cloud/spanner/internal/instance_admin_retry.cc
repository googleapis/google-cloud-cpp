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

#include "google/cloud/spanner/internal/instance_admin_retry.h"
#include "google/cloud/spanner/internal/retry_loop.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_RETRY_TIMEOUT
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_RETRY_TIMEOUT \
  std::chrono::minutes(30)
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_RETRY_TIMEOUT

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_INITIAL_BACKOFF
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_INITIAL_BACKOFF \
  std::chrono::seconds(1)
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_INITIAL_BACKOFF

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_MAXIMUM_BACKOFF
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_MAXIMUM_BACKOFF \
  std::chrono::minutes(5)
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_MAXIMUM_BACKOFF

#ifndef GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_BACKOFF_SCALING
#define GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_BACKOFF_SCALING 2.0
#endif  // GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_BACKOFF_SCALING

std::unique_ptr<RetryPolicy> DefaultInstanceAdminRetryPolicy() {
  return google::cloud::spanner::LimitedTimeRetryPolicy(
             GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_RETRY_TIMEOUT)
      .clone();
}

std::unique_ptr<BackoffPolicy> DefaultInstanceAdminBackoffPolicy() {
  return google::cloud::spanner::ExponentialBackoffPolicy(
             GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_INITIAL_BACKOFF,
             GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_MAXIMUM_BACKOFF,
             GOOGLE_CLOUD_CPP_SPANNER_ADMIN_DEFAULT_BACKOFF_SCALING)
      .clone();
}

InstanceAdminRetry::InstanceAdminRetry(PrivateConstructorTag,
                                       std::shared_ptr<InstanceAdminStub> child)
    : child_(std::move(child)),
      retry_policy_(DefaultInstanceAdminRetryPolicy()),
      backoff_policy_(DefaultInstanceAdminBackoffPolicy()) {}

namespace gcsa = google::spanner::admin::instance::v1;
namespace giam = google::iam::v1;

StatusOr<gcsa::Instance> InstanceAdminRetry::GetInstance(
    grpc::ClientContext& context, gcsa::GetInstanceRequest const& request) {
  return RetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(), true,
      [this](grpc::ClientContext& context,
             gcsa::GetInstanceRequest const& request) {
        return child_->GetInstance(context, request);
      },
      context, request, __func__);
}

StatusOr<gcsa::ListInstancesResponse> InstanceAdminRetry::ListInstances(
    grpc::ClientContext& context, gcsa::ListInstancesRequest const& request) {
  return RetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(), true,
      [this](grpc::ClientContext& context,
             gcsa::ListInstancesRequest const& request) {
        return child_->ListInstances(context, request);
      },
      context, request, __func__);
}

StatusOr<google::iam::v1::Policy> InstanceAdminRetry::GetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::GetIamPolicyRequest const& request) {
  return RetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(), true,
      [this](grpc::ClientContext& context,
             giam::GetIamPolicyRequest const& request) {
        return child_->GetIamPolicy(context, request);
      },
      context, request, __func__);
}

StatusOr<giam::TestIamPermissionsResponse>
InstanceAdminRetry::TestIamPermissions(
    grpc::ClientContext& context,
    giam::TestIamPermissionsRequest const& request) {
  return RetryLoop(
      retry_policy_->clone(), backoff_policy_->clone(), true,
      [this](grpc::ClientContext& context,
             giam::TestIamPermissionsRequest const& request) {
        return child_->TestIamPermissions(context, request);
      },
      context, request, __func__);
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
