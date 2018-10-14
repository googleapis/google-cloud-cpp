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
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>

namespace btadmin = google::bigtable::admin::v2;
namespace bigtable = google::cloud::bigtable;

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

/// @test Verify that Instance CRUD operations work as expected.
TEST_F(InstanceAdminAsyncIntegrationTest, CreateListGetDeleteInstanceTest) {
  std::string instance_id =
      "it-" + google::cloud::internal::Sample(
                  generator_, 8, "abcdefghijklmnopqrstuvwxyz0123456789");

  // verify new instance id in list of instances
  auto instances_before = instance_admin_->ListInstances();
  ASSERT_FALSE(IsInstancePresent(instances_before, instance_id))
      << "Instance (" << instance_id << ") already exists."
      << " This is unexpected, as the instance ids are"
      << " generated at random.";

  google::cloud::bigtable::CompletionQueue cq;
  std::thread pool([&cq] { cq.Run(); });

  // create instance
  auto config = IntegrationTestConfig(instance_id);
  auto instance = instance_admin_->CreateInstance(config).get();
  auto instances_current = instance_admin_->ListInstances();
  EXPECT_TRUE(IsInstancePresent(instances_current, instance.name()));

  // Get instance
  bigtable::noex::InstanceAdmin admin(instance_admin_client_);
  std::promise<btadmin::Instance> done;
  admin.AsyncGetInstance(
      instance_id, cq,
      [&done](google::cloud::bigtable::CompletionQueue& cq,
              btadmin::Instance& instance, grpc::Status const& status) {
        done.set_value(std::move(instance));
      });
  auto instance_result = done.get_future().get();
  auto const npos = std::string::npos;
  EXPECT_NE(npos, instance_result.name().find(instance_admin_->project_name()));
  EXPECT_NE(npos, instance_result.name().find(instance_id));

  // update instance
  btadmin::Instance instance_copy;
  instance_copy.CopyFrom(instance);
  bigtable::InstanceUpdateConfig instance_update_config(std::move(instance));
  auto const updated_display_name = instance_id + " updated";
  instance_update_config.set_display_name(updated_display_name);
  auto instance_after =
      instance_admin_->UpdateInstance(std::move(instance_update_config)).get();
  auto instance_after_update = instance_admin_->GetInstance(instance_id);
  EXPECT_EQ(updated_display_name, instance_after_update.display_name());

  // Delete instance
  instance_admin_->DeleteInstance(instance_id);
  auto instances_after_delete = instance_admin_->ListInstances();
  EXPECT_TRUE(IsInstancePresent(instances_current, instance_copy.name()));
  EXPECT_FALSE(IsInstancePresent(instances_after_delete, instance.name()));

  cq.Shutdown();
  pool.join();
}

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

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
