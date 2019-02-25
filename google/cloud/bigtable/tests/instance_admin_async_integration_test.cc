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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace btadmin = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;
using testing::HasSubstr;

namespace {
class InstanceTestEnvironment : public ::testing::Environment {
 public:
  explicit InstanceTestEnvironment(std::string project, std::string zone,
                                   std::string replication_zone) {
    project_id_ = std::move(project);
    zone_ = std::move(zone);
    replication_zone_ = std::move(replication_zone);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& zone() { return zone_; }
  static std::string const& replication_zone() { return replication_zone_; }

 private:
  static std::string project_id_;
  static std::string zone_;
  static std::string replication_zone_;
};

std::string InstanceTestEnvironment::project_id_;
std::string InstanceTestEnvironment::zone_;
std::string InstanceTestEnvironment::replication_zone_;

class InstanceAdminAsyncIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    instance_admin_client_ = bigtable::CreateDefaultInstanceAdminClient(
        InstanceTestEnvironment::project_id(), bigtable::ClientOptions());
    instance_admin_ =
        google::cloud::internal::make_unique<bigtable::InstanceAdmin>(
            instance_admin_client_);
  }

 protected:
  std::unique_ptr<bigtable::InstanceAdmin> instance_admin_;
  std::shared_ptr<bigtable::InstanceAdminClient> instance_admin_client_;
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
    std::string const& id,
    std::string const& zone = InstanceTestEnvironment::zone(),
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

/// @test Verify that Instance async CRUD operations work as expected.
TEST_F(InstanceAdminAsyncIntegrationTest, AsyncCreateListDeleteInstanceTest) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  bigtable::noex::InstanceAdmin admin(instance_admin_client_);
  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  // verify new instance id in list of instances
  std::promise<bigtable::InstanceList> promise_instances_before;
  admin.AsyncListInstances(
      cq, [&promise_instances_before](bigtable::CompletionQueue& cq,
                                      bigtable::InstanceList& response,
                                      grpc::Status const& status) {
        ASSERT_TRUE(status.ok());
        ASSERT_TRUE(response.failed_locations.empty());
        promise_instances_before.set_value(response);
      });
  auto response_instances_before =
      promise_instances_before.get_future().get().instances;
  ASSERT_FALSE(IsInstancePresent(response_instances_before, instance_id))
      << "Instance (" << instance_id << ") already exists."
      << " This is unexpected, as the instance ids are"
      << " generated at random.";

  // create instance
  auto config = IntegrationTestConfig(instance_id);
  std::promise<btadmin::Instance> create_promise;
  admin.AsyncCreateInstance(
      cq,
      [&create_promise](google::cloud::bigtable::CompletionQueue&,
                        btadmin::Instance& response, grpc::Status& status) {
        ASSERT_TRUE(status.ok());
        create_promise.set_value(std::move(response));
      },
      config);
  auto instance = create_promise.get_future().get();
  auto instances_current = instance_admin_->ListInstances();
  ASSERT_STATUS_OK(instances_current);
  EXPECT_TRUE(instances_current->failed_locations.empty());
  EXPECT_TRUE(IsInstancePresent(instances_current->instances, instance.name()));

  // Get instance
  std::promise<btadmin::Instance> done;
  admin.AsyncGetInstance(
      cq,
      [&done](google::cloud::bigtable::CompletionQueue& cq,
              btadmin::Instance& instance, grpc::Status const& status) {
        done.set_value(std::move(instance));
      },
      instance_id);
  auto instance_result = done.get_future().get();
  auto const npos = std::string::npos;
  EXPECT_NE(npos, instance_result.name().find(instance_admin_->project_name()));
  EXPECT_NE(npos, instance_result.name().find(instance_id));

