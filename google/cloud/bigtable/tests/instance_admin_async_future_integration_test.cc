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
#include "google/cloud/future.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/contains_once.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

using ::google::cloud::testing_util::ContainsOnce;
using ::testing::Contains;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::Not;
namespace btadmin = google::bigtable::admin::v2;

class InstanceAdminAsyncFutureIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (google::cloud::internal::GetEnv(
            "ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS")
            .value_or("") != "yes") {
      GTEST_SKIP();
    }
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty());
    zone_a_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A")
            .value_or("");
    ASSERT_FALSE(zone_a_.empty());
    zone_b_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B")
            .value_or("");
    ASSERT_FALSE(zone_b_.empty());
    service_account_ = google::cloud::internal::GetEnv(
                           "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT")
                           .value_or("");
    ASSERT_FALSE(service_account_.empty());

    auto instance_admin_client = bigtable::CreateDefaultInstanceAdminClient(
        project_id_, bigtable::ClientOptions());
    instance_admin_ =
        absl::make_unique<bigtable::InstanceAdmin>(instance_admin_client);
  }

  std::string project_id_;
  std::string zone_a_;
  std::string zone_b_;
  std::string service_account_;
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

bool IsIdOrNamePresentInClusterList(
    std::vector<btadmin::Cluster> const& clusters,
    std::string const& cluster_id) {
  return clusters.end() !=
         std::find_if(clusters.begin(), clusters.end(),
                      [&cluster_id](btadmin::Cluster const& i) {
                        return i.name().find(cluster_id) != std::string::npos;
                      });
}

bigtable::InstanceConfig IntegrationTestConfig(
    std::string const& instance_id, std::string const& zone,
    bigtable::InstanceConfig::InstanceType instance_type =
        bigtable::InstanceConfig::DEVELOPMENT,
    int32_t serve_node = 0) {
  // This cannot have more than 30 characters.
  auto display_name = ("Integration Tests " + instance_id).substr(0, 30);
  auto cluster_config =
      bigtable::ClusterConfig(zone, serve_node, bigtable::ClusterConfig::HDD);
  bigtable::InstanceConfig config(instance_id, display_name,
                                  {{instance_id + "-c1", cluster_config}});
  config.set_type(instance_type);
  return config;
}

/// @test Verify that Instance async future CRUD operations work as expected.
TEST_F(InstanceAdminAsyncFutureIntegrationTest,
       CreateListGetDeleteInstanceTest) {
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

  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  // Get async list instances
  // Make an asynchronous request, but immediately block because this is just a
  // test.
  auto instance_list = instance_admin_->AsyncListInstances(cq).get();
  ASSERT_STATUS_OK(instance_list);
  EXPECT_TRUE(instance_list->failed_locations.empty())
      << "The Cloud Bigtable service (or emulator) reports that it could not"
      << " retrieve the information for some locations. This is typically due"
      << " to an outage or some other transient condition.";
  ASSERT_FALSE(IsInstancePresent(instance_list->instances, instance_id))
      << "Instance (" << instance_id << ") already exists."
      << " This is unexpected, as the instance ids are"
      << " generated at random.";

  // create instance
  auto config = IntegrationTestConfig(instance_id, zone_a_);
  auto instance = instance_admin_->AsyncCreateInstance(cq, config).get();
  ASSERT_STATUS_OK(instance);

  auto async_instances_current = instance_admin_->AsyncListInstances(cq).get();
  ASSERT_STATUS_OK(async_instances_current);
  EXPECT_TRUE(
      IsInstancePresent(async_instances_current->instances, instance->name()));

  // Get instance
  google::cloud::future<google::cloud::StatusOr<btadmin::Instance>> fut =
      instance_admin_->AsyncGetInstance(cq, instance_id);
  auto instance_check = fut.get();
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
      instance_admin_
          ->AsyncUpdateInstance(cq, std::move(instance_update_config))
          .get();
  auto instance_after_update = instance_admin_->GetInstance(instance_id);
  ASSERT_STATUS_OK(instance_after_update);
  EXPECT_EQ(updated_display_name, instance_after_update->display_name());

  // Delete instance
  // Make an asynchronous request, but immediately block because this is just a
  // test.
  auto status = instance_admin_->AsyncDeleteInstance(instance_id, cq).get();
  ASSERT_STATUS_OK(status);
  auto instances_after_delete = instance_admin_->AsyncListInstances(cq).get();
  ASSERT_STATUS_OK(instances_after_delete);
  EXPECT_TRUE(IsInstancePresent(async_instances_current->instances,
                                instance_copy->name()));
  EXPECT_FALSE(
      IsInstancePresent(instances_after_delete->instances, instance->name()));

  cq.Shutdown();
  pool.join();
}

