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

#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace btadmin = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;
using testing::HasSubstr;

namespace {

// Initialized in main() below.
char const* flag_project_id;
char const* flag_zone_a;
char const* flag_zone_b;
char const* flag_service_account;

class InstanceAdminIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto instance_admin_client = bigtable::CreateDefaultInstanceAdminClient(
        flag_project_id, bigtable::ClientOptions());
    instance_admin_ =
        google::cloud::internal::make_unique<bigtable::InstanceAdmin>(
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
    std::string const& instance_id, std::string const& zone,
    bigtable::InstanceConfig::InstanceType instance_type =
        bigtable::InstanceConfig::DEVELOPMENT,
    int32_t serve_node = 0) {
  // The description cannot exceed 30 characters
  auto display_name = ("IT " + instance_id).substr(0, 30);
  auto cluster_config =
      bigtable::ClusterConfig(zone, serve_node, bigtable::ClusterConfig::HDD);
  bigtable::InstanceConfig config(instance_id, display_name,
                                  {{instance_id + "-c1", cluster_config}});
  config.set_type(instance_type);
  return config;
}

}  // anonymous namespace

/// @test Verify that default InstanceAdmin::ListClusters works as expected.
TEST_F(InstanceAdminIntegrationTest, ListAllClustersTest) {
  std::string id1 =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string id2 =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  auto instance_config1 = IntegrationTestConfig(
      id1, flag_zone_a, bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_config2 = IntegrationTestConfig(
      id2, flag_zone_b, bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance1_future = instance_admin_->CreateInstance(instance_config1);
  auto instance2_future = instance_admin_->CreateInstance(instance_config2);
  // Wait for instance creation
  auto instance1 = instance1_future.get();
  auto instance2 = instance2_future.get();
  EXPECT_STATUS_OK(instance1);
  EXPECT_STATUS_OK(instance2);

  EXPECT_THAT(instance1->name(), HasSubstr(id1));
  EXPECT_THAT(instance2->name(), HasSubstr(id2));

  auto clusters = instance_admin_->ListClusters();
  ASSERT_STATUS_OK(clusters);
  for (auto const& cluster : clusters->clusters) {
    EXPECT_NE(std::string::npos,
              cluster.name().find(instance_admin_->project_name()));
  }
  EXPECT_FALSE(clusters->clusters.empty());

  EXPECT_STATUS_OK(instance_admin_->DeleteInstance(id2));
  EXPECT_STATUS_OK(instance_admin_->DeleteInstance(id1));
}

/// @test Verify that AppProfile CRUD operations work as expected.
TEST_F(InstanceAdminIntegrationTest, CreateListGetDeleteAppProfile) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  auto instance_config = IntegrationTestConfig(
      instance_id, flag_zone_a, bigtable::InstanceConfig::PRODUCTION, 3);
  auto future = instance_admin_->CreateInstance(instance_config);
  auto actual = future.get();
  EXPECT_STATUS_OK(actual);
  // Wait for instance creation
  ASSERT_THAT(actual->name(), HasSubstr(instance_id));

  std::string id1 =
      "profile-" + google::cloud::internal::Sample(
                       generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string id2 =
      "profile-" + google::cloud::internal::Sample(
                       generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  auto initial_profiles = instance_admin_->ListAppProfiles(instance_id);
  ASSERT_STATUS_OK(initial_profiles);

  // Simplify writing the rest of the test.
  auto count_matching_profiles =
      [](std::string const& id, std::vector<btadmin::AppProfile> const& list) {
        std::string suffix = "/appProfiles/" + id;
        return std::count_if(
            list.begin(), list.end(), [&suffix](btadmin::AppProfile const& x) {
              return std::string::npos != x.name().find(suffix);
            });
      };

  EXPECT_EQ(0, count_matching_profiles(id1, *initial_profiles));
  EXPECT_EQ(0, count_matching_profiles(id2, *initial_profiles));

  auto profile_1 = instance_admin_->CreateAppProfile(
      instance_id, bigtable::AppProfileConfig::MultiClusterUseAny(id1));
  ASSERT_STATUS_OK(profile_1);
  auto profile_2 = instance_admin_->CreateAppProfile(
      instance_id, bigtable::AppProfileConfig::MultiClusterUseAny(id2));
  ASSERT_STATUS_OK(profile_2);

  auto current_profiles = instance_admin_->ListAppProfiles(instance_id);
  ASSERT_STATUS_OK(current_profiles);
  EXPECT_EQ(1, count_matching_profiles(id1, *current_profiles));
  EXPECT_EQ(1, count_matching_profiles(id2, *current_profiles));

  auto detail_1 = instance_admin_->GetAppProfile(instance_id, id1);
  ASSERT_STATUS_OK(detail_1);
  EXPECT_EQ(detail_1->name(), profile_1->name());
  EXPECT_THAT(detail_1->name(), HasSubstr(instance_id));
  EXPECT_THAT(detail_1->name(), HasSubstr(id1));

  auto detail_2 = instance_admin_->GetAppProfile(instance_id, id2);
  ASSERT_STATUS_OK(detail_2);
  EXPECT_EQ(detail_2->name(), profile_2->name());
  EXPECT_THAT(detail_2->name(), HasSubstr(instance_id));
  EXPECT_THAT(detail_2->name(), HasSubstr(id2));

  auto profile_updated_future = instance_admin_->UpdateAppProfile(
      instance_id, id2,
      bigtable::AppProfileUpdateConfig().set_description("new description"));

  auto update_2 = profile_updated_future.get();
  auto detail_2_after_update = instance_admin_->GetAppProfile(instance_id, id2);
  ASSERT_STATUS_OK(detail_2_after_update);
  EXPECT_EQ("new description", update_2->description());
  EXPECT_EQ("new description", detail_2_after_update->description());

  ASSERT_STATUS_OK(instance_admin_->DeleteAppProfile(instance_id, id1, true));
  current_profiles = instance_admin_->ListAppProfiles(instance_id);
  ASSERT_STATUS_OK(current_profiles);
  EXPECT_EQ(0, count_matching_profiles(id1, *current_profiles));
  EXPECT_EQ(1, count_matching_profiles(id2, *current_profiles));

  ASSERT_STATUS_OK(instance_admin_->DeleteAppProfile(instance_id, id2, true));
  current_profiles = instance_admin_->ListAppProfiles(instance_id);
  ASSERT_STATUS_OK(current_profiles);
  EXPECT_EQ(0, count_matching_profiles(id1, *current_profiles));
  EXPECT_EQ(0, count_matching_profiles(id2, *current_profiles));

  ASSERT_STATUS_OK(instance_admin_->DeleteInstance(instance_id));
}

/// @test Verify that Instance CRUD operations work as expected.
TEST_F(InstanceAdminIntegrationTest, CreateListGetDeleteInstanceTest) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  // verify new instance id in list of instances
  auto instances_before = instance_admin_->ListInstances();
  ASSERT_STATUS_OK(instances_before);
  ASSERT_TRUE(instances_before->failed_locations.empty());
  ASSERT_FALSE(IsInstancePresent(instances_before->instances, instance_id))
      << "Instance (" << instance_id << ") already exists."
      << " This is unexpected, as the instance ids are"
      << " generated at random.";

  // create instance
  auto config = IntegrationTestConfig(instance_id, flag_zone_a);
  auto instance = instance_admin_->CreateInstance(config).get();
  ASSERT_STATUS_OK(instance);

  auto instances_current = instance_admin_->ListInstances();
  ASSERT_STATUS_OK(instances_current);
  ASSERT_TRUE(instances_current->failed_locations.empty());
  EXPECT_TRUE(
      IsInstancePresent(instances_current->instances, instance->name()));

  // Get instance
  auto instance_check = instance_admin_->GetInstance(instance_id);
  ASSERT_STATUS_OK(instance_check);
  auto const npos = std::string::npos;
  EXPECT_NE(npos, instance_check->name().find(instance_admin_->project_name()));
  EXPECT_NE(npos, instance_check->name().find(instance_id));

  // update instance
  google::cloud::StatusOr<btadmin::Instance> instance_copy;
  instance_copy = *instance;
  bigtable::InstanceUpdateConfig instance_update_config(std::move(*instance));
  auto const updated_display_name = instance_id + " updated";
  instance_update_config.set_display_name(updated_display_name);
  auto instance_after =
      instance_admin_->UpdateInstance(std::move(instance_update_config)).get();
  auto instance_after_update = instance_admin_->GetInstance(instance_id);
  ASSERT_STATUS_OK(instance_after_update);
  EXPECT_EQ(updated_display_name, instance_after_update->display_name());

  // Delete instance
  ASSERT_STATUS_OK(instance_admin_->DeleteInstance(instance_id));
  auto instances_after_delete = instance_admin_->ListInstances();
  ASSERT_STATUS_OK(instances_after_delete);
  ASSERT_TRUE(instances_after_delete->failed_locations.empty());
  EXPECT_TRUE(
      IsInstancePresent(instances_current->instances, instance_copy->name()));
  EXPECT_FALSE(
      IsInstancePresent(instances_after_delete->instances, instance->name()));
}

/// @test Verify that cluster CRUD operations work as expected.
TEST_F(InstanceAdminIntegrationTest, CreateListGetDeleteClusterTest) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string cluster_id_str = instance_id + "-cl2";

  // create instance prerequisites for cluster operations
  auto instance_config = IntegrationTestConfig(
      instance_id, flag_zone_a, bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_details =
      instance_admin_->CreateInstance(instance_config).get();
  ASSERT_STATUS_OK(instance_details);

  // create cluster
  auto clusters_before = instance_admin_->ListClusters(instance_id);
  ASSERT_STATUS_OK(clusters_before);
  ASSERT_FALSE(IsClusterPresent(clusters_before->clusters, cluster_id_str))
      << "Cluster (" << cluster_id_str << ") already exists."
      << " This is unexpected, as the cluster ids are"
      << " generated at random.";
  std::string cluster_id(cluster_id_str);
  auto cluster_config =
      bigtable::ClusterConfig(flag_zone_b, 3, bigtable::ClusterConfig::HDD);
  auto cluster =
      instance_admin_->CreateCluster(cluster_config, instance_id, cluster_id)
          .get();
  auto clusters_after = instance_admin_->ListClusters(instance_id);
  ASSERT_STATUS_OK(clusters_after);
  EXPECT_FALSE(IsClusterPresent(clusters_before->clusters, cluster->name()));
  EXPECT_TRUE(IsClusterPresent(clusters_after->clusters, cluster->name()));

  // Get cluster
  auto cluster_check = instance_admin_->GetCluster(instance_id, cluster_id);
  ASSERT_STATUS_OK(cluster_check);
  std::string cluster_name_prefix = instance_admin_->project_name() +
                                    "/instances/" + instance_id + "/clusters/";
  EXPECT_EQ(cluster_name_prefix + cluster_id, cluster_check->name());

  // Update cluster
  google::cloud::StatusOr<btadmin::Cluster> cluster_copy;
  cluster_copy = *cluster;
  // update the storage type
  cluster->set_serve_nodes(4);
  cluster->clear_state();
  bigtable::ClusterConfig updated_cluster_config(std::move(*cluster));
  auto cluster_after_update =
      instance_admin_->UpdateCluster(std::move(updated_cluster_config)).get();
  auto check_cluster_after_update =
      instance_admin_->GetCluster(instance_id, cluster_id);
  ASSERT_STATUS_OK(check_cluster_after_update);

  EXPECT_EQ(3, cluster_copy->serve_nodes());
  EXPECT_EQ(4, check_cluster_after_update->serve_nodes());

  // Delete cluster
  ASSERT_STATUS_OK(instance_admin_->DeleteCluster(instance_id, cluster_id));
  auto clusters_after_delete = instance_admin_->ListClusters(instance_id);
  ASSERT_STATUS_OK(clusters_after_delete);
  ASSERT_STATUS_OK(instance_admin_->DeleteInstance(instance_id));
  EXPECT_TRUE(IsClusterPresent(
      clusters_after->clusters,
      instance_details->name() + "/clusters/" + instance_id + "-cl2"));
  EXPECT_FALSE(IsClusterPresent(
      clusters_after_delete->clusters,
      instance_details->name() + "/clusters/" + instance_id + "-cl2"));
}

/// @test Verify that IAM Policy APIs work as expected.
TEST_F(InstanceAdminIntegrationTest, SetGetTestIamAPIsTest) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  // create instance prerequisites for cluster operations
  auto instance_config = IntegrationTestConfig(
      instance_id, flag_zone_a, bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_details =
      instance_admin_->CreateInstance(instance_config).get();
  ASSERT_STATUS_OK(instance_details);

  auto iam_bindings = google::cloud::IamBindings(
      "roles/bigtable.reader",
      {"serviceAccount:" + std::string(flag_service_account)});

  auto initial_policy =
      instance_admin_->SetIamPolicy(instance_id, iam_bindings);
  ASSERT_STATUS_OK(initial_policy);

  auto fetched_policy = instance_admin_->GetIamPolicy(instance_id);
  ASSERT_STATUS_OK(fetched_policy);

  EXPECT_EQ(initial_policy->version, fetched_policy->version);
  EXPECT_EQ(initial_policy->etag, fetched_policy->etag);

  auto permission_set = instance_admin_->TestIamPermissions(
      instance_id, {"bigtable.tables.list", "bigtable.tables.delete"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->size());
  EXPECT_STATUS_OK(instance_admin_->DeleteInstance(instance_id));
}

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  if (argc != 5) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    // Show usage if number of arguments is invalid.
    std::cerr << "Usage: " << cmd.substr(last_slash + 1) << " <project_id>"
              << " <zone-a> <zone-b> <service-account>\n";
    return 1;
  }

  flag_project_id = argv[1];
  flag_zone_a = argv[2];
  flag_zone_b = argv[3];
  flag_service_account = argv[4];

  return RUN_ALL_TESTS();
}
