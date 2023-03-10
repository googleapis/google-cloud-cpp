// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/instance_admin_logging.h"
#include "google/cloud/internal/log_wrapper.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace gsai = ::google::spanner::admin::instance;

using ::google::cloud::internal::LogWrapper;

StatusOr<gsai::v1::Instance> InstanceAdminLogging::GetInstance(
    grpc::ClientContext& context, gsai::v1::GetInstanceRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gsai::v1::GetInstanceRequest const& request) {
        return child_->GetInstance(context, request);
      },
      context, request, __func__, tracing_options_);
}

future<StatusOr<google::longrunning::Operation>>
InstanceAdminLogging::AsyncCreateInstance(
    CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
    gsai::v1::CreateInstanceRequest const& request) {
  return LogWrapper(
      [this](CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
             gsai::v1::CreateInstanceRequest const& request) {
        return child_->AsyncCreateInstance(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

future<StatusOr<google::longrunning::Operation>>
InstanceAdminLogging::AsyncUpdateInstance(
    CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
    gsai::v1::UpdateInstanceRequest const& request) {
  return google::cloud::internal::LogWrapper(
      [this](CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
             gsai::v1::UpdateInstanceRequest const& request) {
        return child_->AsyncUpdateInstance(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

Status InstanceAdminLogging::DeleteInstance(
    grpc::ClientContext& context,
    gsai::v1::DeleteInstanceRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gsai::v1::DeleteInstanceRequest const& request) {
        return child_->DeleteInstance(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<gsai::v1::InstanceConfig> InstanceAdminLogging::GetInstanceConfig(
    grpc::ClientContext& context,
    gsai::v1::GetInstanceConfigRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gsai::v1::GetInstanceConfigRequest const& request) {
        return child_->GetInstanceConfig(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<gsai::v1::ListInstanceConfigsResponse>
InstanceAdminLogging::ListInstanceConfigs(
    grpc::ClientContext& context,
    gsai::v1::ListInstanceConfigsRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gsai::v1::ListInstanceConfigsRequest const& request) {
        return child_->ListInstanceConfigs(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<gsai::v1::ListInstancesResponse> InstanceAdminLogging::ListInstances(
    grpc::ClientContext& context,
    gsai::v1::ListInstancesRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             gsai::v1::ListInstancesRequest const& request) {
        return child_->ListInstances(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::iam::v1::Policy> InstanceAdminLogging::GetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::GetIamPolicyRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::iam::v1::GetIamPolicyRequest const& request) {
        return child_->GetIamPolicy(context, request);
      },
      context, request, __func__, tracing_options_);
}

StatusOr<google::iam::v1::Policy> InstanceAdminLogging::SetIamPolicy(
    grpc::ClientContext& context,
    google::iam::v1::SetIamPolicyRequest const& request) {
  return LogWrapper(
      [this](grpc::ClientContext& context,
             google::iam::v1::SetIamPolicyRequest const& request) {
        return child_->SetIamPolicy(context, request);
      },
      context, request, __func__, tracing_options_);
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
      context, request, __func__, tracing_options_);
}

future<StatusOr<google::longrunning::Operation>>
InstanceAdminLogging::AsyncGetOperation(
    CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
    google::longrunning::GetOperationRequest const& request) {
  return google::cloud::internal::LogWrapper(
      [this](CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
             google::longrunning::GetOperationRequest const& request) {
        return child_->AsyncGetOperation(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

future<Status> InstanceAdminLogging::AsyncCancelOperation(
    CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
    google::longrunning::CancelOperationRequest const& request) {
  return google::cloud::internal::LogWrapper(
      [this](CompletionQueue& cq, std::shared_ptr<grpc::ClientContext> context,
             google::longrunning::CancelOperationRequest const& request) {
        return child_->AsyncCancelOperation(cq, std::move(context), request);
      },
      cq, std::move(context), request, __func__, tracing_options_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
