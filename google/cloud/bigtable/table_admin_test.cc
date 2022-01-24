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

std::string const kProjectId = "the-project";
std::string const kInstanceId = "the-instance";

class TableAdminTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(*client_, project()).WillRepeatedly(ReturnRef(kProjectId));
  }

  std::shared_ptr<MockAdminClient> client_ =
      std::make_shared<MockAdminClient>();
};

TEST_F(TableAdminTest, ResourceNames) {
  TableAdmin tested(client_, kInstanceId);
  EXPECT_EQ(kProjectId, tested.project());
  EXPECT_EQ(kInstanceId, tested.instance_id());
  EXPECT_EQ(InstanceName(kProjectId, kInstanceId), tested.instance_name());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
