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

#include "bigtable/client/grpc_error.h"
#include "bigtable/client/instance_admin.h"
#include "bigtable/client/internal/make_unique.h"
#include "bigtable/client/testing/random.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace btadmin = google::bigtable::admin::v2;

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
  bigtable::testing::DefaultPRNG generator_ =
      bigtable::testing::MakeDefaultPRNG();
};

bool UsingCloudBigtableEmulator() {
  return std::getenv("BIGTABLE_EMULATOR_HOST") != nullptr;
}

bool IsInstancePresent(std::vector<btadmin::Instance> const& instances,
                       std::string const& instance_name) {
  return instances.end() !=
         std::find_if(instances.begin(), instances.end(),
                      [&instance_name](btadmin::Instance const& i) {
                        return i.name() == instance_name;
                      });
}

bigtable::InstanceConfig IntegrationTestConfig(std::string const& id) {
  bigtable::InstanceId instance_id(id);
  bigtable::DisplayName display_name("Integration Tests " + id);
  auto cluster_config =
      bigtable::ClusterConfig("us-central1-f", 0, bigtable::ClusterConfig::HDD);
  bigtable::InstanceConfig config(instance_id, display_name,
                                  {{id + "-c1", cluster_config}});
  config.set_type(bigtable::InstanceConfig::DEVELOPMENT);
  return config;
}

}  // anonymous namespace

/// @test Verify that InstanceAdmin::CreateInstance works as expected.
TEST_F(InstanceAdminIntegrationTest, CreateInstanceTest) {
  std::string instance_id =
      "it-" + bigtable::testing::Sample(generator_, 8,
                                        "abcdefghijklmnopqrstuvwxyz0123456789");
  auto config = IntegrationTestConfig(instance_id);

  auto instances_before = instance_admin_->ListInstances();
  auto instance = instance_admin_->CreateInstance(config).get();
  auto instances_after = instance_admin_->ListInstances();
  instance_admin_->DeleteInstance(instance_id);
  EXPECT_FALSE(IsInstancePresent(instances_before, instance.name()));
  EXPECT_TRUE(IsInstancePresent(instances_after, instance.name()));
  EXPECT_NE(std::string::npos, instance.name().find(instance_id));
  EXPECT_NE(std::string::npos,
            instance.name().find(InstanceTestEnvironment::project_id()));
  EXPECT_NE(std::string::npos, instance.display_name().find(instance_id));
}

