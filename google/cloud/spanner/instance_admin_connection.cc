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
#include "google/cloud/spanner/internal/retry_loop.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace gcsa = ::google::spanner::admin::instance::v1;
namespace giam = google::iam::v1;

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

namespace {

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

class InstanceAdminConnectionImpl : public InstanceAdminConnection {
 public:
  InstanceAdminConnectionImpl(std::shared_ptr<internal::InstanceAdminStub> stub,
                              std::unique_ptr<RetryPolicy> retry_policy,
                              std::unique_ptr<BackoffPolicy> backoff_policy)
      : stub_(std::move(stub)),
        retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)) {}

  explicit InstanceAdminConnectionImpl(
      std::shared_ptr<internal::InstanceAdminStub> stub)
      : InstanceAdminConnectionImpl(std::move(stub),
                                    DefaultInstanceAdminRetryPolicy(),
                                    DefaultInstanceAdminBackoffPolicy()) {}

  ~InstanceAdminConnectionImpl() override = default;

  StatusOr<gcsa::Instance> GetInstance(GetInstanceParams gip) override {
    gcsa::GetInstanceRequest request;
    request.set_name(std::move(gip.instance_name));
    return internal::RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(), true,
        [this](grpc::ClientContext& context,
               gcsa::GetInstanceRequest const& request) {
          return stub_->GetInstance(context, request);
        },
        request, __func__);
  }

  StatusOr<gcsa::InstanceConfig> GetInstanceConfig(
      GetInstanceConfigParams p) override {
    gcsa::GetInstanceConfigRequest request;
    request.set_name(std::move(p.instance_config_name));
    return internal::RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(), true,
        [this](grpc::ClientContext& context,
               gcsa::GetInstanceConfigRequest const& request) {
          return stub_->GetInstanceConfig(context, request);
        },
        request, __func__);
  }

  ListInstancesRange ListInstances(ListInstancesParams params) override {
    gcsa::ListInstancesRequest request;
    request.set_parent("projects/" + params.project_id);
    request.set_filter(std::move(params.filter));
    request.clear_page_token();
    auto stub = stub_;
    // Because we do not have C++14 generalized lambda captures we cannot just
    // use the unique_ptr<> here, so convert to shared_ptr<> instead.
    auto retry = std::shared_ptr<RetryPolicy>(retry_policy_->clone());
    auto backoff = std::shared_ptr<BackoffPolicy>(backoff_policy_->clone());

    char const* function_name = __func__;
    return ListInstancesRange(
        std::move(request),
        [stub, retry, backoff,
         function_name](gcsa::ListInstancesRequest const& r) {
          return RetryLoop(
              retry->clone(), backoff->clone(), true,
              [stub](grpc::ClientContext& context,
                     gcsa::ListInstancesRequest const& request) {
                return stub->ListInstances(context, request);
              },
              r, function_name);
        },
        [](gcsa::ListInstancesResponse r) {
          std::vector<gcsa::Instance> result(r.instances().size());
          auto& dbs = *r.mutable_instances();
          std::move(dbs.begin(), dbs.end(), result.begin());
          return result;
        });
  }

  StatusOr<giam::Policy> GetIamPolicy(GetIamPolicyParams p) override {
    google::iam::v1::GetIamPolicyRequest request;
    request.set_resource(std::move(p.instance_name));
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(), true,
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
    bool is_idempotent = !request.policy().etag().empty();
    return RetryLoop(
        retry_policy_->clone(), backoff_policy_->clone(), is_idempotent,
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
        retry_policy_->clone(), backoff_policy_->clone(), true,
        [this](grpc::ClientContext& context,
               giam::TestIamPermissionsRequest const& request) {
          return stub_->TestIamPermissions(context, request);
        },
        request, __func__);
  }

 private:
  std::shared_ptr<internal::InstanceAdminStub> stub_;
  std::unique_ptr<RetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;
};
}  // namespace

InstanceAdminConnection::~InstanceAdminConnection() = default;

std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    ConnectionOptions const& options) {
  return internal::MakeInstanceAdminConnection(
      internal::CreateDefaultInstanceAdminStub(options), options);
}

namespace internal {

std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    std::shared_ptr<internal::InstanceAdminStub> base_stub,
    ConnectionOptions const&) {
  return std::make_shared<InstanceAdminConnectionImpl>(std::move(base_stub));
}

std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    std::shared_ptr<internal::InstanceAdminStub> base_stub,
    ConnectionOptions const&, std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy) {
  return std::make_shared<InstanceAdminConnectionImpl>(
      std::move(base_stub), std::move(retry_policy), std::move(backoff_policy));
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
