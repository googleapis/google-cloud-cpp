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

#include "google/cloud/bigtable/grpc_error.h"
#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/internal/make_unique.h"
#include "google/cloud/internal/random.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace btadmin = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;
using testing::HasSubstr;

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
  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();
};

bool IsInstancePresent(std::vector<btadmin::Instance> const& instances,
                       std::string const& instance_name) {
  return instances.end() !=
         std::find_if(instances.begin(), instances.end(),
                      [&instance_name](btadmin::Instance const& i) {
                        return i.name() == instance_name;
                      });
}

bool IsClusterPresent(std::vector<btadmin::Cluster> const& clusters,
                      std::string const& cluster_name) {
  return clusters.end() !=
         std::find_if(clusters.begin(), clusters.end(),
                      [&cluster_name](btadmin::Cluster const& i) {
                        return i.name() == cluster_name;
                      });
}

bigtable::InstanceConfig IntegrationTestConfig(
    std::string const& id, std::string const& zone = "us-central1-f",
    bigtable::InstanceConfig::InstanceType instance_type =
        bigtable::InstanceConfig::DEVELOPMENT,
    int32_t serve_node = 0) {
  bigtable::InstanceId instance_id(id);
  bigtable::DisplayName display_name("Integration Tests " + id);
  auto cluster_config =
      bigtable::ClusterConfig(zone, serve_node, bigtable::ClusterConfig::HDD);
  bigtable::InstanceConfig config(instance_id, display_name,
                                  {{id + "-c1", cluster_config}});
  config.set_type(instance_type);
  return config;
}

}  // anonymous namespace

/// @test Verify that InstanceAdmin::CreateInstance works as expected.
TEST_F(InstanceAdminIntegrationTest, CreateInstanceTest) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  auto config = IntegrationTestConfig(instance_id);

  auto instances_before = instance_admin_->ListInstances();
  auto instance = instance_admin_->CreateInstance(config).get();
  auto instances_after = instance_admin_->ListInstances();
  instance_admin_->DeleteInstance(instance_id);
  EXPECT_FALSE(IsInstancePresent(instances_before, instance.name()));
  EXPECT_TRUE(IsInstancePresent(instances_after, instance.name()));
  EXPECT_THAT(instance.name(), HasSubstr(instance_id));
  EXPECT_THAT(instance.name(),
              HasSubstr(InstanceTestEnvironment::project_id()));
  EXPECT_THAT(instance.display_name(), HasSubstr(instance_id));
}

/// @test Verify that InstanceAdmin::UpdateInstance works as expected.
TEST_F(InstanceAdminIntegrationTest, UpdateInstanceTest) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  auto config = IntegrationTestConfig(instance_id);

  auto instances_before = instance_admin_->ListInstances();
  auto instance = instance_admin_->CreateInstance(config).get();
  btadmin::Instance instance_copy;
  instance_copy.CopyFrom(instance);
  bigtable::InstanceUpdateConfig instance_update_config(std::move(instance));
  auto const updated_display_name = instance_id + " updated";
  instance_update_config.set_display_name(updated_display_name);

  auto instance_after =
      instance_admin_->UpdateInstance(std::move(instance_update_config)).get();

  auto instances_after = instance_admin_->ListInstances();
  instance_admin_->DeleteInstance(instance_id);
  EXPECT_FALSE(IsInstancePresent(instances_before, instance_copy.name()));
  EXPECT_TRUE(IsInstancePresent(instances_after, instance_copy.name()));
  EXPECT_THAT(instance_copy.name(), HasSubstr(instance_id));
  EXPECT_THAT(instance_copy.name(),
              HasSubstr(InstanceTestEnvironment::project_id()));
  EXPECT_EQ(updated_display_name, instance_after.display_name());
  EXPECT_THAT(instance_copy.display_name(), HasSubstr(instance_id));
}
/// @test Verify that InstanceAdmin::ListInstances works as expected.
TEST_F(InstanceAdminIntegrationTest, ListInstancesTest) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
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
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
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
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  auto config = IntegrationTestConfig(instance_id);
  auto instance = instance_admin_->CreateInstance(config).get();
  auto instances_before = instance_admin_->ListInstances();
  instance_admin_->DeleteInstance(instance_id);
  auto instances_after = instance_admin_->ListInstances();
  EXPECT_TRUE(IsInstancePresent(instances_before, instance.name()));
  EXPECT_FALSE(IsInstancePresent(instances_after, instance.name()));
}

