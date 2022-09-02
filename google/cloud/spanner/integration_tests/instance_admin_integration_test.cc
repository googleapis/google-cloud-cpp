// Copyright 2019 Google LLC
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

// TODO(#7356): Remove this file after the deprecation period expires
#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/spanner/create_instance_request_builder.h"
#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/testing/cleanup_stale_instances.h"
#include "google/cloud/spanner/testing/pick_instance_config.h"
#include "google/cloud/spanner/testing/random_instance_name.h"
#include "google/cloud/spanner/update_instance_request_builder.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/strings/match.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::UnorderedElementsAre;

std::string const& ProjectId() {
  static std::string project_id =
      internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  return project_id;
}

std::string const& InstanceId() {
  static std::string instance_id =
      internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_TEST_INSTANCE_ID")
          .value_or("");
  return instance_id;
}

bool RunSlowInstanceTests() {
  static bool run_slow_instance_tests = absl::StrContains(
      internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS")
          .value_or(""),
      "instance");
  return run_slow_instance_tests;
}

bool Emulator() {
  static bool emulator = internal::GetEnv("SPANNER_EMULATOR_HOST").has_value();
  return emulator;
}

class CleanupStaleInstances : public ::testing::Environment {
 public:
  void SetUp() override {
    EXPECT_STATUS_OK(
        spanner_testing::CleanupStaleInstances(Project(ProjectId())));
  }
};

::testing::Environment* const kCleanupEnv =
    ::testing::AddGlobalTestEnvironment(new CleanupStaleInstances);

class InstanceAdminClientTest
    : public ::google::cloud::testing_util::IntegrationTest {
 public:
  InstanceAdminClientTest()
      : generator_(internal::MakeDefaultPRNG()),
        client_(MakeInstanceAdminConnection()) {
    static_cast<void>(kCleanupEnv);
  }

 protected:
  void SetUp() override {
    if (Emulator()) {
      // We expect test instances to exist when running against real services,
      // but if we are running against the emulator we're happy to create one.
      Instance in(ProjectId(), InstanceId());
      auto create_instance_request =
          CreateInstanceRequestBuilder(in,
                                       "projects/" + in.project_id() +
                                           "/instanceConfigs/emulator-config")
              .Build();
      auto instance = client_.CreateInstance(create_instance_request).get();
      if (!instance) {
        ASSERT_THAT(instance, StatusIs(StatusCode::kAlreadyExists));
      }
    }
  }

  internal::DefaultPRNG generator_;
  InstanceAdminClient client_;
};

/// @test Verify the basic read operations for instances work.
TEST_F(InstanceAdminClientTest, InstanceReadOperations) {
  Instance in(ProjectId(), InstanceId());
  ASSERT_FALSE(in.project_id().empty());
  ASSERT_FALSE(in.instance_id().empty());

  auto instance = client_.GetInstance(in);
  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(instance->name(), in.FullName());
  EXPECT_NE(instance->node_count(), 0);

  auto instance_names = [&in, this]() mutable {
    std::vector<std::string> names;
    for (auto const& instance : client_.ListInstances(in.project_id(), "")) {
      EXPECT_STATUS_OK(instance);
      if (!instance) break;
      names.push_back(instance->name());
    }
    return names;
  }();
  EXPECT_EQ(1, std::count(instance_names.begin(), instance_names.end(),
                          instance->name()));
}

