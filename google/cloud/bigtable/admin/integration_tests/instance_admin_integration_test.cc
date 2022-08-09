// Copyright 2021 Google LLC
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
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/project.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/contains_once.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_admin {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::bigtable::testing::RandomInstanceId;
using ::google::cloud::internal::GetEnv;
using ::google::cloud::testing_util::ContainsOnce;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
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
  }

  StatusOr<std::vector<std::string>> ListInstances(
      BigtableInstanceAdminClient& client) {
    auto const project_name = Project(project_id_).FullName();
    auto sor = client.ListInstances(project_name);
    if (!sor) return std::move(sor).status();
    auto resp = *std::move(sor);

    std::vector<std::string> names;
    names.reserve(resp.instances_size());
    auto& instances = *resp.mutable_instances();
    EXPECT_EQ(0, resp.failed_locations_size());
    std::transform(instances.begin(), instances.end(),
                   std::back_inserter(names),
                   [](btadmin::Instance const& i) { return i.name(); });
    return names;
  }

  StatusOr<std::vector<std::string>> ListClusters(
      std::string const& instance_id) {
    auto const instance_name = bigtable::InstanceName(project_id_, instance_id);
    auto sor = client_.ListClusters(instance_name);
    if (!sor) return std::move(sor).status();
    auto resp = *std::move(sor);

    std::vector<std::string> names;
    names.reserve(resp.clusters_size());
    auto& clusters = *resp.mutable_clusters();
    std::transform(clusters.begin(), clusters.end(), std::back_inserter(names),
                   [](btadmin::Cluster const& c) { return c.name(); });
    return names;
  }

  std::string project_id_;
  std::string zone_a_;
  std::string zone_b_;
  std::string service_account_;
  BigtableInstanceAdminClient client_ =
      BigtableInstanceAdminClient(MakeBigtableInstanceAdminConnection());
  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();
};

btadmin::CreateInstanceRequest IntegrationTestConfig(
    std::string const& project, std::string const& instance_id,
    std::string const& location,
    btadmin::Instance::Type type = btadmin::Instance::DEVELOPMENT,
    int32_t serve_nodes = 0) {
  // The description cannot exceed 30 characters
  auto const display_name = ("IT " + instance_id).substr(0, 30);
  auto const project_name = Project(project).FullName();

  btadmin::Cluster c;
  c.set_location(project_name + "/locations/" + location);
  c.set_serve_nodes(serve_nodes);
  c.set_default_storage_type(btadmin::StorageType::HDD);

  btadmin::CreateInstanceRequest r;
  r.set_parent(std::move(project_name));
  r.set_instance_id(instance_id);
  r.mutable_instance()->set_type(type);
  r.mutable_instance()->set_display_name(std::move(display_name));
  (*r.mutable_clusters())[instance_id + "-c1"] = std::move(c);
  return r;
}

protobuf::FieldMask Mask(std::string const& path) {
  protobuf::FieldMask mask;
  mask.add_paths(path);
  return mask;
}

/// @test Verify that default InstanceAdmin::ListClusters works as expected.
TEST_F(InstanceAdminIntegrationTest, ListAllClustersTest) {
  auto const id_1 = RandomInstanceId(generator_);
  auto const id_2 = RandomInstanceId(generator_);
  auto const project_name = Project(project_id_).FullName();
  auto const name_1 = bigtable::InstanceName(project_id_, id_1);
  auto const name_2 = bigtable::InstanceName(project_id_, id_2);

  auto config_1 = IntegrationTestConfig(project_id_, id_1, zone_a_,
                                        btadmin::Instance::PRODUCTION, 3);
  auto config_2 = IntegrationTestConfig(project_id_, id_2, zone_b_,
                                        btadmin::Instance::PRODUCTION, 3);

  auto instance_1_fut = client_.CreateInstance(config_1);
  auto instance_2_fut = client_.CreateInstance(config_2);

  // Wait for instance creation
  auto instance_1 = instance_1_fut.get();
  auto instance_2 = instance_2_fut.get();
  EXPECT_STATUS_OK(instance_1);
  EXPECT_STATUS_OK(instance_2);

  EXPECT_EQ(instance_1->name(), name_1);
  EXPECT_EQ(instance_2->name(), name_2);

  auto clusters = ListClusters("-");
  ASSERT_STATUS_OK(clusters);
  for (auto const& cluster : *clusters) {
    EXPECT_THAT(cluster, HasSubstr(project_name));
  }
  EXPECT_THAT(*clusters, Not(IsEmpty()));

  EXPECT_STATUS_OK(client_.DeleteInstance(name_1));
  EXPECT_STATUS_OK(client_.DeleteInstance(name_2));
}

