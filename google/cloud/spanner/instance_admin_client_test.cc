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
  auto actual = client.GetInstance("test-project", "test-instance");
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
