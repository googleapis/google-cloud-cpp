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
#include "google/cloud/spanner/internal/instance_admin_retry.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace gcsa = ::google::spanner::admin::instance::v1;

namespace {
class InstanceAdminConnectionImpl : public InstanceAdminConnection {
 public:
  explicit InstanceAdminConnectionImpl(
      std::shared_ptr<internal::InstanceAdminStub> stub)
      : stub_(std::move(stub)) {}
  ~InstanceAdminConnectionImpl() override = default;

  StatusOr<gcsa::Instance> GetInstance(GetInstanceParams gip) override {
    gcsa::GetInstanceRequest request;
    request.set_name(std::move(gip.instance_name));
    grpc::ClientContext context;
    return stub_->GetInstance(context, request);
  }

  ListInstancesRange ListInstances(ListInstancesParams params) override {
    gcsa::ListInstancesRequest request;
    request.set_parent("projects/" + params.project_id);
    request.set_filter(std::move(params.filter));
    request.clear_page_token();
    auto stub = stub_;
    return ListInstancesRange(
        std::move(request),
        [stub](gcsa::ListInstancesRequest const& r) {
          grpc::ClientContext context;
          return stub->ListInstances(context, r);
        },
        [](gcsa::ListInstancesResponse r) {
          std::vector<gcsa::Instance> result(r.instances().size());
          auto& dbs = *r.mutable_instances();
          std::move(dbs.begin(), dbs.end(), result.begin());
          return result;
        });
  }

  StatusOr<google::iam::v1::Policy> GetIamPolicy(
      GetIamPolicyParams p) override {
    google::iam::v1::GetIamPolicyRequest request;
    request.set_resource(std::move(p.instance_name));
    grpc::ClientContext context;
    return stub_->GetIamPolicy(context, request);
  }

  StatusOr<google::iam::v1::TestIamPermissionsResponse> TestIamPermissions(
      TestIamPermissionsParams p) override {
    google::iam::v1::TestIamPermissionsRequest request;
    request.set_resource(std::move(p.instance_name));
    for (auto& permission : p.permissions) {
      request.add_permissions(std::move(permission));
    }
    grpc::ClientContext context;
    return stub_->TestIamPermissions(context, request);
  }

 private:
  std::shared_ptr<internal::InstanceAdminStub> stub_;
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
  return std::make_shared<InstanceAdminConnectionImpl>(
      std::make_shared<internal::InstanceAdminRetry>(std::move(base_stub)));
}

std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    std::shared_ptr<internal::InstanceAdminStub> base_stub,
    ConnectionOptions const&, std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy) {
  return std::make_shared<InstanceAdminConnectionImpl>(
      std::make_shared<internal::InstanceAdminRetry>(
          std::move(base_stub), *retry_policy, *backoff_policy));
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
