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

// TODO(#7356): Remove this file after the deprecation period expires
#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/spanner/instance_admin_connection.h"
#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/internal/defaults.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/internal/async_long_running_operation.h"
#include "google/cloud/internal/retry_loop.h"
#include <grpcpp/grpcpp.h>
#include <chrono>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace gsai = ::google::spanner::admin::instance;

using ::google::cloud::Idempotency;

namespace {

class InstanceAdminConnectionImpl : public InstanceAdminConnection {
 public:
  InstanceAdminConnectionImpl(
      std::shared_ptr<spanner_internal::InstanceAdminStub> stub, Options opts)
      : stub_(std::move(stub)),
        opts_(std::move(opts)),
        retry_policy_prototype_(opts_.get<SpannerRetryPolicyOption>()->clone()),
        backoff_policy_prototype_(
            opts_.get<SpannerBackoffPolicyOption>()->clone()),
        polling_policy_prototype_(
            opts_.get<SpannerPollingPolicyOption>()->clone()),
        background_threads_(internal::MakeBackgroundThreadsFactory(opts_)()) {}

  ~InstanceAdminConnectionImpl() override = default;

  Options options() override { return opts_; }

  StatusOr<gsai::v1::Instance> GetInstance(GetInstanceParams gip) override {
    gsai::v1::GetInstanceRequest request;
    request.set_name(std::move(gip.instance_name));
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               gsai::v1::GetInstanceRequest const& request) {
          return stub_->GetInstance(context, request);
        },
        request, __func__);
  }

  future<StatusOr<gsai::v1::Instance>> CreateInstance(
      CreateInstanceParams p) override {
    auto& stub = stub_;
    return google::cloud::internal::AsyncLongRunningOperation<
        gsai::v1::Instance>(
        background_threads_->cq(), std::move(p.request),
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               gsai::v1::CreateInstanceRequest const& request) {
          return stub->AsyncCreateInstance(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::GetOperationRequest const& request) {
          return stub->AsyncGetOperation(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::CancelOperationRequest const& request) {
          return stub->AsyncCancelOperation(cq, std::move(context), request);
        },
        &google::cloud::internal::ExtractLongRunningResultResponse<
            gsai::v1::Instance>,
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kNonIdempotent, polling_policy_prototype_->clone(),
        __func__);
  }

  future<StatusOr<gsai::v1::Instance>> UpdateInstance(
      UpdateInstanceParams p) override {
    auto& stub = stub_;
    return google::cloud::internal::AsyncLongRunningOperation<
        gsai::v1::Instance>(
        background_threads_->cq(), std::move(p.request),
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               gsai::v1::UpdateInstanceRequest const& request) {
          return stub->AsyncUpdateInstance(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::GetOperationRequest const& request) {
          return stub->AsyncGetOperation(cq, std::move(context), request);
        },
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               google::longrunning::CancelOperationRequest const& request) {
          return stub->AsyncCancelOperation(cq, std::move(context), request);
        },
        &google::cloud::internal::ExtractLongRunningResultResponse<
            gsai::v1::Instance>,
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent, polling_policy_prototype_->clone(), __func__);
  }

  Status DeleteInstance(DeleteInstanceParams p) override {
    gsai::v1::DeleteInstanceRequest request;
    request.set_name(std::move(p.instance_name));
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               gsai::v1::DeleteInstanceRequest const& request) {
          return stub_->DeleteInstance(context, request);
        },
        request, __func__);
  }

  StatusOr<gsai::v1::InstanceConfig> GetInstanceConfig(
      GetInstanceConfigParams p) override {
    gsai::v1::GetInstanceConfigRequest request;
    request.set_name(std::move(p.instance_config_name));
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               gsai::v1::GetInstanceConfigRequest const& request) {
          return stub_->GetInstanceConfig(context, request);
        },
        request, __func__);
  }

  ListInstanceConfigsRange ListInstanceConfigs(
      ListInstanceConfigsParams params) override {
    gsai::v1::ListInstanceConfigsRequest request;
    request.set_parent("projects/" + params.project_id);
    request.clear_page_token();
    auto& stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry = std::shared_ptr<RetryPolicy>(retry_policy_prototype_->clone());
    auto backoff =
        std::shared_ptr<BackoffPolicy>(backoff_policy_prototype_->clone());

    char const* function_name = __func__;
    return google::cloud::internal::MakePaginationRange<
        ListInstanceConfigsRange>(
        std::move(request),
        [stub, retry, backoff,
         function_name](gsai::v1::ListInstanceConfigsRequest const& r) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& context,
                     gsai::v1::ListInstanceConfigsRequest const& request) {
                return stub->ListInstanceConfigs(context, request);
              },
              r, function_name);
        },
        [](gsai::v1::ListInstanceConfigsResponse r) {
          std::vector<gsai::v1::InstanceConfig> result(
              r.instance_configs().size());
          auto& configs = *r.mutable_instance_configs();
          std::move(configs.begin(), configs.end(), result.begin());
          return result;
        });
  }

  ListInstancesRange ListInstances(ListInstancesParams params) override {
    gsai::v1::ListInstancesRequest request;
    request.set_parent("projects/" + params.project_id);
    request.set_filter(std::move(params.filter));
    request.clear_page_token();
    auto& stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry = std::shared_ptr<RetryPolicy>(retry_policy_prototype_->clone());
    auto backoff =
        std::shared_ptr<BackoffPolicy>(backoff_policy_prototype_->clone());

    char const* function_name = __func__;
    return google::cloud::internal::MakePaginationRange<ListInstancesRange>(
        std::move(request),
        [stub, retry, backoff,
         function_name](gsai::v1::ListInstancesRequest const& r) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& context,
                     gsai::v1::ListInstancesRequest const& request) {
                return stub->ListInstances(context, request);
              },
              r, function_name);
        },
        [](gsai::v1::ListInstancesResponse r) {
          std::vector<gsai::v1::Instance> result(r.instances().size());
          auto& instances = *r.mutable_instances();
          std::move(instances.begin(), instances.end(), result.begin());
          return result;
        });
  }

  StatusOr<google::iam::v1::Policy> GetIamPolicy(
      GetIamPolicyParams p) override {
    google::iam::v1::GetIamPolicyRequest request;
    request.set_resource(std::move(p.instance_name));
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::iam::v1::GetIamPolicyRequest const& request) {
          return stub_->GetIamPolicy(context, request);
        },
        request, __func__);
  }

  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      SetIamPolicyParams p) override {
    google::iam::v1::SetIamPolicyRequest request;
    request.set_resource(std::move(p.instance_name));
    *request.mutable_policy() = std::move(p.policy);
    auto const idempotency = request.policy().etag().empty()
                                 ? Idempotency::kNonIdempotent
                                 : Idempotency::kIdempotent;
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        idempotency,
        [this](grpc::ClientContext& context,
               google::iam::v1::SetIamPolicyRequest const& request) {
          return stub_->SetIamPolicy(context, request);
        },
        request, __func__);
  }

  StatusOr<google::iam::v1::TestIamPermissionsResponse> TestIamPermissions(
      TestIamPermissionsParams p) override {
    google::iam::v1::TestIamPermissionsRequest request;
    request.set_resource(std::move(p.instance_name));
    for (auto& permission : p.permissions) {
      request.add_permissions(std::move(permission));
    }
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               google::iam::v1::TestIamPermissionsRequest const& request) {
          return stub_->TestIamPermissions(context, request);
        },
        request, __func__);
  }

 private:
  std::shared_ptr<spanner_internal::InstanceAdminStub> stub_;
  Options opts_;
  std::unique_ptr<RetryPolicy const> retry_policy_prototype_;
  std::unique_ptr<BackoffPolicy const> backoff_policy_prototype_;
  std::unique_ptr<PollingPolicy const> polling_policy_prototype_;

  // Implementations of `BackgroundThreads` typically create a pool of
  // threads that are joined during destruction, so, to avoid ownership
  // cycles, those threads should never assume ownership of this object
  // (e.g., via a `std::shared_ptr<>`).
  std::unique_ptr<BackgroundThreads> background_threads_;
};

}  // namespace

