// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/admin/bigtable_table_admin_client.h"
#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_admin {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class AdminIAMPolicyIntegrationTest
    : public bigtable::testing::TableIntegrationTest {
 protected:
  void SetUp() override {
    TableIntegrationTest::SetUp();

    service_account_ = google::cloud::internal::GetEnv(
                           "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT")
                           .value_or("");
    ASSERT_FALSE(service_account_.empty());
  }

  std::string service_account_;
  BigtableTableAdminClient client_ =
      BigtableTableAdminClient(MakeBigtableTableAdminConnection());
};

/// @test Verify that IAM Policy APIs work as expected.
TEST_F(AdminIAMPolicyIntegrationTest, SetGetTestIamAPIsTest) {
  auto const instance_name =
      bigtable::InstanceName(project_id(), instance_id());
  auto const table_name =
      bigtable::TableName(project_id(), instance_id(),
                          bigtable::testing::TableTestEnvironment::table_id());

  auto iam_policy = bigtable::IamPolicy({bigtable::IamBinding(
      "roles/bigtable.reader", {"serviceAccount:" + service_account_})});

  auto initial_policy = client_.SetIamPolicy(table_name, iam_policy);
  ASSERT_STATUS_OK(initial_policy);

  auto fetched_policy = client_.GetIamPolicy(table_name);
  ASSERT_STATUS_OK(fetched_policy);

  EXPECT_EQ(initial_policy->version(), fetched_policy->version());
  EXPECT_EQ(initial_policy->etag(), fetched_policy->etag());

  auto permission_set = client_.TestIamPermissions(
      table_name, {"bigtable.tables.get", "bigtable.tables.readRows"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->permissions_size());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_admin
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::testing::InitGoogleMock(&argc, argv);
  (void)::testing::AddGlobalTestEnvironment(
      new google::cloud::bigtable::testing::TableTestEnvironment);
  return RUN_ALL_TESTS();
}