/// @test Verify that AppProfile CRUD operations work as expected.
TEST_F(InstanceAdminIntegrationTest, CreateListGetDeleteAppProfile) {
  auto const instance_id = RandomInstanceId(generator_);
  auto const instance_name = bigtable::InstanceName(project_id_, instance_id);

  auto config = IntegrationTestConfig(project_id_, instance_id, zone_a_,
                                      btadmin::Instance::PRODUCTION, 3);
  auto instance_fut = client_.CreateInstance(config);
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
  auto profile_names = [](StreamRange<btadmin::AppProfile> list)
      -> StatusOr<std::vector<std::string>> {
    std::vector<std::string> names;
    for (auto const& profile : list) {
      if (!profile) return profile.status();
      names.push_back(profile->name());
    }
    return names;
  };

  auto profiles = profile_names(client_.ListAppProfiles(instance_name));
  ASSERT_STATUS_OK(profiles);
  EXPECT_THAT(*profiles, Not(Contains(name_1)));
  EXPECT_THAT(*profiles, Not(Contains(name_2)));

  auto ap_1 = bigtable::AppProfileConfig::MultiClusterUseAny(id_1).as_proto();
  ap_1.set_parent(instance_name);
  auto profile_1 = client_.CreateAppProfile(ap_1);
  ASSERT_STATUS_OK(profile_1);
  EXPECT_EQ(profile_1->name(), name_1);

  auto ap_2 = bigtable::AppProfileConfig::MultiClusterUseAny(id_2).as_proto();
  ap_2.set_parent(instance_name);
  auto profile_2 = client_.CreateAppProfile(ap_2);
  ASSERT_STATUS_OK(profile_2);
  EXPECT_EQ(profile_2->name(), name_2);

  profiles = profile_names(client_.ListAppProfiles(instance_name));
  ASSERT_STATUS_OK(profiles);
  EXPECT_THAT(*profiles, ContainsOnce(name_1));
  EXPECT_THAT(*profiles, ContainsOnce(name_2));

  profile_1 = client_.GetAppProfile(name_1);
  ASSERT_STATUS_OK(profile_1);
  EXPECT_EQ(profile_1->name(), name_1);

  profile_2 = client_.GetAppProfile(name_2);
  ASSERT_STATUS_OK(profile_2);
  EXPECT_EQ(profile_2->name(), name_2);

  profile_2->set_description("new description");
  profile_2 =
      client_.UpdateAppProfile(*std::move(profile_2), Mask("description"))
          .get();
  ASSERT_STATUS_OK(profile_2);
  EXPECT_EQ("new description", profile_2->description());

  profile_2 = client_.GetAppProfile(name_2);
  ASSERT_STATUS_OK(profile_2);
  EXPECT_EQ("new description", profile_2->description());

  btadmin::DeleteAppProfileRequest req_1;
  req_1.set_ignore_warnings(true);
  req_1.set_name(name_1);

  ASSERT_STATUS_OK(client_.DeleteAppProfile(std::move(req_1)));
  profiles = profile_names(client_.ListAppProfiles(instance_name));
  ASSERT_STATUS_OK(profiles);
  EXPECT_THAT(*profiles, Not(Contains(name_1)));
  EXPECT_THAT(*profiles, ContainsOnce(name_2));

  btadmin::DeleteAppProfileRequest req_2;
  req_2.set_ignore_warnings(true);
  req_2.set_name(name_2);

  ASSERT_STATUS_OK(client_.DeleteAppProfile(std::move(req_2)));
  profiles = profile_names(client_.ListAppProfiles(instance_name));
  ASSERT_STATUS_OK(profiles);
  EXPECT_THAT(*profiles, Not(Contains(name_1)));
  EXPECT_THAT(*profiles, Not(Contains(name_2)));

  ASSERT_STATUS_OK(client_.DeleteInstance(std::move(instance_name)));
}

