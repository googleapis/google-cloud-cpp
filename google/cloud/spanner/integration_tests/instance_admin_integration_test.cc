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

#include "google/cloud/spanner/create_instance_request_builder.h"
#include "google/cloud/spanner/instance_admin_client.h"
#include "google/cloud/spanner/testing/random_instance_name.h"
#include "google/cloud/spanner/update_instance_request_builder.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>
#include <chrono>
#include <ctime>
#include <regex>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;

class InstanceAdminClientTest : public testing::Test {
 public:
  InstanceAdminClientTest() : client_(MakeInstanceAdminConnection()) {}

 protected:
  void SetUp() override {
    emulator_ =
        google::cloud::internal::GetEnv("SPANNER_EMULATOR_HOST").has_value();
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    instance_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_INSTANCE")
            .value_or("");
    run_slow_integration_tests_ =
        google::cloud::internal::GetEnv("RUN_SLOW_INTEGRATION_TESTS")
            .value_or("");
    test_iam_service_account_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_SPANNER_IAM_TEST_SA")
            .value_or("");
  }
  InstanceAdminClient client_;
  bool emulator_;
  std::string project_id_;
  std::string instance_id_;
  std::string run_slow_integration_tests_;
  std::string test_iam_service_account_;
};

class InstanceAdminClientTestWithCleanup : public InstanceAdminClientTest {
 protected:
  void SetUp() override {
    InstanceAdminClientTest::SetUp();
    instance_name_regex_ = std::regex(
        R"(projects/.+/instances/(temporary-instance-(\d{4}-\d{2}-\d{2})-.+))");
    if (run_slow_integration_tests_ != "yes") {
      return;
    }
    // Deletes leaked temporary instances.
    std::vector<std::string> instance_ids = [this]() mutable {
      std::vector<std::string> instance_ids;
      for (auto const& instance : client_.ListInstances(project_id_, "")) {
        EXPECT_STATUS_OK(instance);
        if (!instance) break;
        auto name = instance->name();
        std::smatch m;
        if (std::regex_match(name, m, instance_name_regex_)) {
          auto instance_id = m[1];
          auto date_str = m[2];
          std::string cut_off_date = "1973-03-01";
          auto cut_off_time_t = std::chrono::system_clock::to_time_t(
              std::chrono::system_clock::now() - std::chrono::hours(48));
          std::strftime(&cut_off_date[0], cut_off_date.size() + 1, "%Y-%m-%d",
                        std::localtime(&cut_off_time_t));
          // Compare the strings
          if (date_str < cut_off_date) {
            instance_ids.push_back(instance_id);
          }
        }
      }
      return instance_ids;
    }();
    // Let it fail if we have too many leaks.
    EXPECT_GT(20, instance_ids.size());
    for (auto const& id_to_delete : instance_ids) {
      // Probably better to ignore failures.
      client_.DeleteInstance(Instance(project_id_, id_to_delete));
    }
  }
  std::regex instance_name_regex_;
};

