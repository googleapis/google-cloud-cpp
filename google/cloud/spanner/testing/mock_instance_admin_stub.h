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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_INSTANCE_ADMIN_STUB_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_INSTANCE_ADMIN_STUB_H

#include "google/cloud/spanner/internal/instance_admin_stub.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

class MockInstanceAdminStub
    : public google::cloud::spanner_internal::InstanceAdminStub {
 public:
  MOCK_METHOD(StatusOr<google::spanner::admin::instance::v1::Instance>,
              GetInstance,
              (grpc::ClientContext&,
               google::spanner::admin::instance::v1::GetInstanceRequest const&),
              (override));
  MOCK_METHOD(
      future<StatusOr<google::longrunning::Operation>>, AsyncCreateInstance,
      (CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
       google::spanner::admin::instance::v1::CreateInstanceRequest const&),
      (override));
  MOCK_METHOD(
      future<StatusOr<google::longrunning::Operation>>, AsyncUpdateInstance,
      (CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
       google::spanner::admin::instance::v1::UpdateInstanceRequest const&),
      (override));
  MOCK_METHOD(
      Status, DeleteInstance,
      (grpc::ClientContext&,
       google::spanner::admin::instance::v1::DeleteInstanceRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<google::spanner::admin::instance::v1::InstanceConfig>,
      GetInstanceConfig,
      (grpc::ClientContext&,
       google::spanner::admin::instance::v1::GetInstanceConfigRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<
          google::spanner::admin::instance::v1::ListInstanceConfigsResponse>,
      ListInstanceConfigs,
      (grpc::ClientContext&,
       google::spanner::admin::instance::v1::ListInstanceConfigsRequest const&),
      (override));
  MOCK_METHOD(
      StatusOr<google::spanner::admin::instance::v1::ListInstancesResponse>,
      ListInstances,
      (grpc::ClientContext&,
       google::spanner::admin::instance::v1::ListInstancesRequest const&),
      (override));
  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, GetIamPolicy,
              (grpc::ClientContext&,
               google::iam::v1::GetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, SetIamPolicy,
              (grpc::ClientContext&,
               google::iam::v1::SetIamPolicyRequest const&),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions,
              (grpc::ClientContext&,
               google::iam::v1::TestIamPermissionsRequest const&),
              (override));
  MOCK_METHOD(future<StatusOr<google::longrunning::Operation>>,
              AsyncGetOperation,
              (CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
               google::longrunning::GetOperationRequest const&),
              (override));
  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
               google::longrunning::CancelOperationRequest const&),
              (override));
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_INSTANCE_ADMIN_STUB_H
