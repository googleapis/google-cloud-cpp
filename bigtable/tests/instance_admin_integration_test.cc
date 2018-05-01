// Copyright 2018 Google LLC
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

#include "bigtable/client/instance_admin.h"
#include "bigtable/client/internal/make_unique.h"
#include <gmock/gmock.h>

namespace {
class InstanceTestEnvironment : public ::testing::Environment {
 public:
  explicit InstanceTestEnvironment(std::string project) {
    project_id_ = std::move(project);
  }

  static std::string const& project_id() { return project_id_; }

 private:
  static std::string project_id_;
};

std::string InstanceTestEnvironment::project_id_;

class InstanceAdminIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto instance_admin_client = bigtable::CreateDefaultInstanceAdminClient(
        InstanceTestEnvironment::project_id(), bigtable::ClientOptions());
    instance_admin_ = bigtable::internal::make_unique<bigtable::InstanceAdmin>(
        instance_admin_client);
  }

 protected:
  std::unique_ptr<bigtable::InstanceAdmin> instance_admin_;
};

bool UsingCloudBigtableEmulator() {
  return std::getenv("BIGTABLE_EMULATOR_HOST") != nullptr;
}

bool IsInstancePresent(
    std::vector<::google::bigtable::admin::v2::Instance> instances,
    std::string const& instance_name) {
  bool instance_present = false;
  for (auto const& i : instances) {
    if (instance_name == i.name()) {
      instance_present = true;
      std::cout << "Instance Name " << i.name();
      break;
    }
  }
  return instance_present;
}
}  // namespace

/// @test Verify that InstanceAdmin::ListInstances works as expected.
TEST_F(InstanceAdminIntegrationTest, ListInstancesTest) {
  // The emulator does not support instance operations.
  if (UsingCloudBigtableEmulator()) {
    return;
  }
  auto instances = instance_admin_->ListInstances();
  for (auto const& i : instances) {
    auto const npos = std::string::npos;
    EXPECT_NE(npos, i.name().find(instance_admin_->project_name()));
  }
}

/// @test Verify that InstanceAdmin::DeleteInstances works as expected.
TEST_F(InstanceAdminIntegrationTest, DeleteInstancesTest) {
  // The emulator does not support instance operations.
  if (UsingCloudBigtableEmulator()) {
    return;
  }
  std::string instance_id = "DeleteInstancesTest_instance";
  bigtable::InstanceId instance(instance_id);
  bigtable::DisplayName diplay_name(instance_id);
  EXPECT_FALSE(
      IsInstancePresent(instance_admin_->ListInstances(), instance_id));

  std::vector<std::pair<std::string, bigtable::ClusterConfig>> clusters;
  clusters.push_back(std::make_pair(
      "sample-cluster",
      bigtable::ClusterConfig("sample-cluster", 1,
                              google::bigtable::admin::v2::StorageType::HDD)));
  auto instance_config =
      bigtable::InstanceConfig(instance, diplay_name, clusters);
  instance_admin_->CreateInstance(instance_config);
  EXPECT_TRUE(IsInstancePresent(instance_admin_->ListInstances(), instance_id));

  instance_admin_->DeleteInstance(instance_id);
  EXPECT_FALSE(
      IsInstancePresent(instance_admin_->ListInstances(), instance_id));
}

/// @test Verify that InstanceAdmin::ListClusters works as expected.
TEST_F(InstanceAdminIntegrationTest, ListClustersTest) {
  // The emulator does not support cluster operations.
  if (UsingCloudBigtableEmulator()) {
    return;
  }

  // TODO(#418) - create an instance and test that its cluster is returned here.
  auto clusters = instance_admin_->ListClusters();
  for (auto const& i : clusters) {
    auto const npos = std::string::npos;
    EXPECT_NE(npos, i.name().find(instance_admin_->project_name()));
  }
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    // Show usage if number of arguments is invalid.
    std::cerr << "Usage: " << cmd.substr(last_slash + 1) << " <project_id>"
              << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  (void)::testing::AddGlobalTestEnvironment(
      new InstanceTestEnvironment(project_id));

  return RUN_ALL_TESTS();
}