InstanceAdminConnection::~InstanceAdminConnection() = default;

std::shared_ptr<spanner::InstanceAdminConnection> MakeInstanceAdminConnection(
    Options opts) {
  internal::CheckExpectedOptions<CommonOptionList, GrpcOptionList,
                                 SpannerPolicyOptionList>(opts, __func__);
  opts = spanner_internal::DefaultAdminOptions(std::move(opts));
  auto stub = spanner_internal::CreateDefaultInstanceAdminStub(opts);
  return std::make_shared<spanner::InstanceAdminConnectionImpl>(
      std::move(stub), std::move(opts));
}

std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    ConnectionOptions const& options) {
  return MakeInstanceAdminConnection(internal::MakeOptions(options));
}

std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    ConnectionOptions const& options, std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::unique_ptr<PollingPolicy> polling_policy) {
  auto opts = internal::MakeOptions(options);
  opts.set<SpannerRetryPolicyOption>(std::move(retry_policy));
  opts.set<SpannerBackoffPolicyOption>(std::move(backoff_policy));
  opts.set<SpannerPollingPolicyOption>(std::move(polling_policy));
  return MakeInstanceAdminConnection(std::move(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner

namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<spanner::InstanceAdminConnection>
MakeInstanceAdminConnectionForTesting(std::shared_ptr<InstanceAdminStub> stub,
                                      Options opts) {
  opts = spanner_internal::DefaultAdminOptions(std::move(opts));
  return std::make_shared<spanner::InstanceAdminConnectionImpl>(
      std::move(stub), std::move(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
