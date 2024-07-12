// Copyright 2023 Google LLC
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

#include "google/cloud/spanner/admin/instance_admin_client.h"
#include "google/cloud/spanner/admin/internal/database_admin_option_defaults.h"
#include "google/cloud/spanner/admin/internal/database_admin_rest_connection_impl.h"
#include "google/cloud/spanner/admin/internal/database_admin_rest_stub_factory.h"
#include "google/cloud/spanner/admin/internal/instance_admin_option_defaults.h"
#include "google/cloud/spanner/admin/internal/instance_admin_rest_connection_impl.h"
#include "google/cloud/spanner/admin/internal/instance_admin_rest_stub_factory.h"
#include "google/cloud/spanner/create_instance_request_builder.h"
#include "google/cloud/spanner/testing/cleanup_stale_instances.h"
#include "google/cloud/spanner/testing/pick_instance_config.h"
#include "google/cloud/spanner/testing/random_instance_name.h"
#include "google/cloud/spanner/update_instance_request_builder.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/internal/rest_background_threads_impl.h"
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

using ::google::cloud::testing_util::StatusIs;
using ::testing::Eq;

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
  static bool emulator =
      internal::GetEnv("SPANNER_EMULATOR_REST_HOST").has_value();
  return emulator;
}

// We need to override stub creation to use an alternate emulator endpoint for
// the InstanceAdmin.
std::shared_ptr<google::cloud::spanner_admin::InstanceAdminConnection>
MakeInstanceAdminConnectionRestEmulator() {
  auto options = spanner_admin_internal::InstanceAdminDefaultOptions(Options{});
  if (internal::GetEnv("SPANNER_EMULATOR_REST_HOST").has_value()) {
    options.set<EndpointOption>(
        internal::GetEnv("SPANNER_EMULATOR_REST_HOST").value());
    options.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
  }
  auto background = std::make_unique<
      rest_internal::AutomaticallyCreatedRestBackgroundThreads>();
  auto stub =
      spanner_admin_internal::CreateDefaultInstanceAdminRestStub(options);
  return std::make_shared<
      spanner_admin_internal::InstanceAdminRestConnectionImpl>(
      std::move(background), std::move(stub), std::move(options));
}

// We need to override stub creation to use an alternate emulator endpoint for
// the DatabaseAdmin.
std::shared_ptr<google::cloud::spanner_admin::DatabaseAdminConnection>
MakeDatabaseAdminConnectionRestEmulator() {
  auto options = spanner_admin_internal::DatabaseAdminDefaultOptions(Options{});
  if (internal::GetEnv("SPANNER_EMULATOR_REST_HOST").has_value()) {
    options.set<EndpointOption>(
        internal::GetEnv("SPANNER_EMULATOR_REST_HOST").value());
    options.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
  }
  auto background = std::make_unique<
      rest_internal::AutomaticallyCreatedRestBackgroundThreads>();
  auto stub =
      spanner_admin_internal::CreateDefaultDatabaseAdminRestStub(options);
  return std::make_shared<
      spanner_admin_internal::DatabaseAdminRestConnectionImpl>(
      std::move(background), std::move(stub), std::move(options));
}

class CleanupStaleInstances : public ::testing::Environment {
 public:
  void SetUp() override {
    spanner_admin::InstanceAdminClient instance_admin_client(
        MakeInstanceAdminConnectionRestEmulator());
    spanner_admin::DatabaseAdminClient database_admin_client(
        MakeDatabaseAdminConnectionRestEmulator());
    EXPECT_STATUS_OK(spanner_testing::CleanupStaleInstances(
        Project(ProjectId()), std::move(instance_admin_client),
        std::move(database_admin_client)));
  }
};

class CleanupStaleInstanceConfigs : public ::testing::Environment {
 public:
  void SetUp() override {
    spanner_admin::InstanceAdminClient instance_admin_client(
        MakeInstanceAdminConnectionRestEmulator());
    EXPECT_STATUS_OK(spanner_testing::CleanupStaleInstanceConfigs(
        Project(ProjectId()), std::move(instance_admin_client)));
  }
};

// Cleanup stale instances before instance configs.
::testing::Environment* const kCleanupStaleInstancesEnv =
    ::testing::AddGlobalTestEnvironment(new CleanupStaleInstances);