/// @test Verify that InstanceAdmin::UpdateInstance works as expected.
TEST_F(InstanceAdminIntegrationTest, UpdateInstanceTest) {
  std::string instance_id =
      "it-" + bigtable::testing::Sample(generator_, 8,
                                        "abcdefghijklmnopqrstuvwxyz0123456789");
  auto config = IntegrationTestConfig(instance_id);

  auto instances_before = instance_admin_->ListInstances();
  auto instance = instance_admin_->CreateInstance(config).get();
  btadmin::Instance instance_copy;
  instance_copy.CopyFrom(instance);
  bigtable::InstanceUpdateConfig instance_update_config(std::move(instance));
  instance_update_config.set_display_name("foo");

  auto instance_after =
      instance_admin_->UpdateInstance(std::move(instance_update_config)).get();

  auto instances_after = instance_admin_->ListInstances();
  instance_admin_->DeleteInstance(instance_id);
  EXPECT_FALSE(IsInstancePresent(instances_before, instance_copy.name()));
  EXPECT_TRUE(IsInstancePresent(instances_after, instance_copy.name()));
  EXPECT_NE(std::string::npos, instance_copy.name().find(instance_id));
  EXPECT_NE(std::string::npos,
            instance_copy.name().find(InstanceTestEnvironment::project_id()));
  EXPECT_EQ("foo", instance_after.display_name());
  EXPECT_NE(std::string::npos, instance_copy.display_name().find(instance_id));
}
/// @test Verify that InstanceAdmin::ListInstances works as expected.
TEST_F(InstanceAdminIntegrationTest, ListInstancesTest) {
  std::string instance_id =
      "it-" + bigtable::testing::Sample(generator_, 8,
                                        "abcdefghijklmnopqrstuvwxyz0123456789");
  auto config = IntegrationTestConfig(instance_id);
  auto instances_before = instance_admin_->ListInstances();
  auto instance = instance_admin_->CreateInstance(config).get();
  auto instances_after = instance_admin_->ListInstances();
  instance_admin_->DeleteInstance(instance_id);
  EXPECT_FALSE(IsInstancePresent(instances_before, instance.name()));
  EXPECT_TRUE(IsInstancePresent(instances_after, instance.name()));

  for (auto const& i : instances_after) {
    auto const npos = std::string::npos;
    EXPECT_NE(npos, i.name().find(instance_admin_->project_name()));
  }
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
/// @test Verify that InstanceAdmin::GetInstance works as expected.
TEST_F(InstanceAdminIntegrationTest, GetInstanceTest) {
  std::string instance_id =
      "it-" + bigtable::testing::Sample(generator_, 8,
                                        "abcdefghijklmnopqrstuvwxyz0123456789");
  try {
    auto instance = instance_admin_->GetInstance(instance_id);
    FAIL();
  } catch (bigtable::GRpcError const& ex) {
    EXPECT_EQ(grpc::StatusCode::NOT_FOUND, ex.error_code());
  }
  auto config = IntegrationTestConfig(instance_id);
  auto instance_create = instance_admin_->CreateInstance(config).get();
  auto instance = instance_admin_->GetInstance(instance_id);
  instance_admin_->DeleteInstance(instance_id);

  auto const npos = std::string::npos;
  EXPECT_NE(npos, instance.name().find(instance_admin_->project_name()));
  EXPECT_NE(npos, instance.name().find(instance_id));
  EXPECT_EQ(bigtable::InstanceConfig::DEVELOPMENT, instance.type());
  EXPECT_EQ(instance.READY, instance.state());
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

/// @test Verify that InstanceAdmin::DeleteInstances works as expected.
TEST_F(InstanceAdminIntegrationTest, DeleteInstancesTest) {
  std::string instance_id =
      "it-" + bigtable::testing::Sample(generator_, 8,
                                        "abcdefghijklmnopqrstuvwxyz0123456789");
  auto config = IntegrationTestConfig(instance_id);
  auto instance = instance_admin_->CreateInstance(config).get();
  auto instances_before = instance_admin_->ListInstances();
  instance_admin_->DeleteInstance(instance_id);
  auto instances_after = instance_admin_->ListInstances();
  EXPECT_TRUE(IsInstancePresent(instances_before, instance.name()));
  EXPECT_FALSE(IsInstancePresent(instances_after, instance.name()));
}

/// @test Verify that InstanceAdmin::ListClusters works as expected.
TEST_F(InstanceAdminIntegrationTest, ListClustersTest) {
  // The emulator does not support cluster operations.
  if (UsingCloudBigtableEmulator()) {
    return;
  }
  std::string id = "list-clusters-test";
  bigtable::InstanceId instance_id(id);
  bigtable::DisplayName display_name(id);
  std::vector<std::pair<std::string, bigtable::ClusterConfig>> clusters_config;
  clusters_config.push_back(std::make_pair(
      id + "-cluster1", bigtable::ClusterConfig("us-central1-f", 0,
                                                bigtable::ClusterConfig::HDD)));
  auto instance_config =
      bigtable::InstanceConfig(instance_id, display_name, clusters_config)
          .set_type(bigtable::InstanceConfig::DEVELOPMENT);
  auto instance_details =
      instance_admin_->CreateInstance(instance_config).get();

  // Create clusters in an instance
  // TODO(#422) - Implement InstanceAdmin::CreateCluster

  // TODO(#418) - create an instance and test that its cluster is returned here.
  auto clusters = instance_admin_->ListClusters(id);
  for (auto const& i : clusters) {
    auto const npos = std::string::npos;
    EXPECT_NE(npos, i.name().find(instance_admin_->project_name()));
  }
  EXPECT_FALSE(clusters.empty());

  instance_admin_->DeleteInstance(id);
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
