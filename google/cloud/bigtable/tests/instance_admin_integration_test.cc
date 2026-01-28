// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/admin/bigtable_instance_admin_client.h"
#include "google/cloud/bigtable/app_profile_config.h"
#include "google/cloud/bigtable/iam_binding.h"
#include "google/cloud/bigtable/iam_policy.h"
#include "google/cloud/bigtable/instance_config.h"
#include "google/cloud/bigtable/instance_update_config.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/location.h"
#include "google/cloud/project.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::RandomInstanceId;
using ::google::cloud::internal::GetEnv;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Not;
namespace btadmin = ::google::bigtable::admin::v2;

class InstanceAdminIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  void SetUp() override {
    auto emulator_present =
        GetEnv("BIGTABLE_INSTANCE_ADMIN_EMULATOR_HOST").has_value();
    auto run_prod_tests =
        GetEnv("ENABLE_BIGTABLE_ADMIN_INTEGRATION_TESTS").value_or("") == "yes";
    if (!emulator_present && !run_prod_tests) {
      GTEST_SKIP();
    }

    project_id_ = GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty());
    zone_a_ = GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A").value_or("");
    ASSERT_FALSE(zone_a_.empty());
    zone_b_ = GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B").value_or("");
    ASSERT_FALSE(zone_b_.empty());
    service_account_ =
        GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT").value_or("");
    ASSERT_FALSE(service_account_.empty());

    instance_admin_ =
        std::make_unique<bigtable_admin::BigtableInstanceAdminClient>(
            bigtable_admin::MakeBigtableInstanceAdminConnection());
  }
  std::string project_id_;
  std::string zone_a_;
  std::string zone_b_;
  std::string service_account_;
  std::unique_ptr<bigtable_admin::BigtableInstanceAdminClient> instance_admin_;
  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();
};

bool IsInstancePresent(
    google::protobuf::RepeatedPtrField<btadmin::Instance> const& instances,
    std::string const& instance_name) {
  return google::cloud::internal::ContainsIf(
      instances, [&instance_name](btadmin::Instance const& i) {
        return i.name() == instance_name;
      });
}

bool IsClusterPresent(
    google::protobuf::RepeatedPtrField<btadmin::Cluster> const& clusters,
    std::string const& cluster_name) {
  return google::cloud::internal::ContainsIf(
      clusters, [&cluster_name](btadmin::Cluster const& i) {
        return i.name() == cluster_name;
      });
}

bigtable::InstanceConfig IntegrationTestConfig(
    std::string const& instance_id, std::string const& zone,
    bigtable::InstanceConfig::InstanceType instance_type =
        bigtable::InstanceConfig::DEVELOPMENT,
    int32_t serve_node = 0) {
  // The description cannot exceed 30 characters
  auto const display_name = ("IT " + instance_id).substr(0, 30);
  auto cluster_config =
      bigtable::ClusterConfig(zone, serve_node, bigtable::ClusterConfig::HDD);
  bigtable::InstanceConfig config(instance_id, display_name,
                                  {{instance_id + "-c1", cluster_config}});
  config.set_type(instance_type);
  return config;
}

/// @test Verify that default InstanceAdmin::ListClusters works as expected.
TEST_F(InstanceAdminIntegrationTest, ListAllClustersTest) {
  auto const id_1 = RandomInstanceId(generator_);
  auto const id_2 = RandomInstanceId(generator_);
  auto const name_1 = bigtable::InstanceName(project_id_, id_1);
  auto const name_2 = bigtable::InstanceName(project_id_, id_2);

  auto config_1 = IntegrationTestConfig(
      id_1, zone_a_, bigtable::InstanceConfig::PRODUCTION, 3);
  auto config_2 = IntegrationTestConfig(
      id_2, zone_b_, bigtable::InstanceConfig::PRODUCTION, 3);

  auto create_request_1 = config_1.as_proto();
  create_request_1.set_parent(Project(project_id_).FullName());
  auto create_request_2 = config_2.as_proto();
  create_request_2.set_parent(Project(project_id_).FullName());

  auto instance_1_fut = instance_admin_->CreateInstance(create_request_1);
  auto instance_2_fut = instance_admin_->CreateInstance(create_request_2);

  // Wait for instance creation
  auto instance_1 = instance_1_fut.get();
  auto instance_2 = instance_2_fut.get();
  ASSERT_STATUS_OK(instance_1);
  ASSERT_STATUS_OK(instance_2);

  EXPECT_EQ(instance_1->name(), name_1);
  EXPECT_EQ(instance_2->name(), name_2);

  auto clusters = instance_admin_->ListClusters(instance_1->name());
  ASSERT_STATUS_OK(clusters);
  for (auto const& cluster : clusters->clusters()) {
    EXPECT_NE(std::string::npos,
              cluster.name().find("projects/" + project_id_));
  }
  EXPECT_FALSE(clusters->clusters().empty());

  EXPECT_STATUS_OK(instance_admin_->DeleteInstance(name_1));
  EXPECT_STATUS_OK(instance_admin_->DeleteInstance(name_2));
}