/// @test Verify that cluster async future CRUD operations work as expected.
TEST_F(InstanceAdminAsyncFutureIntegrationTest,
       CreateListGetDeleteClusterTest) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string cluster_id = instance_id + "-cl2";

  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  // create instance prerequisites for cluster operations
  auto instance_config = IntegrationTestConfig(
      instance_id, zone_a_, bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_details =
      instance_admin_->AsyncCreateInstance(cq, instance_config).get();
  ASSERT_STATUS_OK(instance_details);

  // Make an asynchronous request, but immediately block because this is just a
  // test.
  auto clusters_list_before =
      instance_admin_->AsyncListClusters(cq, instance_id).get();
  ASSERT_STATUS_OK(clusters_list_before);
  EXPECT_TRUE(clusters_list_before->failed_locations.empty())
      << "The Cloud Bigtable service (or emulator) reports that it could not"
      << " retrieve the information for some locations. This is typically due"
      << " to an outage or some other transient condition.";
  ASSERT_FALSE(IsIdOrNamePresentInClusterList(clusters_list_before->clusters,
                                              cluster_id));
  // create cluster
  auto cluster_config =
      bigtable::ClusterConfig(zone_b_, 3, bigtable::ClusterConfig::HDD);
  auto cluster =
      instance_admin_
          ->AsyncCreateCluster(cq, cluster_config, instance_id, cluster_id)
          .get();
  ASSERT_FALSE(cluster->name().empty());
  auto clusters_list_after =
      instance_admin_->AsyncListClusters(cq, instance_id).get();
  ASSERT_STATUS_OK(clusters_list_after);
  EXPECT_TRUE(clusters_list_after->failed_locations.empty())
      << "The Cloud Bigtable service (or emulator) reports that it could not"
      << " retrieve the information for some locations. This is typically due"
      << " to an outage or some other transient condition.";
  EXPECT_FALSE(
      IsClusterPresent(clusters_list_before->clusters, cluster->name()));
  EXPECT_TRUE(IsClusterPresent(clusters_list_after->clusters, cluster->name()));
  EXPECT_TRUE(IsIdOrNamePresentInClusterList(clusters_list_after->clusters,
                                             cluster_id));

  // Get cluster
  google::cloud::future<google::cloud::StatusOr<btadmin::Cluster>> fut =
      instance_admin_->AsyncGetCluster(cq, instance_id, cluster_id);
  auto cluster_check = fut.get();
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
      instance_admin_->AsyncUpdateCluster(cq, std::move(updated_cluster_config))
          .get();
  auto check_cluster_after_update =
      instance_admin_->GetCluster(instance_id, cluster_id);
  ASSERT_STATUS_OK(check_cluster_after_update);

  EXPECT_EQ(3, cluster_copy->serve_nodes());
  EXPECT_EQ(4, check_cluster_after_update->serve_nodes());

  // Delete cluster
  ASSERT_STATUS_OK(
      instance_admin_->AsyncDeleteCluster(cq, instance_id, cluster_id).get());
  auto clusters_list_after_delete =
      instance_admin_->AsyncListClusters(cq, instance_id).get();
  ASSERT_STATUS_OK(clusters_list_after_delete);
  // Make an asynchronous request, but immediately block because this is just a
  // test.
  auto status = instance_admin_->AsyncDeleteInstance(instance_id, cq).get();
  ASSERT_STATUS_OK(status);
  EXPECT_TRUE(IsClusterPresent(
      clusters_list_after->clusters,
      instance_details->name() + "/clusters/" + instance_id + "-cl2"));
  EXPECT_FALSE(IsClusterPresent(
      clusters_list_after_delete->clusters,
      instance_details->name() + "/clusters/" + instance_id + "-cl2"));
  EXPECT_FALSE(
      IsClusterPresent(clusters_list_after_delete->clusters, cluster->name()));
  cq.Shutdown();
  pool.join();
}