/// @test Verify that Instance CRUD operations work as expected.
TEST_F(InstanceAdminIntegrationTest, CreateListGetDeleteInstanceTest) {
  auto const instance_id = RandomInstanceId(generator_);
  auto const instance_name = bigtable::InstanceName(project_id_, instance_id);

  // Create instance
  auto config = IntegrationTestConfig(project_id_, instance_id, zone_a_);
  auto instance = client_.CreateInstance(config).get();
  ASSERT_STATUS_OK(instance);

  // List instances
  auto instances = ListInstances(client_);
  ASSERT_STATUS_OK(instances);
  EXPECT_THAT(*instances, Contains(instance_name));

  // Get instance
  instance = client_.GetInstance(instance_name);
  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(instance->name(), instance_name);

  // Update instance
  auto const updated_display_name = instance_id.substr(0, 22) + " updated";
  instance->set_display_name(updated_display_name);
  instance =
      client_.PartialUpdateInstance(*std::move(instance), Mask("display_name"))
          .get();
  ASSERT_STATUS_OK(instance);

  // Verify update
  instance = client_.GetInstance(instance_name);
  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(updated_display_name, instance->display_name());

  // Delete instance
  ASSERT_STATUS_OK(client_.DeleteInstance(instance_name));

  // Verify delete
  instances = ListInstances(client_);
  ASSERT_STATUS_OK(instances);
  EXPECT_THAT(*instances, Not(Contains(instance_name)));
}

/// @test Verify that cluster CRUD operations work as expected.
TEST_F(InstanceAdminIntegrationTest, CreateListGetDeleteClusterTest) {
  auto const instance_id = RandomInstanceId(generator_);
  auto const cluster_id = instance_id + "-cl2";
  auto const project_name = Project(project_id_).FullName();
  auto const instance_name = bigtable::InstanceName(project_id_, instance_id);
  auto const cluster_name =
      bigtable::ClusterName(project_id_, instance_id, cluster_id);

  // Create instance prerequisites for cluster operations
  auto config = IntegrationTestConfig(project_id_, instance_id, zone_a_,
                                      btadmin::Instance::PRODUCTION, 3);
  auto instance = client_.CreateInstance(config).get();
  ASSERT_STATUS_OK(instance);

  // Create cluster
  btadmin::Cluster c;
  c.set_location(project_name + "/locations/" + zone_b_);
  c.set_serve_nodes(3);
  c.set_default_storage_type(btadmin::StorageType::HDD);
  auto cluster = client_.CreateCluster(instance_name, cluster_id, c).get();
  ASSERT_STATUS_OK(cluster);
  EXPECT_EQ(3, cluster->serve_nodes());

  // Verify create
  auto clusters = ListClusters(instance_id);
  ASSERT_STATUS_OK(clusters);
  EXPECT_THAT(*clusters, Contains(cluster_name));

  // Get cluster
  cluster = client_.GetCluster(cluster_name);
  ASSERT_STATUS_OK(cluster);
  EXPECT_EQ(cluster_name, cluster->name());

  // Update cluster
  cluster->set_serve_nodes(4);
  cluster->clear_state();
  cluster = client_.UpdateCluster(*std::move(cluster)).get();
  ASSERT_STATUS_OK(cluster);

  // Verify update
  cluster = client_.GetCluster(cluster_name);
  ASSERT_STATUS_OK(cluster);
  EXPECT_EQ(4, cluster->serve_nodes());

  // Delete cluster
  ASSERT_STATUS_OK(client_.DeleteCluster(cluster_name));

  // Verify delete
  clusters = ListClusters(instance_id);
  ASSERT_STATUS_OK(clusters);
  EXPECT_THAT(*clusters, Not(Contains(cluster_name)));

  // Delete instance
  ASSERT_STATUS_OK(client_.DeleteInstance(instance_name));
}