::testing::Environment* const kCleanupStaleInstanceConfigsEnv =
    ::testing::AddGlobalTestEnvironment(new CleanupStaleInstanceConfigs);

class InstanceAdminClientRestTest
    : public ::google::cloud::testing_util::IntegrationTest {
 public:
  InstanceAdminClientRestTest()
      : generator_(internal::MakeDefaultPRNG()),
        client_(MakeInstanceAdminConnectionRestEmulator()) {
    static_cast<void>(kCleanupStaleInstancesEnv);
    static_cast<void>(kCleanupStaleInstanceConfigsEnv);
  }

 protected:
  void SetUp() override {
    if (Emulator()) {
      // We expect test instances to exist when running against real services,
      // but if we are running against the emulator we're happy to create one.
      Instance in(ProjectId(), InstanceId());

      auto create_instance_request =
          CreateInstanceRequestBuilder(
              in, in.project().FullName() + "/instanceConfigs/emulator-config")
              .Build();
      auto instance = client_.CreateInstance(create_instance_request).get();
      if (!instance) {
        ASSERT_THAT(instance,
                    ::testing::AnyOf(StatusIs(StatusCode::kAlreadyExists),
                                     StatusIs(StatusCode::kAborted)));
      }
    }
  }

  internal::DefaultPRNG generator_;
  spanner_admin::InstanceAdminClient client_;
};

/// @test Verify the basic read operations for instances work.
TEST_F(InstanceAdminClientRestTest, InstanceReadOperations) {
  Instance in(ProjectId(), InstanceId());
  ASSERT_FALSE(in.project_id().empty());
  ASSERT_FALSE(in.instance_id().empty());

  auto instance = client_.GetInstance(in.FullName());
  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(instance->name(), in.FullName());
  EXPECT_NE(instance->node_count(), 0);

  auto instance_names = [&in, this]() mutable {
    std::vector<std::string> names;
    auto const parent = in.project().FullName();
    for (auto const& instance : client_.ListInstances(parent)) {
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
TEST_F(InstanceAdminClientRestTest, InstanceCRUDOperations) {
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
  if (Emulator()) {
    EXPECT_THAT(instance, StatusIs(StatusCode::kInternal));
  } else {
    EXPECT_STATUS_OK(instance);
    if (instance) {
      EXPECT_EQ(instance->display_name(), "New display name");
      EXPECT_EQ(instance->labels_size(), 2);
      EXPECT_EQ(instance->labels().at("new-key"), "new-value");
      EXPECT_EQ(instance->node_count(), 2);
    }
  }

  EXPECT_STATUS_OK(client_.DeleteInstance(in.FullName()));
}

TEST_F(InstanceAdminClientRestTest, CreateInstanceStartAwait) {
  if (!Emulator() && !RunSlowInstanceTests()) {
    GTEST_SKIP() << "skipping slow instance tests; set "
                 << "GOOGLE_CLOUD_CPP_SPANNER_SLOW_INTEGRATION_TESTS=instance"
                 << " to override";
  }

  Instance in(ProjectId(), spanner_testing::RandomInstanceName(generator_));

  auto config_name = spanner_testing::PickInstanceConfig(
      in.project(), generator_,
      [](google::spanner::admin::instance::v1::InstanceConfig const& config) {
        return absl::StrContains(config.name(), "/regional-us-west");
      });
  ASSERT_FALSE(config_name.empty()) << "could not get an instance config";

  auto operation =
      client_.CreateInstance(ExperimentalTag{}, NoAwaitTag{},
                             CreateInstanceRequestBuilder(in, config_name)
                                 .SetDisplayName("test-display-name")
                                 .SetNodeCount(1)
                                 .SetLabels({{"label-key", "label-value"}})
                                 .Build());
  ASSERT_STATUS_OK(operation);
  // Verify that an error is returned if there is a mismatch between the RPC
  // that returned the operation and the RPC in which is it used.
  auto instance_config =
      client_.CreateInstanceConfig(ExperimentalTag{}, *operation).get();
  EXPECT_THAT(instance_config, StatusIs(StatusCode::kInvalidArgument));

  auto instance = client_.CreateInstance(ExperimentalTag{}, *operation).get();
  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(instance->name(), in.FullName());
  EXPECT_EQ(instance->display_name(), "test-display-name");
  EXPECT_STATUS_OK(client_.DeleteInstance(in.FullName()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