/// @test Verify the basic CRUD operations for instances work.
TEST_F(InstanceAdminClientTest, InstanceCRUDOperations) {
  if (!Emulator() && !RunSlowInstanceTests()) {
    GTEST_SKIP() << "skipping slow instance tests; set "
                 << "GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS=instance"
                 << " to override";
  }

  std::string instance_id = spanner_testing::RandomInstanceName(generator_);
  Instance in(ProjectId(), instance_id);
  ASSERT_FALSE(in.project_id().empty());
  ASSERT_FALSE(in.instance_id().empty());

  auto config_name = spanner_testing::PickInstanceConfig(
      in.project(), generator_,
      [](google::spanner::admin::instance::v1::InstanceConfig const& config) {
        return absl::StrContains(config.name(), "/regional-us-west");
      });
  ASSERT_FALSE(config_name.empty()) << "could not get an instance config";

  auto instance =
      client_
          .CreateInstance(CreateInstanceRequestBuilder(in, config_name)
                              .SetDisplayName("test-display-name")
                              .SetNodeCount(1)
                              .SetLabels({{"label-key", "label-value"}})
                              .Build())
          .get();

  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(instance->name(), in.FullName());
  EXPECT_EQ(instance->display_name(), "test-display-name");
  EXPECT_NE(instance->node_count(), 0);
  EXPECT_EQ(instance->config(), config_name);
  EXPECT_EQ(instance->labels().at("label-key"), "label-value");

  // Then update the instance
  instance = client_
                 .UpdateInstance(UpdateInstanceRequestBuilder(*instance)
                                     .SetDisplayName("New display name")
                                     .AddLabels({{"new-key", "new-value"}})
                                     .SetNodeCount(2)
                                     .Build())
                 .get();
  if (!Emulator() || instance) {
    EXPECT_STATUS_OK(instance);
    if (instance) {
      EXPECT_EQ(instance->display_name(), "New display name");
      EXPECT_EQ(instance->labels_size(), 2);
      EXPECT_EQ(instance->labels().at("new-key"), "new-value");
      EXPECT_EQ(instance->node_count(), 2);
    }
  }

  EXPECT_STATUS_OK(client_.DeleteInstance(in));
}

TEST_F(InstanceAdminClientTest, InstanceConfig) {
  auto project_id = ProjectId();
  ASSERT_FALSE(project_id.empty());

  auto config_names = [&project_id, this]() mutable {
    std::vector<std::string> names;
    for (auto const& config : client_.ListInstanceConfigs(project_id)) {
      EXPECT_STATUS_OK(config);
      if (!config) break;
      names.push_back(config->name());
    }
    return names;
  }();
  ASSERT_FALSE(config_names.empty());

  // Use the name of the first element from the list of instance configs.
  auto config = client_.GetInstanceConfig(config_names[0]);
  ASSERT_STATUS_OK(config);
  EXPECT_THAT(config->name(), HasSubstr(project_id));
  EXPECT_EQ(
      1, std::count(config_names.begin(), config_names.end(), config->name()));
}

TEST_F(InstanceAdminClientTest, InstanceIam) {
  Instance in(ProjectId(), InstanceId());
  ASSERT_FALSE(in.project_id().empty());
  ASSERT_FALSE(in.instance_id().empty());

  auto actual_policy = client_.GetIamPolicy(in);
  if (Emulator() &&
      actual_policy.status().code() == StatusCode::kUnimplemented) {
    GTEST_SKIP() << "emulator does not support IAM policies";
  }
  ASSERT_STATUS_OK(actual_policy);
  EXPECT_FALSE(actual_policy->etag().empty());

  if (RunSlowInstanceTests()) {
    // Set the policy to the existing value of the policy. While this
    // changes nothing, it tests all the code in the client library.
    auto updated_policy = client_.SetIamPolicy(in, *actual_policy);
    ASSERT_THAT(updated_policy, AnyOf(IsOk(), StatusIs(StatusCode::kAborted)));
    if (updated_policy) {
      EXPECT_THAT(updated_policy->etag(), Not(IsEmpty()));
    }

    // Repeat the test using the OCC API.
    updated_policy =
        client_.SetIamPolicy(in, [](google::iam::v1::Policy p) { return p; });
    ASSERT_STATUS_OK(updated_policy);
    EXPECT_THAT(updated_policy->etag(), Not(IsEmpty()));
  }

  auto actual = client_.TestIamPermissions(
      in, {"spanner.databases.list", "spanner.databases.get"});
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(
      actual->permissions(),
      UnorderedElementsAre("spanner.databases.list", "spanner.databases.get"));
}

/// @test Verify the backwards compatibility `v1` namespace still exists.
TEST_F(InstanceAdminClientTest, BackwardsCompatibility) {
  auto connection = ::google::cloud::spanner::v1::MakeInstanceAdminConnection();
  EXPECT_THAT(connection, NotNull());
  ASSERT_NO_FATAL_FAILURE(InstanceAdminClient(std::move(connection)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