/// @test Verify that IAM Policy APIs work as expected.
TEST_F(InstanceAdminIntegrationTest, SetGetTestIamAPIsTest) {
  auto const instance_id = RandomInstanceId(generator_);
  auto const instance_name = bigtable::InstanceName(project_id_, instance_id);

  // Create instance
  auto config = IntegrationTestConfig(project_id_, instance_id, zone_a_);
  ASSERT_STATUS_OK(client_.CreateInstance(config).get());

  auto iam_policy = bigtable::IamPolicy({bigtable::IamBinding(
      "roles/bigtable.reader", {"serviceAccount:" + service_account_})});

  auto initial_policy = client_.SetIamPolicy(instance_name, iam_policy);
  ASSERT_STATUS_OK(initial_policy);

  auto fetched_policy = client_.GetIamPolicy(instance_name);
  ASSERT_STATUS_OK(fetched_policy);

  EXPECT_EQ(initial_policy->version(), fetched_policy->version());
  EXPECT_EQ(initial_policy->etag(), fetched_policy->etag());

  auto permission_set = client_.TestIamPermissions(
      instance_name, {"bigtable.tables.list", "bigtable.tables.delete"});
  ASSERT_STATUS_OK(permission_set);

  EXPECT_EQ(2, permission_set->permissions_size());
  EXPECT_STATUS_OK(client_.DeleteInstance(instance_name));
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
  auto const project_name = Project(project_id_).FullName();
  auto const instance_name = bigtable::InstanceName(project_id_, instance_id);

  auto client = BigtableInstanceAdminClient(MakeBigtableInstanceAdminConnection(
      Options{}.set<TracingComponentsOption>({"rpc"})));

  // Create instance
  auto config = IntegrationTestConfig(project_id_, instance_id, zone_a_);
  auto instance = client.CreateInstance(config).get();
  ASSERT_STATUS_OK(instance);

  // Verify create
  auto instances = ListInstances(client);
  ASSERT_STATUS_OK(instances);
  EXPECT_THAT(*instances, Contains(instance_name));

  // Get instance
  instance = client.GetInstance(instance_name);
  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(instance->name(), instance_name);

  // Update instance
  auto const updated_display_name = instance_id.substr(0, 22) + " updated";
  instance->set_display_name(updated_display_name);
  instance =
      client.PartialUpdateInstance(*instance, Mask("display_name")).get();
  ASSERT_STATUS_OK(instance);

  // Verify update
  instance = client.GetInstance(instance_name);
  ASSERT_STATUS_OK(instance);
  EXPECT_EQ(updated_display_name, instance->display_name());

  // Delete instance
  ASSERT_STATUS_OK(client.DeleteInstance(instance_name));

  // Verify delete
  instances = ListInstances(client);
  ASSERT_STATUS_OK(instances);
  EXPECT_THAT(*instances, Not(Contains(instance_name)));

  auto const log_lines = log.ExtractLines();
  EXPECT_THAT(log_lines, Contains(HasSubstr("AsyncCreateInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("ListInstances")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("GetInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("AsyncPartialUpdateInstance")));
  EXPECT_THAT(log_lines, Contains(HasSubstr("DeleteInstance")));

  // Verify that a normal client does not log.
  auto no_logging_client =
      BigtableInstanceAdminClient(MakeBigtableInstanceAdminConnection());
  (void)no_logging_client.ListInstances(project_name);
  EXPECT_THAT(log.ExtractLines(), Not(Contains(HasSubstr("ListInstances"))));
}

TEST_F(InstanceAdminIntegrationTest, CustomWorkers) {
  CompletionQueue cq;
  auto client = BigtableInstanceAdminClient(MakeBigtableInstanceAdminConnection(
      Options{}.set<GrpcCompletionQueueOption>(cq)));

  // CompletionQueue `cq` is not being `Run()`, so this should never finish.
  auto const instance_id = RandomInstanceId(generator_);
  auto instance_fut = client.CreateInstance(IntegrationTestConfig(
      project_id_, instance_id, zone_a_, btadmin::Instance::PRODUCTION, 3));

  EXPECT_EQ(std::future_status::timeout,
            instance_fut.wait_for(std::chrono::milliseconds(100)));

  std::thread t([cq]() mutable { cq.Run(); });
  auto instance = instance_fut.get();
  ASSERT_STATUS_OK(instance);
  EXPECT_STATUS_OK(
      client.DeleteInstance(bigtable::InstanceName(project_id_, instance_id)));

  cq.CancelAll();
  cq.Shutdown();
  t.join();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_admin
}  // namespace cloud
}  // namespace google