  // Delete instance
  std::promise<void> promise_delete_instance;
  admin.AsyncDeleteInstance(
      cq,
      [&promise_delete_instance](google::cloud::bigtable::CompletionQueue& cq,
                                 grpc::Status const& status) {
        promise_delete_instance.set_value();
      },
      instance_id);
  promise_delete_instance.get_future().get();
  auto instances_after_delete = instance_admin_->ListInstances();
  ASSERT_STATUS_OK(instances_after_delete);
  EXPECT_TRUE(instances_after_delete->failed_locations.empty());
  EXPECT_TRUE(IsInstancePresent(instances_current->instances, instance.name()));
  EXPECT_FALSE(
      IsInstancePresent(instances_after_delete->instances, instance.name()));

  cq.Shutdown();
  pool.join();
}

/// @test Verify that cluster async CRUD operations work as expected.
TEST_F(InstanceAdminAsyncIntegrationTest, AsyncCreateListDeleteClusterTest) {
  std::string id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string cluster_id_str = id + "-cl2";

  // create instance prerequisites for cluster operations
  bigtable::InstanceId instance_id(id);
  auto instance_config =
      IntegrationTestConfig(id, InstanceTestEnvironment::zone(),
                            bigtable::InstanceConfig::PRODUCTION, 3);

  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });
  bigtable::noex::InstanceAdmin admin(instance_admin_client_);

  std::promise<btadmin::Instance> create_instance_promise;
  admin.AsyncCreateInstance(
      cq,
      [&create_instance_promise](google::cloud::bigtable::CompletionQueue&,
                                 btadmin::Instance& response,
                                 grpc::Status& status) {
        ASSERT_TRUE(status.ok());
        create_instance_promise.set_value(std::move(response));
      },
      instance_config);
  auto instance_details = create_instance_promise.get_future().get();

  // create cluster
  std::promise<bigtable::ClusterList> clusters_before_promise;
  admin.AsyncListClusters(
      cq,
      [&clusters_before_promise](bigtable::CompletionQueue&,
                                 bigtable::ClusterList& response,
                                 grpc::Status& status) {
        ASSERT_TRUE(status.ok());
        ASSERT_TRUE(response.failed_locations.empty());
        clusters_before_promise.set_value(response);
      },
      id);
  auto clusters_before = clusters_before_promise.get_future().get().clusters;
  ASSERT_FALSE(IsClusterPresent(clusters_before, cluster_id_str))
      << "Cluster (" << cluster_id_str << ") already exists."
      << " This is unexpected, as the cluster ids are"
      << " generated at random.";
  bigtable::ClusterId cluster_id(cluster_id_str);
  std::promise<btadmin::Cluster> create_promise;
  auto cluster_config =
      bigtable::ClusterConfig(InstanceTestEnvironment::replication_zone(), 3,
                              bigtable::ClusterConfig::HDD);
  admin.AsyncCreateCluster(
      cq,
      [&create_promise](google::cloud::bigtable::CompletionQueue&,
                        btadmin::Cluster& response, grpc::Status& status) {
        ASSERT_TRUE(status.ok());
        create_promise.set_value(std::move(response));
      },
      cluster_config, instance_id, cluster_id);
  auto cluster = create_promise.get_future().get();
  std::promise<bigtable::ClusterList> clusters_after_promise;
  admin.AsyncListClusters(
      cq,
      [&clusters_after_promise](bigtable::CompletionQueue&,
                                bigtable::ClusterList& response,
                                grpc::Status& status) {
        ASSERT_TRUE(status.ok());
        ASSERT_TRUE(response.failed_locations.empty());
        clusters_after_promise.set_value(response);
      },
      id);
  auto clusters_after = clusters_after_promise.get_future().get().clusters;
  EXPECT_FALSE(IsClusterPresent(clusters_before, cluster.name()));
  EXPECT_TRUE(IsClusterPresent(clusters_after, cluster.name()));

  // Get cluster
  std::promise<btadmin::Cluster> done;
  admin.AsyncGetCluster(
      cq,
      [&done](google::cloud::bigtable::CompletionQueue& cq,
              btadmin::Cluster& cluster, grpc::Status const& status) {
        done.set_value(std::move(cluster));
      },
      instance_id, cluster_id);
  auto cluster_result = done.get_future().get();
  std::string cluster_name_prefix =
      instance_admin_->project_name() + "/instances/" + id + "/clusters/";
  EXPECT_EQ(cluster_name_prefix + cluster_id.get(), cluster_result.name());

  // Delete cluster
  std::promise<void> promise_delete_cluster;
  admin.AsyncDeleteCluster(
      cq,
      [&promise_delete_cluster](google::cloud::bigtable::CompletionQueue& cq,
                                grpc::Status const& status) {
        promise_delete_cluster.set_value();
      },
      instance_id, cluster_id);
  promise_delete_cluster.get_future().get();
  auto clusters_after_delete = instance_admin_->ListClusters(id);
  ASSERT_STATUS_OK(clusters_after_delete);
  ASSERT_STATUS_OK(instance_admin_->DeleteInstance(id));
  EXPECT_TRUE(IsClusterPresent(
      clusters_after, instance_details.name() + "/clusters/" + id + "-cl2"));
  EXPECT_FALSE(
      IsClusterPresent(clusters_after_delete->clusters,
                       instance_details.name() + "/clusters/" + id + "-cl2"));

  cq.Shutdown();
  pool.join();
}

