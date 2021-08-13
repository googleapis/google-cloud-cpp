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

//! [all code]

//! [bigtable includes]
#include "google/cloud/bigtable/instance_admin.h"
//! [bigtable includes]
#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/crash_handler.h"
#include <iostream>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void BigtableHelloInstance(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw Usage{
        "bigtable-hello-instance <project-id> <instance-id> <cluster-id> "
        "<zone>"};
  }

  std::string const project_id = argv[0];
  std::string const instance_id = argv[1];
  std::string const cluster_id = argv[2];
  std::string const zone = argv[3];

  //! [aliases]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  //! [aliases]

  // Connect to the Cloud Bigtable admin endpoint.
  //! [connect instance admin]
  cbt::InstanceAdmin instance_admin(
      cbt::CreateDefaultInstanceAdminClient(project_id, cbt::ClientOptions()));
  //! [connect instance admin]

  //! [check instance exists]
  std::cout << "\nCheck Instance exists:\n";
  StatusOr<cbt::InstanceList> instances = instance_admin.ListInstances();
  if (!instances) throw std::runtime_error(instances.status().message());
  if (!instances->failed_locations.empty()) {
    std::cerr
        << "The service tells us it has no information about these locations:";
    for (auto const& failed_location : instances->failed_locations) {
      std::cerr << " " << failed_location;
    }
    std::cerr << ". Continuing anyway\n";
  }
  auto instance_name =
      instance_admin.project_name() + "/instances/" + instance_id;
  bool instance_exists = google::cloud::internal::ContainsIf(
      instances->instances,
      [&instance_name](google::bigtable::admin::v2::Instance const& i) {
        return i.name() == instance_name;
      });
  std::cout << "The instance " << instance_id
            << (instance_exists ? " already exists" : " does not exist")
            << "\n";
  //! [check instance exists]

  // Create instance if does not exists
  if (!instance_exists) {
    //! [create production instance]
    std::cout << "\nCreating a PRODUCTION Instance: ";

    // production instance needs at least 3 nodes
    auto cluster_config = cbt::ClusterConfig(zone, 3, cbt::ClusterConfig::HDD);
    cbt::InstanceConfig config(instance_id, "Sample Instance",
                               {{cluster_id, cluster_config}});
    config.set_type(cbt::InstanceConfig::PRODUCTION);

    google::cloud::future<void> creation_done =
        instance_admin.CreateInstance(config).then(
            [instance_id](
                future<StatusOr<google::bigtable::admin::v2::Instance>> f) {
              auto instance = f.get();
              if (!instance) {
                std::cerr << "Could not create instance " << instance_id
                          << "\n";
                throw std::runtime_error(instance.status().message());
              }
              std::cout << "Successfully created instance: "
                        << instance->DebugString() << "\n";
            });
    // Note how this blocks until the instance is created, in production code
    // you may want to perform this task asynchronously.
    creation_done.get();
    std::cout << "DONE\n";
    //! [create production instance]
  }

  //! [list instances]
  std::cout << "\nListing Instances:\n";
  instances = instance_admin.ListInstances();
  if (!instances) throw std::runtime_error(instances.status().message());
  if (!instances->failed_locations.empty()) {
    std::cerr
        << "The service tells us it has no information about these locations:";
    for (auto const& failed_location : instances->failed_locations) {
      std::cerr << " " << failed_location;
    }
    std::cerr << ". Continuing anyway\n";
  }
  for (auto const& instance : instances->instances) {
    std::cout << "  " << instance.name() << "\n";
  }
  std::cout << "DONE\n";
  //! [list instances]

  //! [get instance]
  std::cout << "\nGet Instance:\n";
  auto instance = instance_admin.GetInstance(instance_id);
  if (!instance) throw std::runtime_error(instance.status().message());
  std::cout << "Instance details :\n" << instance->DebugString() << "\n";
  //! [get instance]

  //! [list clusters]
  std::cout << "\nListing Clusters:\n";
  StatusOr<cbt::ClusterList> cluster_list =
      instance_admin.ListClusters(instance_id);
  if (!cluster_list) throw std::runtime_error(cluster_list.status().message());
  if (!cluster_list->failed_locations.empty()) {
    std::cout << "The Cloud Bigtable service reports that the following "
                 "locations are temporarily unavailable and no information "
                 "about clusters in these locations can be obtained:\n";
    for (auto const& failed_location : cluster_list->failed_locations) {
      std::cout << failed_location << "\n";
    }
  }
  std::cout << "Cluster Name List:\n";
  for (auto const& cluster : cluster_list->clusters) {
    std::cout << "Cluster Name: " << cluster.name() << "\n";
  }
  std::cout << "DONE\n";
  //! [list clusters]

  //! [delete instance]
  std::cout << "Deleting instance " << instance_id << "\n";
  google::cloud::Status delete_status =
      instance_admin.DeleteInstance(instance_id);
  if (!delete_status.ok()) throw std::runtime_error(delete_status.message());
  std::cout << "DONE\n";
  //! [delete instance]
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = ::google::cloud::bigtable;

  if (!argv.empty()) throw Usage{"auto"};
  if (!examples::RunAdminIntegrationTests()) return;
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const zone_a =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A")
          .value();

  cbt::InstanceAdmin admin(
      cbt::CreateDefaultInstanceAdminClient(project_id, cbt::ClientOptions{}));

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  google::cloud::bigtable::testing::CleanupStaleInstances(admin);
  auto const instance_id =
      google::cloud::bigtable::testing::RandomInstanceId(generator);
  auto const cluster_id = instance_id + "-c1";

  std::cout << "\nRunning the BigtableHelloInstance() example" << std::endl;
  BigtableHelloInstance({project_id, instance_id, cluster_id, zone_a});
}

}  // namespace

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InstallCrashHandler(argv[0]);

  google::cloud::bigtable::examples::Example example({
      {"auto", RunAll},
      {"hello-instance", BigtableHelloInstance},
  });
  return example.Run(argc, argv);
}
//! [all code]