/// @test Verify that InstanceAdmin::CreateCluster works as expected.
TEST_F(InstanceAdminIntegrationTest, CreateClusterTest) {
  std::string id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  bigtable::InstanceId instance_id(id);
  auto instance_config = IntegrationTestConfig(
      id, "us-central1-f", bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_details =
      instance_admin_->CreateInstance(instance_config).get();
  auto clusters_before = instance_admin_->ListClusters(id);
  bigtable::ClusterId cluster_id(id + "-cl2");
  auto location = "us-central1-b";
  auto cluster_config =
      bigtable::ClusterConfig(location, 3, bigtable::ClusterConfig::HDD);
  auto cluster =
      instance_admin_->CreateCluster(cluster_config, instance_id, cluster_id)
          .get();
  auto clusters_after = instance_admin_->ListClusters(id);
  instance_admin_->DeleteInstance(id);

  EXPECT_FALSE(IsClusterPresent(clusters_before, cluster.name()));
  EXPECT_TRUE(IsClusterPresent(clusters_after, cluster.name()));
}

/// @test Verify that InstanceAdmin::ListClusters works as expected.
TEST_F(InstanceAdminIntegrationTest, ListClustersTest) {
  std::string id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  auto config = IntegrationTestConfig(id);
  auto instance = instance_admin_->CreateInstance(config);
  ASSERT_THAT(instance.get().name(), HasSubstr(id));

  auto clusters = instance_admin_->ListClusters(id);
  for (auto const& cluster : clusters) {
    EXPECT_NE(std::string::npos,
              cluster.name().find(instance_admin_->project_name()));
    EXPECT_NE(std::string::npos, cluster.name().find(id));
  }
  EXPECT_FALSE(clusters.empty());

  instance_admin_->DeleteInstance(id);
}

/// @test Verify that default InstanceAdmin::ListClusters works as expected.
TEST_F(InstanceAdminIntegrationTest, ListAllClustersTest) {
  std::string id1 =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string id2 =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  auto instance_config1 = IntegrationTestConfig(
      id1, "us-central1-c", bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_config2 = IntegrationTestConfig(
      id2, "us-central1-f", bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance1 = instance_admin_->CreateInstance(instance_config1);
  auto instance2 = instance_admin_->CreateInstance(instance_config2);
  // Wait for instance creation
  ASSERT_THAT(instance1.get().name(), HasSubstr(id1));
  ASSERT_THAT(instance2.get().name(), HasSubstr(id2));

  auto clusters = instance_admin_->ListClusters();
  for (auto const& cluster : clusters) {
    EXPECT_NE(std::string::npos,
              cluster.name().find(instance_admin_->project_name()));
  }
  EXPECT_FALSE(clusters.empty());

  instance_admin_->DeleteInstance(id1);
  instance_admin_->DeleteInstance(id2);
}

/// @test Verify that InstanceAdmin::UpdateCluster works as expected.
TEST_F(InstanceAdminIntegrationTest, UpdateClusterTest) {
  std::string id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  bigtable::InstanceId instance_id(id);
  auto instance_config1 = IntegrationTestConfig(
      id, "us-central1-f", bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_details =
      instance_admin_->CreateInstance(instance_config1).get();

  auto clusters_before = instance_admin_->ListClusters(id);

  bigtable::ClusterId another_cluster_id(id + "-cl2");
  auto location = "us-central1-b";
  auto cluster_config =
      bigtable::ClusterConfig(location, 3, bigtable::ClusterConfig::HDD);
  auto cluster_before =
      instance_admin_
          ->CreateCluster(cluster_config, instance_id, another_cluster_id)
          .get();

  btadmin::Cluster cluster_copy;
  cluster_copy.CopyFrom(cluster_before);

  // update the storage type
  cluster_before.set_serve_nodes(4);
  cluster_before.clear_state();
  bigtable::ClusterConfig updated_cluster_config(std::move(cluster_before));
  auto cluster_after =
      instance_admin_->UpdateCluster(std::move(updated_cluster_config)).get();

  auto clusters_after = instance_admin_->ListClusters(instance_id.get());
  instance_admin_->DeleteInstance(id);
  EXPECT_FALSE(IsClusterPresent(clusters_before, cluster_copy.name()));
  EXPECT_TRUE(IsClusterPresent(clusters_after, cluster_copy.name()));
  EXPECT_THAT(cluster_copy.name(), HasSubstr(id));
  EXPECT_THAT(cluster_copy.name(),
              HasSubstr(InstanceTestEnvironment::project_id()));
  EXPECT_EQ(3, cluster_copy.serve_nodes());
  EXPECT_EQ(4, cluster_after.serve_nodes());
}

/// @test Verify that default InstanceAdmin::GetCluster works as expected.
TEST_F(InstanceAdminIntegrationTest, GetClusterTest) {
  std::string id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  bigtable::InstanceId instance_id(id);
  bigtable::ClusterId cluster_id1(id + "-cl1");
  bigtable::ClusterId cluster_id2(id + "-cl2");
  bigtable::DisplayName display_name(id);

  std::vector<std::pair<std::string, bigtable::ClusterConfig>> clusters_config;
  clusters_config.push_back(
      std::make_pair(cluster_id1.get(),
                     bigtable::ClusterConfig("us-central1-f", 3,
                                             bigtable::ClusterConfig::HDD)));
  clusters_config.push_back(
      std::make_pair(cluster_id2.get(),
                     bigtable::ClusterConfig("us-central1-b", 3,
                                             bigtable::ClusterConfig::HDD)));
  auto instance_config =
      bigtable::InstanceConfig(instance_id, display_name, clusters_config)
          .set_type(bigtable::InstanceConfig::PRODUCTION);
  auto instance_details =
      instance_admin_->CreateInstance(instance_config).get();

  auto cluster = instance_admin_->GetCluster(instance_id, cluster_id1);

  std::string cluster_name_prefix =
      instance_admin_->project_name() + "/instances/" + id + "/clusters/";

  instance_admin_->DeleteInstance(id);

  EXPECT_EQ(cluster_name_prefix + cluster_id1.get(), cluster.name());
  EXPECT_NE(cluster_name_prefix + cluster_id2.get(), cluster.name());
}

/// @test Verify that default InstanceAdmin::DeleteClusters works as expected.
TEST_F(InstanceAdminIntegrationTest, DeleteClustersTest) {
  std::string id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  bigtable::InstanceId instance_id(id);
  bigtable::ClusterId cluster_id1(id + "-cl1");
  bigtable::ClusterId cluster_id2(id + "-cl2");
  bigtable::DisplayName display_name(id);

  std::vector<std::pair<std::string, bigtable::ClusterConfig>> clusters_config;
  clusters_config.push_back(
      std::make_pair(cluster_id1.get(),
                     bigtable::ClusterConfig("us-central1-f", 3,
                                             bigtable::ClusterConfig::HDD)));
  clusters_config.push_back(
      std::make_pair(cluster_id2.get(),
                     bigtable::ClusterConfig("us-central1-b", 3,
                                             bigtable::ClusterConfig::HDD)));
  auto instance_config =
      bigtable::InstanceConfig(instance_id, display_name, clusters_config)
          .set_type(bigtable::InstanceConfig::PRODUCTION);
  auto instance_details =
      instance_admin_->CreateInstance(instance_config).get();

  auto clusters_before = instance_admin_->ListClusters(id);

  instance_admin_->DeleteCluster(std::move(instance_id),
                                 std::move(cluster_id2));

  auto clusters_after = instance_admin_->ListClusters(id);

  instance_admin_->DeleteInstance(id);

  EXPECT_EQ(2U, clusters_before.size());
  EXPECT_TRUE(IsClusterPresent(
      clusters_before, instance_details.name() + "/clusters/" + id + "-cl2"));
  EXPECT_FALSE(IsClusterPresent(
      clusters_after, instance_details.name() + "/clusters/" + id + "-cl2"));
  EXPECT_EQ(1U, clusters_after.size());
}

/// @test Verify that AppProfile CRUD operations work as expected.
TEST_F(InstanceAdminIntegrationTest, CreateListAppProfile) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  auto instance_config = IntegrationTestConfig(
      instance_id, "us-central1-c", bigtable::InstanceConfig::PRODUCTION, 3);
  auto future = instance_admin_->CreateInstance(instance_config);
  // Wait for instance creation
  ASSERT_THAT(future.get().name(), HasSubstr(instance_id));

  std::string id1 =
      "profile-" + google::cloud::internal::Sample(
                       generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string id2 =
      "profile-" + google::cloud::internal::Sample(
                       generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  auto initial_profiles = instance_admin_->ListAppProfiles(instance_id);

  // Simplify writing the rest of the test.
  auto count_matching_profiles =
      [](std::string const& id, std::vector<btadmin::AppProfile> const& list) {
        std::string suffix = "/appProfiles/" + id;
        return std::count_if(
            list.begin(), list.end(), [&suffix](btadmin::AppProfile const& x) {
              return std::string::npos != x.name().find(suffix);
            });
      };

  EXPECT_EQ(0U, count_matching_profiles(id1, initial_profiles));
  EXPECT_EQ(0U, count_matching_profiles(id2, initial_profiles));

  auto profile_1 = instance_admin_->CreateAppProfile(
      bigtable::InstanceId(instance_id),
      bigtable::AppProfileConfig::MultiClusterUseAny(
          bigtable::AppProfileId(id1)));
  auto profile_2 = instance_admin_->CreateAppProfile(
      bigtable::InstanceId(instance_id),
      bigtable::AppProfileConfig::MultiClusterUseAny(
          bigtable::AppProfileId(id2)));

  auto current_profiles = instance_admin_->ListAppProfiles(instance_id);
  EXPECT_EQ(1U, count_matching_profiles(id1, current_profiles));
  EXPECT_EQ(1U, count_matching_profiles(id2, current_profiles));

  instance_admin_->DeleteInstance(instance_id);
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
