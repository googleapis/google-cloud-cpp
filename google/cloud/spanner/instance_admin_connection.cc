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
inline namespace SPANNER_CLIENT_NS {

namespace gcsa = ::google::spanner::admin::instance::v1;
namespace giam = ::google::iam::v1;
using google::cloud::internal::Idempotency;

namespace {

class InstanceAdminConnectionImpl : public InstanceAdminConnection {
 public:
  InstanceAdminConnectionImpl(
      std::shared_ptr<spanner_internal::InstanceAdminStub> stub,
      Options const& opts)
      : stub_(std::move(stub)),
        retry_policy_prototype_(opts.get<SpannerRetryPolicyOption>()->clone()),
        backoff_policy_prototype_(
            opts.get<SpannerBackoffPolicyOption>()->clone()),
        polling_policy_prototype_(
            opts.get<SpannerPollingPolicyOption>()->clone()),
        background_threads_(internal::MakeBackgroundThreadsFactory(opts)()) {}

  ~InstanceAdminConnectionImpl() override = default;

  StatusOr<gcsa::Instance> GetInstance(GetInstanceParams gip) override {
    gcsa::GetInstanceRequest request;
    request.set_name(std::move(gip.instance_name));
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               gcsa::GetInstanceRequest const& request) {
          return stub_->GetInstance(context, request);
        },
        request, __func__);
  }

  future<StatusOr<gcsa::Instance>> CreateInstance(
      CreateInstanceParams p) override {
    auto stub = stub_;
    return google::cloud::internal::AsyncLongRunningOperation<gcsa::Instance>(
        background_threads_->cq(), std::move(p.request),
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               gcsa::CreateInstanceRequest const& request) {
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
            gcsa::Instance>,
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kNonIdempotent, polling_policy_prototype_->clone(),
        __func__);
  }

  future<StatusOr<gcsa::Instance>> UpdateInstance(
      UpdateInstanceParams p) override {
    auto stub = stub_;
    return google::cloud::internal::AsyncLongRunningOperation<gcsa::Instance>(
        background_threads_->cq(), std::move(p.request),
        [stub](google::cloud::CompletionQueue& cq,
               std::unique_ptr<grpc::ClientContext> context,
               gcsa::UpdateInstanceRequest const& request) {
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
            gcsa::Instance>,
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent, polling_policy_prototype_->clone(), __func__);
  }

  Status DeleteInstance(DeleteInstanceParams p) override {
    gcsa::DeleteInstanceRequest request;
    request.set_name(std::move(p.instance_name));
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               gcsa::DeleteInstanceRequest const& request) {
          return stub_->DeleteInstance(context, request);
        },
        request, __func__);
  }

  StatusOr<gcsa::InstanceConfig> GetInstanceConfig(
      GetInstanceConfigParams p) override {
    gcsa::GetInstanceConfigRequest request;
    request.set_name(std::move(p.instance_config_name));
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               gcsa::GetInstanceConfigRequest const& request) {
          return stub_->GetInstanceConfig(context, request);
        },
        request, __func__);
  }

  ListInstanceConfigsRange ListInstanceConfigs(
      ListInstanceConfigsParams params) override {
    gcsa::ListInstanceConfigsRequest request;
    request.set_parent("projects/" + params.project_id);
    request.clear_page_token();
    auto stub = stub_;
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
         function_name](gcsa::ListInstanceConfigsRequest const& r) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& context,
                     gcsa::ListInstanceConfigsRequest const& request) {
                return stub->ListInstanceConfigs(context, request);
              },
              r, function_name);
        },
        [](gcsa::ListInstanceConfigsResponse r) {
          std::vector<gcsa::InstanceConfig> result(r.instance_configs().size());
          auto& configs = *r.mutable_instance_configs();
          std::move(configs.begin(), configs.end(), result.begin());
          return result;
        });
  }

  ListInstancesRange ListInstances(ListInstancesParams params) override {
    gcsa::ListInstancesRequest request;
    request.set_parent("projects/" + params.project_id);
    request.set_filter(std::move(params.filter));
    request.clear_page_token();
    auto stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry = std::shared_ptr<RetryPolicy>(retry_policy_prototype_->clone());
    auto backoff =
        std::shared_ptr<BackoffPolicy>(backoff_policy_prototype_->clone());

    char const* function_name = __func__;
    return google::cloud::internal::MakePaginationRange<ListInstancesRange>(
        std::move(request),
        [stub, retry, backoff,
         function_name](gcsa::ListInstancesRequest const& r) {
          return RetryLoop(
              retry->clone(), backoff->clone(), Idempotency::kIdempotent,
              [stub](grpc::ClientContext& context,
                     gcsa::ListInstancesRequest const& request) {
                return stub->ListInstances(context, request);
              },
              r, function_name);
        },
        [](gcsa::ListInstancesResponse r) {
          std::vector<gcsa::Instance> result(r.instances().size());
          auto& instances = *r.mutable_instances();
          std::move(instances.begin(), instances.end(), result.begin());
          return result;
        });
  }

  StatusOr<giam::Policy> GetIamPolicy(GetIamPolicyParams p) override {
    google::iam::v1::GetIamPolicyRequest request;
    request.set_resource(std::move(p.instance_name));
    return RetryLoop(
        retry_policy_prototype_->clone(), backoff_policy_prototype_->clone(),
        Idempotency::kIdempotent,
        [this](grpc::ClientContext& context,
               giam::GetIamPolicyRequest const& request) {
          return stub_->GetIamPolicy(context, request);
        },
        request, __func__);
  }

  StatusOr<giam::Policy> SetIamPolicy(SetIamPolicyParams p) override {
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
               giam::SetIamPolicyRequest const& request) {
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
               giam::TestIamPermissionsRequest const& request) {
          return stub_->TestIamPermissions(context, request);
        },
        request, __func__);
  }

 private:
  std::shared_ptr<spanner_internal::InstanceAdminStub> stub_;
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

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner

namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {

std::shared_ptr<spanner::InstanceAdminConnection>
MakeInstanceAdminConnectionForTesting(std::shared_ptr<InstanceAdminStub> stub,
                                      Options opts) {
  opts = spanner_internal::DefaultAdminOptions(std::move(opts));
  return std::make_shared<spanner::InstanceAdminConnectionImpl>(
      std::move(stub), std::move(opts));
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