/// @test Verify the basic read operations for instances work.
TEST_F(InstanceAdminClientTest, InstanceReadOperations) {
  ASSERT_FALSE(project_id_.empty());
  ASSERT_FALSE(instance_id_.empty());

  Instance in(project_id_, instance_id_);

  auto instance = client_.GetInstance(in);
  if (emulator_ && instance.status().code() == StatusCode::kNotFound) return;
  EXPECT_STATUS_OK(instance);
  EXPECT_THAT(instance->name(), HasSubstr(project_id_));
  EXPECT_THAT(instance->name(), HasSubstr(instance_id_));
  EXPECT_NE(0, instance->node_count());

  std::vector<std::string> instance_names = [this]() mutable {
    std::vector<std::string> names;
    for (auto const& instance : client_.ListInstances(project_id_, "")) {
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
TEST_F(InstanceAdminClientTestWithCleanup, InstanceCRUDOperations) {
  if (run_slow_integration_tests_ != "yes") {
    GTEST_SKIP();
  }
  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::string instance_id =
      google::cloud::spanner_testing::RandomInstanceName(generator);
  ASSERT_FALSE(project_id_.empty());
  ASSERT_FALSE(instance_id.empty());
  Instance in(project_id_, instance_id);

  // Make sure we're using correct regex.
  std::smatch m;
  auto full_name = in.FullName();
  EXPECT_TRUE(std::regex_match(full_name, m, instance_name_regex_));
  std::vector<std::string> instance_config_names = [this]() mutable {
    std::vector<std::string> names;
    for (auto const& instance_config :
         client_.ListInstanceConfigs(project_id_)) {
      EXPECT_STATUS_OK(instance_config);
      if (!instance_config) break;
      names.push_back(instance_config->name());
    }
    return names;
  }();
  ASSERT_FALSE(instance_config_names.empty());
  // Use the name of the first element from the list of instance configs.
  auto instance_config = instance_config_names[0];

  future<StatusOr<google::spanner::admin::instance::v1::Instance>> f =
      client_.CreateInstance(CreateInstanceRequestBuilder(in, instance_config)
                                 .SetDisplayName("test-display-name")
                                 .SetNodeCount(1)
                                 .SetLabels({{"label-key", "label-value"}})
                                 .Build());
  StatusOr<google::spanner::admin::instance::v1::Instance> instance = f.get();

  EXPECT_STATUS_OK(instance);
  EXPECT_THAT(instance->name(), HasSubstr(project_id_));
  EXPECT_THAT(instance->name(), HasSubstr(instance_id));
  EXPECT_EQ("test-display-name", instance->display_name());
  EXPECT_NE(0, instance->node_count());
  EXPECT_EQ(instance_config, instance->config());
  if (!emulator_ || instance->labels_size() != 0) {
    EXPECT_EQ("label-value", instance->labels().at("label-key"));
  }

  // Then update the instance
  f = client_.UpdateInstance(UpdateInstanceRequestBuilder(*instance)
                                 .SetDisplayName("New display name")
                                 .AddLabels({{"new-key", "new-value"}})
                                 .SetNodeCount(2)
                                 .Build());
  instance = f.get();
  if (!emulator_ || instance) {
    EXPECT_STATUS_OK(instance);
    EXPECT_EQ("New display name", instance->display_name());
    EXPECT_EQ(2, instance->labels_size());
    EXPECT_EQ("new-value", instance->labels().at("new-key"));
    EXPECT_EQ(2, instance->node_count());
  }
  auto status = client_.DeleteInstance(in);
  EXPECT_STATUS_OK(status);
}

TEST_F(InstanceAdminClientTest, InstanceConfig) {
  ASSERT_FALSE(project_id_.empty());

  std::vector<std::string> instance_config_names = [this]() mutable {
    std::vector<std::string> names;
    for (auto const& instance_config :
         client_.ListInstanceConfigs(project_id_)) {
      EXPECT_STATUS_OK(instance_config);
      if (!instance_config) break;
      names.push_back(instance_config->name());
    }
    return names;
  }();
  ASSERT_FALSE(instance_config_names.empty());
  // Use the name of the first element from the list of instance configs.
  auto instance_config = client_.GetInstanceConfig(instance_config_names[0]);
  EXPECT_STATUS_OK(instance_config);
  EXPECT_THAT(instance_config->name(), HasSubstr(project_id_));
  EXPECT_EQ(
      1, std::count(instance_config_names.begin(), instance_config_names.end(),
                    instance_config->name()));
}

TEST_F(InstanceAdminClientTest, InstanceIam) {
  if (emulator_) return;

  ASSERT_FALSE(project_id_.empty());
  ASSERT_FALSE(instance_id_.empty());
  ASSERT_TRUE(emulator_ || !test_iam_service_account_.empty());

  Instance in(project_id_, instance_id_);

  auto actual_policy = client_.GetIamPolicy(in);
  ASSERT_STATUS_OK(actual_policy);
  EXPECT_FALSE(actual_policy->etag().empty());

  if (run_slow_integration_tests_ == "yes") {
    // Set the policy to the existing value of the policy. While this changes
    // nothing it tests all the code in the client library.
    auto updated_policy = client_.SetIamPolicy(in, *actual_policy);
    ASSERT_STATUS_OK(updated_policy);
    EXPECT_FALSE(actual_policy->etag().empty());

    // Repeat the test using the OCC API.
    updated_policy =
        client_.SetIamPolicy(in, [](google::iam::v1::Policy p) { return p; });
    ASSERT_STATUS_OK(updated_policy);
    EXPECT_FALSE(actual_policy->etag().empty());
  }

  auto actual = client_.TestIamPermissions(
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
