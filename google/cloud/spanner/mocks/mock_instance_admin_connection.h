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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_MOCK_INSTANCE_ADMIN_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_MOCK_INSTANCE_ADMIN_CONNECTION_H

#include "google/cloud/spanner/instance_admin_connection.h"
#include "google/cloud/spanner/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner_mocks {
inline namespace SPANNER_CLIENT_NS {
/**
 * A class to mock `google::cloud::spanner::InstanceAdminConnection`.
 *
 * Application developers may want to test their code with simulated responses,
 * including errors from a `spanner::InstanceAdminClient`. To do so, construct a
 * `spanner::InstanceAdminClient` with an instance of this class. Then use the
 * Google Test framework functions to program the behavior of this mock.
 */
class MockInstanceAdminConnection
    : public google::cloud::spanner::InstanceAdminConnection {
 public:
  MOCK_METHOD1(GetInstance,
               StatusOr<google::spanner::admin::instance::v1::Instance>(
                   GetInstanceParams));
  MOCK_METHOD1(CreateInstance,
               future<StatusOr<google::spanner::admin::instance::v1::Instance>>(
                   CreateInstanceParams));
  MOCK_METHOD1(UpdateInstance,
               future<StatusOr<google::spanner::admin::instance::v1::Instance>>(
                   UpdateInstanceParams));
  MOCK_METHOD1(DeleteInstance, Status(DeleteInstanceParams));
  MOCK_METHOD1(GetInstanceConfig,
               StatusOr<google::spanner::admin::instance::v1::InstanceConfig>(
                   GetInstanceConfigParams));
  MOCK_METHOD1(ListInstanceConfigs,
               spanner::ListInstanceConfigsRange(ListInstanceConfigsParams));
  MOCK_METHOD1(ListInstances, spanner::ListInstancesRange(ListInstancesParams));
  MOCK_METHOD1(GetIamPolicy,
               StatusOr<google::iam::v1::Policy>(GetIamPolicyParams));
  MOCK_METHOD1(SetIamPolicy,
               StatusOr<google::iam::v1::Policy>(SetIamPolicyParams));
  MOCK_METHOD1(TestIamPermissions,
               StatusOr<google::iam::v1::TestIamPermissionsResponse>(
                   TestIamPermissionsParams));
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_MOCK_INSTANCE_ADMIN_CONNECTION_H
