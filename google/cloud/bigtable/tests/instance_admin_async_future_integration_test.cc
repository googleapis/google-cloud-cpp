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
#include "google/cloud/future.h"
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

class InstanceAdminAsyncFutureIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto instance_admin_client = bigtable::CreateDefaultInstanceAdminClient(
        InstanceTestEnvironment::project_id(), bigtable::ClientOptions());
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
  EXPECT_TRUE(instance_list.failed_locations.empty())
      << "The Cloud Bigtable service (or emulator) reports that it could not"
      << " retrieve the information for some locations. This is typically due"
      << " to an outage or some other transient condition.";
  ASSERT_FALSE(IsInstancePresent(instance_list.instances, instance_id))
      << "Instance (" << instance_id << ") already exists."
      << " This is unexpected, as the instance ids are"
      << " generated at random.";

  // create instance
  auto config = IntegrationTestConfig(instance_id);
  auto instance = instance_admin_->CreateInstance(config).get();
  auto async_instances_current = instance_admin_->AsyncListInstances(cq).get();
  EXPECT_TRUE(
      IsInstancePresent(async_instances_current.instances, instance->name()));

  // Get instance
  google::cloud::future<btadmin::Instance> fut =
      instance_admin_->AsyncGetInstance(cq, instance_id);
  auto instance_check = fut.get();
  auto const npos = std::string::npos;
  EXPECT_NE(npos, instance_check.name().find(instance_admin_->project_name()));
  EXPECT_NE(npos, instance_check.name().find(instance_id));

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
  auto instances_after_delete = instance_admin_->AsyncListInstances(cq).get();
  EXPECT_TRUE(IsInstancePresent(async_instances_current.instances,
                                instance_copy->name()));
  EXPECT_FALSE(
      IsInstancePresent(instances_after_delete.instances, instance->name()));

  cq.Shutdown();
  pool.join();
}

/// @test Verify that cluster async future CRUD operations work as expected.
TEST_F(InstanceAdminAsyncFutureIntegrationTest,
       CreateListGetDeleteClusterTest) {
  std::string id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string cluster_id_str = id + "-cl2";

  // create instance prerequisites for cluster operations
  bigtable::InstanceId instance_id(id);
  auto instance_config = IntegrationTestConfig(
      id, "us-central1-f", bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_details =
      instance_admin_->CreateInstance(instance_config).get();

  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  // Make an asynchronous request, but immediately block because this is just a
  // test.
  auto clusters_list_before = instance_admin_->AsyncListClusters(cq, id).get();
  EXPECT_TRUE(clusters_list_before.failed_locations.empty())
      << "The Cloud Bigtable service (or emulator) reports that it could not"
      << " retrieve the information for some locations. This is typically due"
      << " to an outage or some other transient condition.";
  ASSERT_FALSE(IsIdOrNamePresentInClusterList(clusters_list_before.clusters,
                                              cluster_id_str));
  // create cluster
  bigtable::ClusterId cluster_id(cluster_id_str);
  auto cluster_config =
      bigtable::ClusterConfig("us-central1-b", 3, bigtable::ClusterConfig::HDD);
  auto cluster =
      instance_admin_->CreateCluster(cluster_config, instance_id, cluster_id)
          .get();
  ASSERT_FALSE(cluster->name().empty());
  auto clusters_list_after = instance_admin_->AsyncListClusters(cq, id).get();
  EXPECT_TRUE(clusters_list_after.failed_locations.empty())
      << "The Cloud Bigtable service (or emulator) reports that it could not"
      << " retrieve the information for some locations. This is typically due"
      << " to an outage or some other transient condition.";
  EXPECT_FALSE(
      IsClusterPresent(clusters_list_before.clusters, cluster->name()));
  EXPECT_TRUE(IsClusterPresent(clusters_list_after.clusters, cluster->name()));
  EXPECT_TRUE(IsIdOrNamePresentInClusterList(clusters_list_after.clusters,
                                             cluster_id_str));

  // Get cluster
  google::cloud::future<btadmin::Cluster> fut =
      instance_admin_->AsyncGetCluster(cq, instance_id, cluster_id);
  auto cluster_check = fut.get();
  std::string cluster_name_prefix =
      instance_admin_->project_name() + "/instances/" + id + "/clusters/";
  EXPECT_EQ(cluster_name_prefix + cluster_id.get(), cluster_check.name());

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
  ASSERT_STATUS_OK(instance_admin_->DeleteCluster(std::move(instance_id),
                                                  std::move(cluster_id)));
  auto clusters_list_after_delete =
      instance_admin_->AsyncListClusters(cq, id).get();
  ASSERT_STATUS_OK(instance_admin_->DeleteInstance(id));
  EXPECT_TRUE(
      IsClusterPresent(clusters_list_after.clusters,
                       instance_details->name() + "/clusters/" + id + "-cl2"));
  EXPECT_FALSE(
      IsClusterPresent(clusters_list_after_delete.clusters,
                       instance_details->name() + "/clusters/" + id + "-cl2"));
  EXPECT_FALSE(
      IsClusterPresent(clusters_list_after_delete.clusters, cluster->name()));
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
      id1, "us-central1-c", bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_config2 = IntegrationTestConfig(
      id2, "us-central1-f", bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance1 = instance_admin_->CreateInstance(instance_config1);
  auto instance2 = instance_admin_->CreateInstance(instance_config2);
  // Wait for instance creation

  auto instance1_name = instance1.get()->name();
  auto instance2_name = instance2.get()->name();
  ASSERT_THAT(instance1_name, HasSubstr(id1));
  ASSERT_THAT(instance2_name, HasSubstr(id2));

  // Make an asynchronous request, but immediately block because this is just a
  // test.
  auto clusters_list = instance_admin_->AsyncListClusters(cq).get();
  for (auto const& cluster : clusters_list.clusters) {
    EXPECT_NE(std::string::npos,
              cluster.name().find(instance_admin_->project_name()));
  }
  EXPECT_FALSE(clusters_list.clusters.empty());

  EXPECT_TRUE(
      IsIdOrNamePresentInClusterList(clusters_list.clusters, instance1_name));
  EXPECT_TRUE(
      IsIdOrNamePresentInClusterList(clusters_list.clusters, instance2_name));

  ASSERT_STATUS_OK(instance_admin_->DeleteInstance(id1));
  ASSERT_STATUS_OK(instance_admin_->DeleteInstance(id2));

  cq.Shutdown();
  pool.join();
}

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  if (argc != 2) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    // Show usage if number of arguments is invalid.
    std::cerr << "Usage: " << cmd.substr(last_slash + 1) << " <project_id>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  (void)::testing::AddGlobalTestEnvironment(
      new InstanceTestEnvironment(project_id));

  return RUN_ALL_TESTS();
}