/// @test Verify that default AsyncListAllClusters works as expected.
TEST_F(InstanceAdminAsyncFutureIntegrationTest, AsyncListAllClustersTest) {
  std::string id1 =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string id2 =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  auto instance_config1 = IntegrationTestConfig(
      id1, zone_a_, bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_config2 = IntegrationTestConfig(
      id2, zone_b_, bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance1_future =
      instance_admin_->AsyncCreateInstance(cq, instance_config1);
  auto instance2_future =
      instance_admin_->AsyncCreateInstance(cq, instance_config2);

  // Wait for instance creation
  auto instance1 = instance1_future.get();
  auto instance2 = instance2_future.get();
  ASSERT_STATUS_OK(instance1);
  ASSERT_STATUS_OK(instance2);

  auto instance1_name = instance1->name();
  auto instance2_name = instance2->name();
  ASSERT_THAT(instance1_name, HasSubstr(id1));
  ASSERT_THAT(instance2_name, HasSubstr(id2));

  // Make an asynchronous request, but immediately block because this is just a
  // test.
  auto clusters_list = instance_admin_->AsyncListClusters(cq).get();
  ASSERT_STATUS_OK(clusters_list);
  for (auto const& cluster : clusters_list->clusters) {
    EXPECT_NE(std::string::npos,
              cluster.name().find(instance_admin_->project_name()));
  }
  EXPECT_FALSE(clusters_list->clusters.empty());

  EXPECT_TRUE(
      IsIdOrNamePresentInClusterList(clusters_list->clusters, instance1_name));
  EXPECT_TRUE(
      IsIdOrNamePresentInClusterList(clusters_list->clusters, instance2_name));

  EXPECT_STATUS_OK(instance_admin_->AsyncDeleteInstance(id1, cq).get());
  EXPECT_STATUS_OK(instance_admin_->AsyncDeleteInstance(id2, cq).get());

  cq.Shutdown();
  pool.join();
}

TEST_F(InstanceAdminAsyncFutureIntegrationTest, AsyncListAppProfilesTest) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  auto instance_config = IntegrationTestConfig(
      instance_id, zone_a_, bigtable::InstanceConfig::PRODUCTION, 3);
  auto future = instance_admin_->AsyncCreateInstance(cq, instance_config);
  auto actual = future.get();
  ASSERT_STATUS_OK(actual);
  // Wait for instance creation
  ASSERT_THAT(actual->name(), HasSubstr(instance_id));

  std::string id1 =
      "profile-" + google::cloud::internal::Sample(
                       generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string id2 =
      "profile-" + google::cloud::internal::Sample(
                       generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  auto initial_profiles =
      instance_admin_->AsyncListAppProfiles(cq, instance_id).get();
  ASSERT_STATUS_OK(initial_profiles);

  // Simplify writing the rest of the test.
  auto profile_names = [](std::vector<btadmin::AppProfile> const& list) {
    std::vector<std::string> names(list.size());
    std::transform(list.begin(), list.end(), names.begin(),
                   [](btadmin::AppProfile const& x) { return x.name(); });
    return names;
  };

  EXPECT_THAT(profile_names(*initial_profiles),
              Not(Contains(EndsWith("/appProfiles/" + id1))));
  EXPECT_THAT(profile_names(*initial_profiles),
              Not(Contains(EndsWith("/appProfiles/" + id2))));

  auto profile_1 = instance_admin_
                       ->AsyncCreateAppProfile(
                           cq, instance_id,
                           bigtable::AppProfileConfig::MultiClusterUseAny(id1))
                       .get();
  ASSERT_STATUS_OK(profile_1);
  auto profile_2 = instance_admin_
                       ->AsyncCreateAppProfile(
                           cq, instance_id,
                           bigtable::AppProfileConfig::MultiClusterUseAny(id2))
                       .get();
  ASSERT_STATUS_OK(profile_2);

  auto current_profiles =
      instance_admin_->AsyncListAppProfiles(cq, instance_id).get();
  ASSERT_STATUS_OK(current_profiles);
  EXPECT_THAT(profile_names(*current_profiles),
              ContainsOnce(EndsWith("/appProfiles/" + id1)));
  EXPECT_THAT(profile_names(*current_profiles),
              ContainsOnce(EndsWith("/appProfiles/" + id2)));

  auto detail_1 =
      instance_admin_->AsyncGetAppProfile(cq, instance_id, id1).get();
  ASSERT_STATUS_OK(detail_1);
  EXPECT_EQ(detail_1->name(), profile_1->name());
  EXPECT_THAT(detail_1->name(), HasSubstr(instance_id));
  EXPECT_THAT(detail_1->name(), HasSubstr(id1));

  auto detail_2 =
      instance_admin_->AsyncGetAppProfile(cq, instance_id, id2).get();
  ASSERT_STATUS_OK(detail_2);
  EXPECT_EQ(detail_2->name(), profile_2->name());
  EXPECT_THAT(detail_2->name(), HasSubstr(instance_id));
  EXPECT_THAT(detail_2->name(), HasSubstr(id2));

  auto profile_updated_future = instance_admin_->AsyncUpdateAppProfile(
      cq, instance_id, id2,
      bigtable::AppProfileUpdateConfig().set_description("new description"));

  auto update_2 = profile_updated_future.get();
  auto detail_2_after_update =
      instance_admin_->AsyncGetAppProfile(cq, instance_id, id2).get();
  ASSERT_STATUS_OK(detail_2_after_update);
  EXPECT_EQ("new description", update_2->description());
  EXPECT_EQ("new description", detail_2_after_update->description());

  ASSERT_STATUS_OK(instance_admin_
                       ->AsyncDeleteAppProfile(cq, instance_id, id1,
                                               /*ignore_warnings=*/true)
                       .get());
  current_profiles = instance_admin_->ListAppProfiles(instance_id);
  ASSERT_STATUS_OK(current_profiles);
  EXPECT_THAT(profile_names(*current_profiles),
              Not(Contains(EndsWith("/appProfiles/" + id1))));
  EXPECT_THAT(profile_names(*current_profiles),
              ContainsOnce(EndsWith("/appProfiles/" + id2)));

  ASSERT_STATUS_OK(instance_admin_
                       ->AsyncDeleteAppProfile(cq, instance_id, id2,
                                               /*ignore_warnings=*/true)
                       .get());
  current_profiles = instance_admin_->ListAppProfiles(instance_id);
  ASSERT_STATUS_OK(current_profiles);
  EXPECT_THAT(profile_names(*current_profiles),
              Not(Contains(EndsWith("/appProfiles/" + id1))));
  EXPECT_THAT(profile_names(*current_profiles),
              Not(Contains(EndsWith("/appProfiles/" + id2))));

  EXPECT_STATUS_OK(instance_admin_->DeleteInstance(instance_id));

  cq.Shutdown();
  pool.join();
}

TEST_F(InstanceAdminAsyncFutureIntegrationTest, SetGetTestIamAPIsTest) {
  std::string id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  // create instance prerequisites for cluster operations
  auto instance_config = IntegrationTestConfig(
      id, zone_a_, bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_details =
      instance_admin_->CreateInstance(instance_config).get();
  ASSERT_STATUS_OK(instance_details);

  auto iam_bindings = google::cloud::IamBindings(
      "roles/bigtable.reader", {"serviceAccount:" + service_account_});

  auto initial_policy =
      instance_admin_->AsyncSetIamPolicy(cq, id, iam_bindings).get();
  ASSERT_STATUS_OK(initial_policy);

  auto fetched_policy = instance_admin_->AsyncGetIamPolicy(cq, id).get();
  ASSERT_STATUS_OK(fetched_policy);

  EXPECT_EQ(initial_policy->version, fetched_policy->version);
  EXPECT_EQ(initial_policy->etag, fetched_policy->etag);

  auto permission_set =
      instance_admin_
          ->AsyncTestIamPermissions(
              cq, id, {"bigtable.tables.list", "bigtable.tables.delete"})
          .get();
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->size());
  EXPECT_STATUS_OK(instance_admin_->DeleteInstance(id));

  cq.Shutdown();
  pool.join();
}

TEST_F(InstanceAdminAsyncFutureIntegrationTest, SetGetTestIamNativeAPIsTest) {
  std::string id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  // create instance prerequisites for cluster operations
  auto instance_config = IntegrationTestConfig(
      id, zone_a_, bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_details =
      instance_admin_->CreateInstance(instance_config).get();
  ASSERT_STATUS_OK(instance_details);

  auto iam_policy = bigtable::IamPolicy({bigtable::IamBinding(
      "roles/bigtable.reader", {"serviceAccount:" + service_account_})});

  auto initial_policy =
      instance_admin_->AsyncSetIamPolicy(cq, id, iam_policy).get();
  ASSERT_STATUS_OK(initial_policy);

  auto fetched_policy = instance_admin_->AsyncGetNativeIamPolicy(cq, id).get();
  ASSERT_STATUS_OK(fetched_policy);

  EXPECT_EQ(initial_policy->version(), fetched_policy->version());
  EXPECT_EQ(initial_policy->etag(), fetched_policy->etag());

  auto permission_set =
      instance_admin_
          ->AsyncTestIamPermissions(
              cq, id, {"bigtable.tables.list", "bigtable.tables.delete"})
          .get();
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->size());
  EXPECT_STATUS_OK(instance_admin_->DeleteInstance(id));

  cq.Shutdown();
  pool.join();
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
