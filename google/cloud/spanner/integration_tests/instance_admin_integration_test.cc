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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;

/// @test Verify the basic CRUD operations for instances work.
TEST(InstanceAdminClient, InstanceBasicCRUD) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  auto instance_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
          .value_or("");
  ASSERT_FALSE(project_id.empty());
  ASSERT_FALSE(instance_id.empty());

  Instance in(project_id, instance_id);

  InstanceAdminClient client(MakeInstanceAdminConnection());
  auto instance = client.GetInstance(in);
  EXPECT_STATUS_OK(instance);
  EXPECT_THAT(instance->name(), HasSubstr(project_id));
  EXPECT_THAT(instance->name(), HasSubstr(instance_id));
  EXPECT_NE(0, instance->node_count());

  std::vector<std::string> instance_names = [client, project_id]() mutable {
    std::vector<std::string> names;
    for (auto instance : client.ListInstances(project_id, "")) {
      EXPECT_STATUS_OK(instance);
      if (!instance) break;
      names.push_back(instance->name());
    }
    return names;
  }();

  EXPECT_EQ(1, std::count(instance_names.begin(), instance_names.end(),
                          instance->name()));
}

TEST(InstanceAdminClient, InstanceIam) {
  auto project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  auto instance_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
          .value_or("");
  ASSERT_FALSE(project_id.empty());
  ASSERT_FALSE(instance_id.empty());

  Instance in(project_id, instance_id);

  InstanceAdminClient client(MakeInstanceAdminConnection());
  auto actual = client.TestIamPermissions(
      in, {"spanner.databases.list", "spanner.databases.get"});
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(
      actual->permissions(),
      UnorderedElementsAre("spanner.databases.list", "spanner.databases.get"));
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