/// @test Verify that AppProfile CRUD operations work as expected.
TEST_F(InstanceAdminIntegrationTest, CreateListGetDeleteAppProfile) {
  auto const instance_id = RandomInstanceId(generator_);
  auto const instance_name = bigtable::InstanceName(project_id_, instance_id);

  auto config = IntegrationTestConfig(instance_id, zone_a_,
                                      bigtable::InstanceConfig::PRODUCTION, 3);
  auto create_request = config.as_proto();
  create_request.set_parent(Project(project_id_).FullName());
  auto instance_fut = instance_admin_->CreateInstance(create_request);
  // Wait for instance creation
  auto instance = instance_fut.get();
  ASSERT_STATUS_OK(instance);
  ASSERT_EQ(instance->name(), instance_name);

  auto const id_1 =
      "profile-" + google::cloud::internal::Sample(
                       generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  auto const id_2 =
      "profile-" + google::cloud::internal::Sample(
                       generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");
  auto const name_1 = bigtable::AppProfileName(project_id_, instance_id, id_1);
  auto const name_2 = bigtable::AppProfileName(project_id_, instance_id, id_2);

  // Simplify writing the rest of the test.
  auto profile_names = [](StreamRange<btadmin::AppProfile>& list)
      -> StatusOr<std::vector<std::string>> {
    std::vector<std::string> names;
    for (auto const& p : list) {
      if (!p) return p.status();
      names.push_back(p->name());
    }
    return names;
  };

  auto profiles = instance_admin_->ListAppProfiles(instance_name);
  auto names = profile_names(profiles);
  ASSERT_STATUS_OK(names);
  EXPECT_THAT(*names, Not(Contains(name_1)));
  EXPECT_THAT(*names, Not(Contains(name_2)));

  auto create_profile_request =
      bigtable::AppProfileConfig::MultiClusterUseAny(id_1).as_proto();
  create_profile_request.set_parent(instance_name);
  create_profile_request.set_app_profile_id(id_1);
  auto profile_1 = instance_admin_->CreateAppProfile(create_profile_request);
  ASSERT_STATUS_OK(profile_1);
  EXPECT_EQ(profile_1->name(), name_1);

  create_profile_request =
      bigtable::AppProfileConfig::MultiClusterUseAny(id_2).as_proto();
  create_profile_request.set_parent(instance_name);
  create_profile_request.set_app_profile_id(id_2);
  auto profile_2 = instance_admin_->CreateAppProfile(create_profile_request);
  ASSERT_STATUS_OK(profile_2);
  EXPECT_EQ(profile_2->name(), name_2);

  profiles = instance_admin_->ListAppProfiles(instance_name);
  names = profile_names(profiles);
  ASSERT_STATUS_OK(names);
  EXPECT_THAT(*names, Contains(name_1).Times(1));
  EXPECT_THAT(*names, Contains(name_2).Times(1));

  profile_1 = instance_admin_->GetAppProfile(
      AppProfileName(project_id_, instance_id, id_1));
  ASSERT_STATUS_OK(profile_1);
  EXPECT_EQ(profile_1->name(), name_1);

  profile_2 = instance_admin_->GetAppProfile(
      AppProfileName(project_id_, instance_id, id_2));
  ASSERT_STATUS_OK(profile_2);
  EXPECT_EQ(profile_2->name(), name_2);

  profile_2->set_description("new description");
  google::protobuf::FieldMask field_mask;
  *field_mask.add_paths() = "description";
  profile_2 = instance_admin_->UpdateAppProfile(*profile_2, field_mask).get();
  ASSERT_STATUS_OK(profile_2);
  EXPECT_EQ("new description", profile_2->description());

  profile_2 = instance_admin_->GetAppProfile(
      AppProfileName(project_id_, instance_id, id_2));
  ASSERT_STATUS_OK(profile_2);
  EXPECT_EQ("new description", profile_2->description());

  ASSERT_STATUS_OK(instance_admin_->DeleteAppProfile(
      AppProfileName(project_id_, instance_id, id_1), true));
  profiles = instance_admin_->ListAppProfiles(instance_name);
  names = profile_names(profiles);
  ASSERT_STATUS_OK(names);
  EXPECT_THAT((*names), Not(Contains(name_1)));
  EXPECT_THAT(*names, Contains(name_2).Times(1));

  ASSERT_STATUS_OK(instance_admin_->DeleteAppProfile(
      AppProfileName(project_id_, instance_id, id_2), true));
  profiles = instance_admin_->ListAppProfiles(instance_name);
  names = profile_names(profiles);
  ASSERT_STATUS_OK(names);
  EXPECT_THAT(*names, Not(Contains(name_1)));
  EXPECT_THAT(*names, Not(Contains(name_2)));

  ASSERT_STATUS_OK(instance_admin_->DeleteInstance(instance_name));
}

/// @test Verify that Instance CRUD operations work as expected.
TEST_F(InstanceAdminIntegrationTest, CreateListGetDeleteInstanceTest) {
  auto const instance_id = RandomInstanceId(generator_);
  auto const instance_name = bigtable::InstanceName(project_id_, instance_id);

  // Create instance
  auto config = IntegrationTestConfig(instance_id, zone_a_);
  auto create_request = config.as_proto();
  create_request.set_parent(Project(project_id_).FullName());
  auto instance = instance_admin_->CreateInstance(create_request).get();
  ASSERT_STATUS_OK(instance);

  auto project = google::cloud::Project(project_id_);
  auto const zone_a = Location(project.FullName(), zone_a_);
  auto const zone_b = Location(project.FullName(), zone_b_);

  // List instances
  auto instances = instance_admin_->ListInstances(project.FullName());
  ASSERT_STATUS_OK(instances);
  // If either zone_a_ or zone_b_ are in the list of failed locations then we
  // cannot proceed.
  ASSERT_THAT(
      instances->failed_locations(),
      Not(AnyOf(Contains(zone_a.FullName()), Contains(zone_b.FullName()))));
  EXPECT_TRUE(IsInstancePresent(instances->instances(), instance->name()));

  // Get instance
  instance = instance_admin_->GetInstance(instance_name);
  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(instance->name(), instance_name);

  // Update instance
  bigtable::InstanceUpdateConfig instance_update_config(std::move(*instance));
  auto const updated_display_name = instance_id.substr(0, 22) + " updated";
  instance_update_config.set_display_name(updated_display_name);
  instance =
      instance_admin_->PartialUpdateInstance(instance_update_config.as_proto())
          .get();
  ASSERT_STATUS_OK(instance);

  // Verify update
  instance = instance_admin_->GetInstance(instance_name);
  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(updated_display_name, instance->display_name());

  // Delete instance
  ASSERT_STATUS_OK(instance_admin_->DeleteInstance(instance_name));

  // Verify delete
  instances = instance_admin_->ListInstances(project.FullName());
  ASSERT_STATUS_OK(instances);
  // If either zone_a_ or zone_b_ are in the list of failed locations then we
  // cannot proceed.
  ASSERT_THAT(
      instances->failed_locations(),
      Not(AnyOf(Contains(zone_a.FullName()), Contains(zone_b.FullName()))));
  EXPECT_FALSE(IsInstancePresent(instances->instances(), instance_name));
}

/// @test Verify that cluster CRUD operations work as expected.
TEST_F(InstanceAdminIntegrationTest, CreateListGetDeleteClusterTest) {
  auto const instance_id = RandomInstanceId(generator_);
  auto const instance_name = InstanceName(project_id_, instance_id);
  auto const cluster_id = instance_id + "-cl2";
  auto const cluster_name =
      bigtable::ClusterName(project_id_, instance_id, cluster_id);

  // Create instance prerequisites for cluster operations
  auto config = IntegrationTestConfig(instance_id, zone_a_,
                                      bigtable::InstanceConfig::PRODUCTION, 3);
  auto create_request = config.as_proto();
  create_request.set_parent(Project(project_id_).FullName());
  auto instance = instance_admin_->CreateInstance(create_request).get();
  ASSERT_STATUS_OK(instance);

  // Create cluster
  auto cluster_config =
      bigtable::ClusterConfig(zone_b_, 3, bigtable::ClusterConfig::HDD);
  auto cluster =
      instance_admin_
          ->CreateCluster(instance_name, cluster_id, cluster_config.as_proto())
          .get();
  ASSERT_STATUS_OK(cluster);
  EXPECT_EQ(3, cluster->serve_nodes());

  // Verify create
  auto clusters = instance_admin_->ListClusters(instance_name);
  ASSERT_STATUS_OK(clusters);
  EXPECT_TRUE(IsClusterPresent(clusters->clusters(), cluster->name()));

  // Get cluster
  cluster = instance_admin_->GetCluster(cluster_name);
  ASSERT_STATUS_OK(cluster);
  EXPECT_EQ(cluster_name, cluster->name());

  // Update cluster
  cluster->set_serve_nodes(4);
  cluster->clear_state();
  bigtable::ClusterConfig updated_cluster_config(std::move(*cluster));
  cluster =
      instance_admin_->UpdateCluster(updated_cluster_config.as_proto()).get();
  ASSERT_STATUS_OK(cluster);

  // Verify update
  cluster = instance_admin_->GetCluster(cluster_name);
  ASSERT_STATUS_OK(cluster);
  EXPECT_EQ(4, cluster->serve_nodes());

  // Delete cluster
  ASSERT_STATUS_OK(instance_admin_->DeleteCluster(cluster_name));

  // Verify delete
  clusters = instance_admin_->ListClusters(instance_name);
  ASSERT_STATUS_OK(clusters);
  EXPECT_FALSE(IsClusterPresent(clusters->clusters(), cluster_name));

  // Delete instance
  ASSERT_STATUS_OK(instance_admin_->DeleteInstance(instance_name));
}

/// @test Verify that IAM Policy Native APIs work as expected.
TEST_F(InstanceAdminIntegrationTest, SetGetTestIamNativeAPIsTest) {
  auto const instance_id = RandomInstanceId(generator_);
  auto const instance_name = InstanceName(project_id_, instance_id);

  // create instance prerequisites for cluster operations
  auto config = IntegrationTestConfig(instance_id, zone_a_,
                                      bigtable::InstanceConfig::PRODUCTION, 3);
  auto create_request = config.as_proto();
  create_request.set_parent(Project(project_id_).FullName());
  ASSERT_STATUS_OK(instance_admin_->CreateInstance(create_request).get());

  auto iam_policy = bigtable::IamPolicy({bigtable::IamBinding(
      "roles/bigtable.reader", {"serviceAccount:" + service_account_})});

  auto initial_policy =
      instance_admin_->SetIamPolicy(instance_name, iam_policy);
  ASSERT_STATUS_OK(initial_policy);

  auto fetched_policy = instance_admin_->GetIamPolicy(instance_name);
  ASSERT_STATUS_OK(fetched_policy);

  EXPECT_EQ(initial_policy->version(), fetched_policy->version());
  EXPECT_EQ(initial_policy->etag(), fetched_policy->etag());

  auto permission_set = instance_admin_->TestIamPermissions(
      instance_name, {"bigtable.tables.list", "bigtable.tables.delete"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->permissions().size());
  EXPECT_STATUS_OK(instance_admin_->DeleteInstance(instance_name));
}

/// @test Verify that Instance CRUD operations with logging work as expected.
TEST_F(InstanceAdminIntegrationTest,
       CreateListGetDeleteInstanceTestWithLogging) {
  // In our ci builds, we set GOOGLE_CLOUD_CPP_ENABLE_TRACING to log our tests,
  // by default. We should unset this variable and create a fresh client in
  // order to have a conclusive test.
  testing_util::ScopedEnvironment env = {"GOOGLE_CLOUD_CPP_ENABLE_TRACING",
                                         absl::nullopt};
  testing_util::ScopedLog log;
  auto const instance_id = RandomInstanceId(generator_);
  auto const instance_name = bigtable::InstanceName(project_id_, instance_id);
  Project const project(project_id_);
  auto instance_admin =
      std::make_unique<bigtable_admin::BigtableInstanceAdminClient>(
          bigtable_admin::MakeBigtableInstanceAdminConnection(
              Options{}.set<LoggingComponentsOption>({"rpc"})));

  auto const zone_a = Location(project.FullName(), zone_a_);
  auto const zone_b = Location(project.FullName(), zone_b_);

  // Create instance
  auto config = IntegrationTestConfig(instance_id, zone_a_);
  auto create_request = config.as_proto();
  create_request.set_parent(project.FullName());
  auto instance = instance_admin->CreateInstance(create_request).get();
  ASSERT_STATUS_OK(instance);

  // Verify create
  auto instances = instance_admin->ListInstances(project.FullName());
  ASSERT_STATUS_OK(instances);
  // If either zone_a_ or zone_b_ are in the list of failed locations then we
  // cannot proceed.
  ASSERT_THAT(
      instances->failed_locations(),
      Not(AnyOf(Contains(zone_a.FullName()), Contains(zone_b.FullName()))));
  EXPECT_TRUE(IsInstancePresent(instances->instances(), instance->name()));

  // Get instance
  instance = instance_admin->GetInstance(instance_name);
  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(instance->name(), instance_name);

  // Update instance
  bigtable::InstanceUpdateConfig instance_update_config(std::move(*instance));
  auto const updated_display_name = instance_id.substr(0, 22) + " updated";
  instance_update_config.set_display_name(updated_display_name);
  instance =
      instance_admin->PartialUpdateInstance(instance_update_config.as_proto())
          .get();
  ASSERT_STATUS_OK(instance);

  // Verify update
  instance = instance_admin->GetInstance(instance_name);
  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(updated_display_name, instance->display_name());

  // Delete instance
  ASSERT_STATUS_OK(instance_admin->DeleteInstance(instance_name));

  // Verify delete
  instances = instance_admin->ListInstances(project.FullName());
  ASSERT_STATUS_OK(instances);
  // If either zone_a_ or zone_b_ are in the list of failed locations then we
  // cannot proceed.
  ASSERT_THAT(
      instances->failed_locations(),
      Not(AnyOf(Contains(zone_a.FullName()), Contains(zone_b.FullName()))));
  EXPECT_FALSE(IsInstancePresent(instances->instances(), instance_name));

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListInstances")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("AsyncCreateInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("AsyncPartialUpdateInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteInstance")));

  // Verify that a normal client does not log.
  auto no_logging_client =
      std::make_unique<bigtable_admin::BigtableInstanceAdminClient>(
          bigtable_admin::MakeBigtableInstanceAdminConnection());
  (void)no_logging_client->ListInstances(project.FullName());
  EXPECT_THAT(log.ExtractLines(), Not(Contains(HasSubstr("ListInstances"))));
}

TEST_F(InstanceAdminIntegrationTest, CustomWorkers) {
  CompletionQueue cq;
  instance_admin_ =
      std::make_unique<bigtable_admin::BigtableInstanceAdminClient>(
          bigtable_admin::MakeBigtableInstanceAdminConnection(
              Options{}.set<GrpcCompletionQueueOption>(cq)));

  // CompletionQueue `cq` is not being `Run()`, so this should never finish.
  auto const instance_id = RandomInstanceId(generator_);
  auto create_request =
      IntegrationTestConfig(instance_id, zone_a_,
                            bigtable::InstanceConfig::PRODUCTION, 3)
          .as_proto();
  create_request.set_parent(Project(project_id_).FullName());
  auto instance_fut = instance_admin_->CreateInstance(create_request);

  EXPECT_EQ(std::future_status::timeout,
            instance_fut.wait_for(std::chrono::milliseconds(100)));

  std::thread t([cq]() mutable { cq.Run(); });
  auto instance = instance_fut.get();
  ASSERT_STATUS_OK(instance);
  EXPECT_STATUS_OK(
      instance_admin_->DeleteInstance(InstanceName(project_id_, instance_id)));

  cq.CancelAll();
  cq.Shutdown();
  t.join();
}
}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
