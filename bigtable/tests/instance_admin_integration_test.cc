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

  static std::string const &project_id() { return project_id_; }

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

}  // anonymous namespace

/// @test Verify that InstanceAdmin::CreateInstance works as expected.
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
namespace {
bool IsInstancePresent(std::vector<btadmin::Instance> const& instances,
                       std::string const& instance_name) {
  return instances.end() !=
      std::find_if(instances.begin(), instances.end(),
                   [&instance_name](btadmin::Instance const &i) {
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

TEST_F(InstanceAdminIntegrationTest, CreateInstanceTest) {
  std::string instance_id =
      "it-" + bigtable::testing::Sample(generator_, 8,
                                        "abcdefghijklmnopqrstuvwxyz0123456789");
  auto config = IntegrationTestConfig(instance_id);
  if (UsingCloudBigtableEmulator()) {
    // We cannot test the functionality against the emulator, but we can at
    // least test that the gRPC calls are made and the right error is returned.
    auto future = instance_admin_->CreateInstance(config);
    try {
      future.get();
    } catch (bigtable::GRpcError const& ex) {
      EXPECT_EQ(grpc::StatusCode::UNIMPLEMENTED, ex.error_code());
    }
    return;
  }

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

/// @test Verify that InstanceAdmin::ListInstances works as expected.
TEST_F(InstanceAdminIntegrationTest, ListInstancesTest) {
  // The emulator does not support instance operations.
  if (UsingCloudBigtableEmulator()) {
    // We cannot test the functionality against the emulator, but we can at
    // least test that the gRPC calls are made and the right error is returned.
    try {
      auto instances = instance_admin_->ListInstances();
    } catch (bigtable::GRpcError const& ex) {
      EXPECT_EQ(grpc::StatusCode::UNIMPLEMENTED, ex.error_code());
    }
    return;
  }

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

/// @test Verify that InstanceAdmin::GetInstances works as expected.
TEST_F(InstanceAdminIntegrationTest, GetInstancesTest) {
  // The emulator does not support instance operations.
  if (UsingCloudBigtableEmulator()) {
    return;
  }
  // TODO(#418) - make the test functional as part of the CreateInstance()
  // implementation.
  auto instance = instance_admin_->GetInstance("t0");
  auto const npos = std::string::npos;
  EXPECT_NE(npos, instance.name().find(instance_admin_->project_name()));
}

/// @test Verify that InstanceAdmin::DeleteInstances works as expected.
TEST_F(InstanceAdminIntegrationTest, DeleteInstancesTest) {
  std::string instance_id =
      "it-" + bigtable::testing::Sample(generator_, 8,
                                        "abcdefghijklmnopqrstuvwxyz0123456789");
  if (UsingCloudBigtableEmulator()) {
    // We cannot test the functionality against the emulator, but we can at
    // least test that the gRPC calls are made and the right error is returned.
    try {
      instance_admin_->DeleteInstance(instance_id);
    } catch (bigtable::GRpcError const& ex) {
      EXPECT_EQ(grpc::StatusCode::UNIMPLEMENTED, ex.error_code());
    }
    return;
  }

  auto config = IntegrationTestConfig(instance_id);
  auto instance = instance_admin_->CreateInstance(config).get();
  auto instances_before = instance_admin_->ListInstances();
  instance_admin_->DeleteInstance(instance_id);
  auto instances_after = instance_admin_->ListInstances();
  EXPECT_TRUE(IsInstancePresent(instances_before, instance.name()));
  EXPECT_FALSE(IsInstancePresent(instances_after, instance.name()));
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

/// @test Verify that InstanceAdmin::ListClusters works as expected.
TEST_F(InstanceAdminIntegrationTest, ListClustersTest) {
  // The emulator does not support cluster operations.
  if (UsingCloudBigtableEmulator()) {
    return;
  }

  // TODO(#490) - ListCluster() fails without an instance_id parameter.
  // instance_admin_->ListClusters();
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
