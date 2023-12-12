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

//! [all code]

//! [bigtable includes]
#include "google/cloud/bigtable/admin/bigtable_instance_admin_client.h"
//! [bigtable includes]
#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/location.h"
#include "google/cloud/log.h"
#include "google/cloud/project.h"
#include <algorithm>
#include <iostream>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void BigtableHelloInstance(std::vector<std::string> const& argv) {
  if (argv.size() != 4) {
    throw Usage{
        "hello-instance <project-id> <instance-id> <cluster-id> <zone>"};
  }

  std::string const& project_id = argv[0];
  std::string const& instance_id = argv[1];
  std::string const& cluster_id = argv[2];
  std::string const& zone = argv[3];

  //! [aliases]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::Location;
  using ::google::cloud::Project;
  using ::google::cloud::Status;
  using ::google::cloud::StatusOr;
  //! [aliases]

  // Connect to the Cloud Bigtable admin endpoint.
  //! [connect instance admin]
  auto instance_admin = cbta::BigtableInstanceAdminClient(
      cbta::MakeBigtableInstanceAdminConnection());
  //! [connect instance admin]

  //! [check instance exists]
  std::cout << "\nCheck Instance exists:\n";
  auto const project = Project(project_id);
  std::string const project_name = project.FullName();
  StatusOr<google::bigtable::admin::v2::ListInstancesResponse> instances =
      instance_admin.ListInstances(project_name);
  if (!instances) throw std::move(instances).status();
  if (!instances->failed_locations().empty()) {
    std::cerr
        << "The service tells us it has no information about these locations:";
    for (auto const& failed_location : instances->failed_locations()) {
      std::cerr << " " << failed_location;
    }
    std::cerr << ". Continuing anyway\n";
  }
  std::string const instance_name = cbt::InstanceName(project_id, instance_id);
  auto is_instance =
      [&instance_name](google::bigtable::admin::v2::Instance const& i) {
        return i.name() == instance_name;
      };
  auto const& ins = instances->instances();
  auto instance_exists =
      std::find_if(ins.begin(), ins.end(), is_instance) != ins.end();
  std::cout << "The instance " << instance_id
            << (instance_exists ? " already exists" : " does not exist")
            << "\n";
  //! [check instance exists]

  // Create instance if it does not exist
  if (!instance_exists) {
    //! [create production instance]
    std::cout << "\nCreating a PRODUCTION Instance: ";

    // production instance needs at least 3 nodes
    google::bigtable::admin::v2::Cluster c;
    c.set_location(Location(project, zone).FullName());
    c.set_serve_nodes(3);
    c.set_default_storage_type(google::bigtable::admin::v2::HDD);

    google::bigtable::admin::v2::Instance in;
    in.set_display_name("Sample Instance");
    in.set_type(google::bigtable::admin::v2::Instance::PRODUCTION);

    future<void> creation_done =
        instance_admin
            .CreateInstance(project_name, instance_id, std::move(in),
                            {{cluster_id, std::move(c)}})
            .then(
                [instance_id](
                    future<StatusOr<google::bigtable::admin::v2::Instance>> f) {
                  auto instance = f.get();
                  if (!instance) {
                    std::cerr << "Could not create instance " << instance_id
                              << "\n";
                    throw std::move(instance).status();
                  }
                  std::cout << "Successfully created instance: "
                            << instance->DebugString() << "\n";
                });
    // Note how this blocks until the instance is created. In production code
    // you may want to perform this task asynchronously.
    creation_done.get();
    std::cout << "DONE\n";
    //! [create production instance]
  }

  //! [list instances]
  std::cout << "\nListing Instances:\n";
  instances = instance_admin.ListInstances(project_name);
  if (!instances) throw std::move(instances).status();
  if (!instances->failed_locations().empty()) {
    std::cerr
        << "The service tells us it has no information about these locations:";
    for (auto const& failed_location : instances->failed_locations()) {
      std::cerr << " " << failed_location;
    }
    std::cerr << ". Continuing anyway\n";
  }
  for (auto const& instance : instances->instances()) {
    std::cout << "  " << instance.name() << "\n";
  }
  std::cout << "DONE\n";
  //! [list instances]

  //! [get instance]
  std::cout << "\nGet Instance:\n";
  auto instance = instance_admin.GetInstance(instance_name);
  if (!instance) throw std::move(instance).status();
  std::cout << "Instance details :\n" << instance->DebugString() << "\n";
  //! [get instance]

  //! [list clusters]
  std::cout << "\nListing Clusters:\n";
  StatusOr<google::bigtable::admin::v2::ListClustersResponse> cluster_list =
      instance_admin.ListClusters(instance_name);
  if (!cluster_list) throw std::move(cluster_list).status();
  if (!cluster_list->failed_locations().empty()) {
    std::cout << "The Cloud Bigtable service reports that the following "
                 "locations are temporarily unavailable and no information "
                 "about clusters in these locations can be obtained:\n";
    for (auto const& failed_location : cluster_list->failed_locations()) {
      std::cout << failed_location << "\n";
    }
  }
  std::cout << "Cluster Name List:\n";
  for (auto const& cluster : cluster_list->clusters()) {
    std::cout << "Cluster Name: " << cluster.name() << "\n";
  }
  std::cout << "DONE\n";
  //! [list clusters]

  //! [delete instance]
  std::cout << "Deleting instance " << instance_id << "\n";
  Status delete_status = instance_admin.DeleteInstance(instance_name);
  if (!delete_status.ok()) throw std::runtime_error(delete_status.message());
  std::cout << "DONE\n";
  //! [delete instance]
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbta = ::google::cloud::bigtable_admin;

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

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  google::cloud::bigtable::testing::CleanupStaleInstances(
      cbta::MakeBigtableInstanceAdminConnection(), project_id);
  auto const instance_id =
      google::cloud::bigtable::testing::RandomInstanceId(generator);
  auto const cluster_id = instance_id + "-c1";

  std::cout << "\nRunning the BigtableHelloInstance() example" << std::endl;
  BigtableHelloInstance({project_id, instance_id, cluster_id, zone_a});
}

}  // namespace

int main(int argc, char* argv[]) try {
  google::cloud::bigtable::examples::Example example({
      {"auto", RunAll},
      {"hello-instance", BigtableHelloInstance},
  });
  return example.Run(argc, argv);
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  ::google::cloud::LogSink::Instance().Flush();
  return 1;
}
//! [all code]
