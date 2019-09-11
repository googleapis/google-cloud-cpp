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

#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/mocks/mock_instance_admin_connection.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using spanner_mocks::MockInstanceAdminConnection;
using ::testing::_;
using ::testing::ElementsAre;
namespace gcsa = google::spanner::admin::instance::v1;

TEST(InstanceAdminClientTest, CopyAndMove) {
  auto conn1 = std::make_shared<MockInstanceAdminConnection>();
  auto conn2 = std::make_shared<MockInstanceAdminConnection>();

  InstanceAdminClient c1(conn1);
  InstanceAdminClient c2(conn2);
  EXPECT_NE(c1, c2);

  // Copy construction
  InstanceAdminClient c3 = c1;
  EXPECT_EQ(c3, c1);

  // Copy assignment
  c3 = c2;
  EXPECT_EQ(c3, c2);

  // Move construction
  InstanceAdminClient c4 = std::move(c3);
  EXPECT_EQ(c4, c2);

  // Move assignment
  c1 = std::move(c4);
  EXPECT_EQ(c1, c2);
}

TEST(InstanceAdminClientTest, GetInstance) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, GetInstance(_))
      .WillOnce([](InstanceAdminConnection::GetInstanceParams const& p) {
        EXPECT_EQ("projects/test-project/instances/test-instance",
                  p.instance_name);
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  InstanceAdminClient client(mock);
  auto actual = client.GetInstance(Instance("test-project", "test-instance"));
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
}

TEST(InstanceAdminClientTest, ListInstances) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();

  EXPECT_CALL(*mock, ListInstances(_))
      .WillOnce([](InstanceAdminConnection::ListInstancesParams const& p) {
        EXPECT_EQ("test-project", p.project_id);
        EXPECT_EQ("labels.test-key:test-value", p.filter);

        return ListInstancesRange(
            gcsa::ListInstancesRequest{},
            [](gcsa::ListInstancesRequest const&) {
              return StatusOr<gcsa::ListInstancesResponse>(
                  Status(StatusCode::kPermissionDenied, "uh-oh"));
            },
            [](gcsa::ListInstancesResponse const&) {
              return std::vector<gcsa::Instance>{};
            });
      });

  InstanceAdminClient client(mock);
  auto range =
      client.ListInstances("test-project", "labels.test-key:test-value");
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  EXPECT_EQ(StatusCode::kPermissionDenied, begin->status().code());
}

TEST(InstanceAdminClientTest, GetIamPolicy) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, GetIamPolicy(_))
      .WillOnce([](InstanceAdminConnection::GetIamPolicyParams const& p) {
        EXPECT_EQ("projects/test-project/instances/test-instance",
                  p.instance_name);
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  InstanceAdminClient client(mock);
  auto actual = client.GetIamPolicy(Instance("test-project", "test-instance"));
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
}

TEST(InstanceAdminClientTest, TestIamPermissions) {
  auto mock = std::make_shared<MockInstanceAdminConnection>();
  EXPECT_CALL(*mock, TestIamPermissions(_))
      .WillOnce([](InstanceAdminConnection::TestIamPermissionsParams const& p) {
        EXPECT_EQ("projects/test-project/instances/test-instance",
                  p.instance_name);
        EXPECT_THAT(p.permissions,
                    ElementsAre("test.permission1", "test.permission2"));
        return Status(StatusCode::kPermissionDenied, "uh-oh");
      });

  InstanceAdminClient client(mock);
  auto actual =
      client.TestIamPermissions(Instance("test-project", "test-instance"),
                                {"test.permission1", "test.permission2"});
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
