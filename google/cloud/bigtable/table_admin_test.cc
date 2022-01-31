// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/testing/mock_admin_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ReturnRef;

using MockAdminClient = ::google::cloud::bigtable::testing::MockAdminClient;

auto const* const kProjectId = "the-project";
auto const* const kInstanceId = "the-instance";
auto const* const kInstanceName = "projects/the-project/instances/the-instance";

class TableAdminTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(project_id_));
  }

  std::string project_id_ = kProjectId;
  std::shared_ptr<MockAdminClient> client_ =
      std::make_shared<MockAdminClient>();
};

TEST_F(TableAdminTest, ResourceNames) {
  TableAdmin tested(client_, kInstanceId);
  EXPECT_EQ(kProjectId, tested.project());
  EXPECT_EQ(kInstanceId, tested.instance_id());
  EXPECT_EQ(kInstanceName, tested.instance_name());
}

TEST_F(TableAdminTest, WithNewTarget) {
  auto admin = TableAdmin(client_, kInstanceId);
  auto other_admin = admin.WithNewTarget("other-project", "other-instance");
  EXPECT_EQ(other_admin.project(), "other-project");
  EXPECT_EQ(other_admin.instance_id(), "other-instance");
  EXPECT_EQ(other_admin.instance_name(),
            InstanceName("other-project", "other-instance"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
