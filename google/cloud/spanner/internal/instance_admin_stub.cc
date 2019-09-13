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

#include "google/cloud/spanner/internal/instance_admin_stub.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include <google/longrunning/operations.grpc.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

namespace gcsa = google::spanner::admin::instance::v1;
namespace giam = google::iam::v1;

InstanceAdminStub::~InstanceAdminStub() = default;

class DefaultInstanceAdminStub : public InstanceAdminStub {
 public:
  DefaultInstanceAdminStub(
      std::unique_ptr<gcsa::InstanceAdmin::Stub> instance_admin,
      std::unique_ptr<google::longrunning::Operations::Stub> operations)
      : instance_admin_(std::move(instance_admin)),
        operations_(std::move(operations)) {}

  ~DefaultInstanceAdminStub() override = default;

  StatusOr<gcsa::Instance> GetInstance(
      grpc::ClientContext& context,
      gcsa::GetInstanceRequest const& request) override {
    gcsa::Instance response;
    auto status = instance_admin_->GetInstance(&context, request, &response);
    if (!status.ok()) {
      return google::cloud::grpc_utils::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<gcsa::ListInstancesResponse> ListInstances(
      grpc::ClientContext& context,
      gcsa::ListInstancesRequest const& request) override {
    gcsa::ListInstancesResponse response;
    auto status = instance_admin_->ListInstances(&context, request, &response);
    if (!status.ok()) {
      return grpc_utils::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<giam::Policy> GetIamPolicy(
      grpc::ClientContext& context,
      giam::GetIamPolicyRequest const& request) override {
    giam::Policy response;
    auto status = instance_admin_->GetIamPolicy(&context, request, &response);
    if (!status.ok()) {
      return grpc_utils::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<giam::Policy> SetIamPolicy(
      grpc::ClientContext& context,
      giam::SetIamPolicyRequest const& request) override {
    giam::Policy response;
    auto status = instance_admin_->SetIamPolicy(&context, request, &response);
    if (!status.ok()) {
      return grpc_utils::MakeStatusFromRpcError(status);
    }
    return response;
  }

  StatusOr<giam::TestIamPermissionsResponse> TestIamPermissions(
      grpc::ClientContext& context,
      giam::TestIamPermissionsRequest const& request) override {
    giam::TestIamPermissionsResponse response;
    auto status =
        instance_admin_->TestIamPermissions(&context, request, &response);
    if (!status.ok()) {
      return grpc_utils::MakeStatusFromRpcError(status);
    }
    return response;
  }

 private:
  std::unique_ptr<gcsa::InstanceAdmin::Stub> instance_admin_;
  std::unique_ptr<google::longrunning::Operations::Stub> operations_;
};

std::shared_ptr<InstanceAdminStub> CreateDefaultInstanceAdminStub(
    ConnectionOptions const& options) {
  auto channel =
      grpc::CreateCustomChannel(options.endpoint(), options.credentials(),
                                options.CreateChannelArguments());
  auto spanner_grpc_stub = gcsa::InstanceAdmin::NewStub(channel);
  auto longrunning_grpc_stub =
      google::longrunning::Operations::NewStub(channel);

  return std::make_shared<DefaultInstanceAdminStub>(
      std::move(spanner_grpc_stub), std::move(longrunning_grpc_stub));
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
