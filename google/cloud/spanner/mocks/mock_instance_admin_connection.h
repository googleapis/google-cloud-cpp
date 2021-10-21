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
  MOCK_METHOD(StatusOr<google::spanner::admin::instance::v1::Instance>,
              GetInstance, (GetInstanceParams), (override));
  MOCK_METHOD(future<StatusOr<google::spanner::admin::instance::v1::Instance>>,
              CreateInstance, (CreateInstanceParams), (override));
  MOCK_METHOD(future<StatusOr<google::spanner::admin::instance::v1::Instance>>,
              UpdateInstance, (UpdateInstanceParams), (override));
  MOCK_METHOD(Status, DeleteInstance, (DeleteInstanceParams), (override));
  MOCK_METHOD(StatusOr<google::spanner::admin::instance::v1::InstanceConfig>,
              GetInstanceConfig, (GetInstanceConfigParams), (override));
  MOCK_METHOD(spanner::ListInstanceConfigsRange, ListInstanceConfigs,
              (ListInstanceConfigsParams), (override));
  MOCK_METHOD(spanner::ListInstancesRange, ListInstances, (ListInstancesParams),
              (override));
  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, GetIamPolicy,
              (GetIamPolicyParams), (override));
  MOCK_METHOD(StatusOr<google::iam::v1::Policy>, SetIamPolicy,
              (SetIamPolicyParams), (override));
  MOCK_METHOD(StatusOr<google::iam::v1::TestIamPermissionsResponse>,
              TestIamPermissions, (TestIamPermissionsParams), (override));
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_mocks
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_MOCKS_MOCK_INSTANCE_ADMIN_CONNECTION_H