/// @test Verify that AppProfile async CRUD operations work as expected.
TEST_F(InstanceAdminAsyncIntegrationTest, AsyncCreateListDeleteAppProfile) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  auto instance_config = IntegrationTestConfig(
      instance_id, "us-central1-c", bigtable::InstanceConfig::PRODUCTION, 3);
  auto future = instance_admin_->CreateInstance(instance_config);
  auto actual = future.get();
  EXPECT_STATUS_OK(actual);
  // Wait for instance creation
  ASSERT_THAT(actual->name(), HasSubstr(instance_id));

  bigtable::noex::InstanceAdmin admin(instance_admin_client_);
  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  std::string id1 =
      "profile-" + google::cloud::internal::Sample(
                       generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  std::string id2 =
      "profile-" + google::cloud::internal::Sample(
                       generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  std::promise<std::vector<btadmin::AppProfile>> initial_appprofiles_promise;
  admin.AsyncListAppProfiles(
      cq,
      [&initial_appprofiles_promise](bigtable::CompletionQueue&,
                                     std::vector<btadmin::AppProfile>& response,
                                     grpc::Status& status) {
        ASSERT_TRUE(status.ok());
        initial_appprofiles_promise.set_value(response);
      },
      instance_id);
  auto initial_profiles = initial_appprofiles_promise.get_future().get();

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

  // Create First profile
  std::promise<btadmin::AppProfile> promise_create_first_profile;
  admin.AsyncCreateAppProfile(
      cq,
      [&promise_create_first_profile](
          google::cloud::bigtable::CompletionQueue& cq,
          btadmin::AppProfile& app_profiles, grpc::Status const& status) {
        promise_create_first_profile.set_value(std::move(app_profiles));
      },
      bigtable::InstanceId(instance_id),
      bigtable::AppProfileConfig::MultiClusterUseAny(
          bigtable::AppProfileId(id1)));
  auto response_create_first_profile =
      promise_create_first_profile.get_future().get();

  // Create second profile
  std::promise<btadmin::AppProfile> promise_create_second_profile;
  admin.AsyncCreateAppProfile(
      cq,
      [&promise_create_second_profile](
          google::cloud::bigtable::CompletionQueue& cq,
          btadmin::AppProfile& app_profiles, grpc::Status const& status) {
        promise_create_second_profile.set_value(std::move(app_profiles));
      },
      bigtable::InstanceId(instance_id),
      bigtable::AppProfileConfig::MultiClusterUseAny(
          bigtable::AppProfileId(id2)));
  auto response_create_second_profile =
      promise_create_second_profile.get_future().get();

  std::promise<std::vector<btadmin::AppProfile>>
      after_second_appprofiles_promise;
  admin.AsyncListAppProfiles(
      cq,
      [&after_second_appprofiles_promise](
          bigtable::CompletionQueue&,
          std::vector<btadmin::AppProfile>& response, grpc::Status& status) {
        ASSERT_TRUE(status.ok());
        after_second_appprofiles_promise.set_value(response);
      },
      instance_id);
  auto current_profiles = after_second_appprofiles_promise.get_future().get();
  EXPECT_EQ(1U, count_matching_profiles(id1, current_profiles));
  EXPECT_EQ(1U, count_matching_profiles(id2, current_profiles));

  std::promise<btadmin::AppProfile> promise_get_profile_1;
  admin.AsyncGetAppProfile(
      cq,
      [&promise_get_profile_1](google::cloud::bigtable::CompletionQueue& cq,
                               btadmin::AppProfile& app_profiles,
                               grpc::Status const& status) {
        promise_get_profile_1.set_value(std::move(app_profiles));
      },
      bigtable::InstanceId(instance_id), bigtable::AppProfileId(id1));
  auto detail_1 = promise_get_profile_1.get_future().get();
  EXPECT_EQ(detail_1.name(), response_create_first_profile.name());
  EXPECT_THAT(detail_1.name(), HasSubstr(instance_id));
  EXPECT_THAT(detail_1.name(), HasSubstr(id1));

  std::promise<btadmin::AppProfile> promise_get_profile_2;
  admin.AsyncGetAppProfile(
      cq,
      [&promise_get_profile_2](google::cloud::bigtable::CompletionQueue& cq,
                               btadmin::AppProfile& app_profiles,
                               grpc::Status const& status) {
        promise_get_profile_2.set_value(std::move(app_profiles));
      },
      bigtable::InstanceId(instance_id), bigtable::AppProfileId(id2));
  auto detail_2 = promise_get_profile_2.get_future().get();
  EXPECT_EQ(detail_2.name(), response_create_second_profile.name());
  EXPECT_THAT(detail_2.name(), HasSubstr(instance_id));
  EXPECT_THAT(detail_2.name(), HasSubstr(id2));

  // update profile
  std::promise<btadmin::AppProfile> update_promise;
  admin.AsyncUpdateAppProfile(
      cq,
      [&update_promise](google::cloud::bigtable::CompletionQueue&,
                        btadmin::AppProfile& response, grpc::Status& status) {
        ASSERT_TRUE(status.ok());
        update_promise.set_value(std::move(response));
      },
      bigtable::InstanceId(instance_id), bigtable::AppProfileId(id2),
      bigtable::AppProfileUpdateConfig().set_description("new description"));
  auto update_2 = update_promise.get_future().get();

  std::promise<btadmin::AppProfile> promise_get_profile_after_update;
  admin.AsyncGetAppProfile(
      cq,
      [&promise_get_profile_after_update](
          google::cloud::bigtable::CompletionQueue& cq,
          btadmin::AppProfile& app_profiles, grpc::Status const& status) {
        promise_get_profile_after_update.set_value(std::move(app_profiles));
      },
      bigtable::InstanceId(instance_id), bigtable::AppProfileId(id2));
  auto detail_2_after_update =
      promise_get_profile_after_update.get_future().get();
  EXPECT_EQ("new description", update_2.description());
  EXPECT_EQ("new description", detail_2_after_update.description());

  // delete first profile
  std::promise<void> promise_delete_first_profile;
  admin.AsyncDeleteAppProfile(cq,
                              [&promise_delete_first_profile](
                                  google::cloud::bigtable::CompletionQueue& cq,
                                  grpc::Status const& status) {
                                promise_delete_first_profile.set_value();
                              },
                              bigtable::InstanceId(instance_id),
                              bigtable::AppProfileId(id1));

  promise_delete_first_profile.get_future().get();

  std::promise<std::vector<btadmin::AppProfile>>
      after_delete_appprofiles_promise;
  admin.AsyncListAppProfiles(
      cq,
      [&after_delete_appprofiles_promise](
          bigtable::CompletionQueue&,
          std::vector<btadmin::AppProfile>& response, grpc::Status& status) {
        ASSERT_TRUE(status.ok());
        after_delete_appprofiles_promise.set_value(response);
      },
      instance_id);
  current_profiles = after_delete_appprofiles_promise.get_future().get();
  EXPECT_EQ(0U, count_matching_profiles(id1, current_profiles));
  EXPECT_EQ(1U, count_matching_profiles(id2, current_profiles));

  // delete second profile
  std::promise<void> promise_delete_second_profile;
  admin.AsyncDeleteAppProfile(cq,
                              [&promise_delete_second_profile](
                                  google::cloud::bigtable::CompletionQueue& cq,
                                  grpc::Status const& status) {
                                promise_delete_second_profile.set_value();
                              },
                              bigtable::InstanceId(instance_id),
                              bigtable::AppProfileId(id2));

  promise_delete_second_profile.get_future().get();

  std::promise<std::vector<btadmin::AppProfile>>
      after_delete_second_appprofiles_promise;
  admin.AsyncListAppProfiles(
      cq,
      [&after_delete_second_appprofiles_promise](
          bigtable::CompletionQueue&,
          std::vector<btadmin::AppProfile>& response, grpc::Status& status) {
        ASSERT_TRUE(status.ok());
        after_delete_second_appprofiles_promise.set_value(response);
      },
      instance_id);
  current_profiles = after_delete_second_appprofiles_promise.get_future().get();
  EXPECT_EQ(0U, count_matching_profiles(id1, current_profiles));
  EXPECT_EQ(0U, count_matching_profiles(id2, current_profiles));

  ASSERT_STATUS_OK(instance_admin_->DeleteInstance(instance_id));

  cq.Shutdown();
  pool.join();
}

/// @test Verify that Async IAM Policy APIs work as expected.
TEST_F(InstanceAdminAsyncIntegrationTest, AsyncSetGetTestIamAPIsTest) {
  std::string id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  // create instance prerequisites for cluster operations
  bigtable::InstanceId instance_id(id);
  auto instance_config = IntegrationTestConfig(
      id, "us-central1-f", bigtable::InstanceConfig::PRODUCTION, 3);
  auto instance_details =
      instance_admin_->CreateInstance(instance_config).get();

  bigtable::noex::InstanceAdmin admin(instance_admin_client_);
  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  std::string resource = id;
  auto iam_bindings = google::cloud::IamBindings(
      "writer", {"abc@gmail.com", "xyz@gmail.com", "pqr@gmail.com"});

  auto initial_policy =
      instance_admin_->SetIamPolicy(id, iam_bindings, "test-tag");
  ASSERT_STATUS_OK(initial_policy);

  // Get policy
  std::promise<google::cloud::IamPolicy> promise_get_policy;
  admin.AsyncGetIamPolicy(
      id, cq,
      [&promise_get_policy](google::cloud::bigtable::CompletionQueue& cq,
                            google::cloud::IamPolicy policy,
                            grpc::Status const& status) {
        ASSERT_TRUE(status.ok());
        promise_get_policy.set_value(std::move(policy));
      });
  auto response_get_policy = promise_get_policy.get_future().get();

  EXPECT_EQ(initial_policy->version, response_get_policy.version);
  EXPECT_EQ(initial_policy->etag, response_get_policy.etag);

  auto permission_set = instance_admin_->TestIamPermissions(
      id, {"bigtable.tables.list", "bigtable.tables.delete"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2U, permission_set->size());

  cq.Shutdown();
  pool.join();
}

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  if (argc != 4) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    // Show usage if number of arguments is invalid.
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project_id> <zone> <replication_zone>\n";
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const zone = argv[2];
  std::string const replication_zone = argv[3];
  (void)::testing::AddGlobalTestEnvironment(
      new InstanceTestEnvironment(project_id, zone, replication_zone));

  return RUN_ALL_TESTS();
}
