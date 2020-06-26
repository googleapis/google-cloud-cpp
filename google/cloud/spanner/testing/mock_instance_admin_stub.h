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

namespace google {
namespace cloud {
namespace spanner_testing {
inline namespace SPANNER_CLIENT_NS {

class MockInstanceAdminStub
    : public google::cloud::spanner::internal::InstanceAdminStub {
 public:
  MOCK_METHOD2(
      GetInstance,
      StatusOr<google::spanner::admin::instance::v1::Instance>(
          grpc::ClientContext&,
          google::spanner::admin::instance::v1::GetInstanceRequest const&));
  MOCK_METHOD2(
      CreateInstance,
      StatusOr<google::longrunning::Operation>(
          grpc::ClientContext&,
          google::spanner::admin::instance::v1::CreateInstanceRequest const&));
  MOCK_METHOD2(
      UpdateInstance,
      StatusOr<google::longrunning::Operation>(
          grpc::ClientContext&,
          google::spanner::admin::instance::v1::UpdateInstanceRequest const&));
  MOCK_METHOD2(
      DeleteInstance,
      Status(
          grpc::ClientContext&,
          google::spanner::admin::instance::v1::DeleteInstanceRequest const&));
  MOCK_METHOD2(GetInstanceConfig,
               StatusOr<google::spanner::admin::instance::v1::InstanceConfig>(
                   grpc::ClientContext&, google::spanner::admin::instance::v1::
                                             GetInstanceConfigRequest const&));
  MOCK_METHOD2(
      ListInstanceConfigs,
      StatusOr<
          google::spanner::admin::instance::v1::ListInstanceConfigsResponse>(
          grpc::ClientContext&, google::spanner::admin::instance::v1::
                                    ListInstanceConfigsRequest const&));
  MOCK_METHOD2(
      ListInstances,
      StatusOr<google::spanner::admin::instance::v1::ListInstancesResponse>(
          grpc::ClientContext&,
          google::spanner::admin::instance::v1::ListInstancesRequest const&));
  MOCK_METHOD2(GetIamPolicy, StatusOr<google::iam::v1::Policy>(
                                 grpc::ClientContext&,
                                 google::iam::v1::GetIamPolicyRequest const&));
  MOCK_METHOD2(SetIamPolicy, StatusOr<google::iam::v1::Policy>(
                                 grpc::ClientContext&,
                                 google::iam::v1::SetIamPolicyRequest const&));
  MOCK_METHOD2(TestIamPermissions,
               StatusOr<google::iam::v1::TestIamPermissionsResponse>(
                   grpc::ClientContext&,
                   google::iam::v1::TestIamPermissionsRequest const&));
  MOCK_METHOD2(GetOperation,
               StatusOr<google::longrunning::Operation>(
                   grpc::ClientContext&,
                   google::longrunning::GetOperationRequest const&));
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_testing
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_TESTING_MOCK_INSTANCE_ADMIN_STUB_H
