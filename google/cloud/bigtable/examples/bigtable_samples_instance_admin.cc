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
#include "google/cloud/bigtable/instance_admin_client.h"
#include <google/protobuf/text_format.h>
#include <typeindex>
#include <typeinfo>

namespace {
struct Usage {
  std::string msg;
};

char const* ConsumeArg(int& argc, char* argv[]) {
  if (argc < 2) {
    return nullptr;
  }
  char const* result = argv[1];
  std::copy(argv + 2, argv + argc, argv + 1);
  argc--;
  return result;
}

std::string command_usage;

void PrintUsage(int argc, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

// This full example demonstrate various instance operations
// by initially creating instance of type PRODUCTION
void RunInstanceOperations(std::string project_id, int argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{"run: <project-id> <instance-id> <cluster-id> <zone>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  std::string const zone = ConsumeArg(argc, argv);

  // [START connect_instance_admin]
  google::cloud::bigtable::InstanceAdmin instance_admin(
      google::cloud::bigtable::CreateDefaultInstanceAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()));
  // [END connect_instance_admin]

  // [START bigtable_check_instance_exists] [START bigtable_exists_instance]
  std::cout << "\nCheck Instance exists:\n";
  auto instances = instance_admin.ListInstances();
  if (!instances) {
    throw std::runtime_error(instances.status().message());
  }
  auto instance_name =
      instance_admin.project_name() + "/instances/" + instance_id.get();
  bool instance_exists =
      instances->instances.end() !=
      std::find_if(
          instances->instances.begin(), instances->instances.end(),
          [&instance_name](google::bigtable::admin::v2::Instance const& i) {
            return i.name() == instance_name;
          });
  if (!instances->failed_locations.empty()) {
    std::cerr
        << "The service tells us it has no information about these locations:";
    for (auto const& failed_location : instances->failed_locations) {
      std::cerr << " " << failed_location;
    }
    std::cerr << ". Continuing anyway\n";
  }
  // [END bigtable_check_instance_exists] [END bigtable_exists_instance]

  // Create instance if does not exists
  if (!instance_exists) {
    // [START bigtable_create_prod_instance]
    std::cout << "\nCreating a PRODUCTION Instance: ";
    google::cloud::bigtable::DisplayName display_name("Sample Instance");

    // production instance needs at least 3 nodes
    auto cluster_config = google::cloud::bigtable::ClusterConfig(
        zone, 3, google::cloud::bigtable::ClusterConfig::SSD);
    google::cloud::bigtable::InstanceConfig config(
        google::cloud::bigtable::InstanceId(instance_id), display_name,
        {{cluster_id.get(), cluster_config}});
    config.set_type(google::cloud::bigtable::InstanceConfig::PRODUCTION);

    auto instance_details = instance_admin.CreateInstance(config).get();
    std::cout << " Done\n";
    // [END bigtable_create_prod_instance]
  } else {
    std::cout << "\nInstance " << instance_id.get() << " already exists.\n";
    return;
  }

  // [START bigtable_list_instances]
  std::cout << "\nListing Instances:\n";
  auto instances_after = instance_admin.ListInstances();
  if (!instances_after) {
    throw std::runtime_error(instances_after.status().message());
  }
  for (auto const& instance : instances_after->instances) {
    std::cout << instance.name() << "\n";
  }
  if (!instances_after->failed_locations.empty()) {
    std::cerr
        << "The service tells us it has no information about these locations:";
    for (auto const& failed_location : instances_after->failed_locations) {
      std::cerr << " " << failed_location;
    }
    std::cerr << ". Continuing anyway\n";
  }
  // [END bigtable_list_instances]

  // [START bigtable_get_instance]
  std::cout << "\nGet Instance:\n";
  auto instance = instance_admin.GetInstance(instance_id.get());
  if (!instance) {
    throw std::runtime_error(instance.status().message());
  }
  std::string instance_detail;
  google::protobuf::TextFormat::PrintToString(*instance, &instance_detail);
  std::cout << "GetInstance details :\n" << instance_detail;
  // [END bigtable_get_instance]

  // [START bigtable_get_clusters]
  std::cout << "\nListing Clusters:\n";
  auto cluster_list = instance_admin.ListClusters(instance_id.get());
  if (!cluster_list) {
    throw std::runtime_error(cluster_list.status().message());
  }
  std::cout << "Cluster Name List:\n";
  for (auto const& cluster : cluster_list->clusters) {
    std::cout << "Cluster Name: " << cluster.name() << "\n";
  }
  if (!cluster_list->failed_locations.empty()) {
    std::cout << "The Cloud Bigtable service reports that the following "
                 "locations are temporarily unavailable and no information "
                 "about clusters in these locations can be obtained:\n";
    for (auto const& failed_location : cluster_list->failed_locations) {
      std::cout << failed_location << "\n";
    }
  }
  // [END bigtable_get_clusters]
}

void CreateDevInstance(std::string project_id, int argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "create-dev-instance: <project-id> <instance-id> <cluster-id> <zone>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  std::string const zone = ConsumeArg(argc, argv);

  // Create an instance admin endpoint.
  google::cloud::bigtable::InstanceAdmin instance_admin(
      google::cloud::bigtable::CreateDefaultInstanceAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()));

  std::cout << "\nCheck Instance exists:\n";
  auto instances = instance_admin.ListInstances();
  if (!instances) {
    throw std::runtime_error(instances.status().message());
  }
  auto instance_name =
      instance_admin.project_name() + "/instances/" + instance_id.get();
  bool instance_exists =
      instances->instances.end() !=
      std::find_if(
          instances->instances.begin(), instances->instances.end(),
          [&instance_name](google::bigtable::admin::v2::Instance const& i) {
            return i.name() == instance_name;
          });
  if (!instances->failed_locations.empty()) {
    std::cerr
        << "The service tells us it has no information about these locations:";
    for (auto const& failed_location : instances->failed_locations) {
      std::cerr << " " << failed_location;
    }
    std::cerr << ". Continuing anyway\n";
  }
  // Create instance if does not exists
  if (!instance_exists) {
    // [START bigtable_create_dev_instance]
    std::cout << "\nCreating a DEVELOPMENT Instance: ";
    google::cloud::bigtable::DisplayName display_name("Put description here");

    // Cluster nodes should not be set while creating Development Instance
    auto cluster_config = google::cloud::bigtable::ClusterConfig(
        zone, 0, google::cloud::bigtable::ClusterConfig::HDD);
    google::cloud::bigtable::InstanceConfig config(
        google::cloud::bigtable::InstanceId(instance_id), display_name,
        {{cluster_id.get(), cluster_config}});
    config.set_type(google::cloud::bigtable::InstanceConfig::DEVELOPMENT);

    auto future = instance_admin.CreateInstance(config).get();
    std::cout << " Done\n";
    // [END bigtable_create_dev_instance]
  } else {
    std::cout << "\nInstance " << instance_id.get() << " already exists.\n";
    return;
  }
  std::cout << " Done\n";
}

void DeleteInstance(std::string project_id, int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"delete-instance: <project-id> <instance-id>"};
  }

  std::string instance_id = ConsumeArg(argc, argv);

  // Create an instance admin endpoint.
  google::cloud::bigtable::InstanceAdmin instance_admin(
      google::cloud::bigtable::CreateDefaultInstanceAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()));

  // [START bigtable_delete_instance]
  std::cout << "\nDeleting Instance: ";
  google::cloud::Status status = instance_admin.DeleteInstance(instance_id);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
  std::cout << " Done\n";
  // [END bigtable_delete_instance]
}

void CreateCluster(std::string project_id, int argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "create-cluster: <project-id> <instance-id> <cluster-id> <zone>"};
  }

  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  std::string const zone = ConsumeArg(argc, argv);

  // Create an instance admin endpoint.
  google::cloud::bigtable::InstanceAdmin instance_admin(
      google::cloud::bigtable::CreateDefaultInstanceAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()));

  std::cout << "\nCheck Instance exists:\n";
  auto instances = instance_admin.ListInstances();
  if (!instances) {
    throw std::runtime_error(instances.status().message());
  }
  auto instance_name =
      instance_admin.project_name() + "/instances/" + instance_id.get();
  bool instance_exists =
      instances->instances.end() !=
      std::find_if(
          instances->instances.begin(), instances->instances.end(),
          [&instance_name](google::bigtable::admin::v2::Instance const& i) {
            return i.name() == instance_name;
          });
  if (!instances->failed_locations.empty()) {
    std::cerr
        << "The service tells us it has no information about these locations:";
    for (auto const& failed_location : instances->failed_locations) {
      std::cerr << " " << failed_location;
    }
    std::cerr << ". Continuing anyway\n";
  }
  if (instance_exists) {
    // [START bigtable_create_cluster]
    std::cout << "Adding Cluster to Instance: " << instance_id.get() << "\n";
    auto cluster_config = google::cloud::bigtable::ClusterConfig(
        zone, 3, google::cloud::bigtable::ClusterConfig::SSD);
    auto cluster =
        instance_admin.CreateCluster(cluster_config, instance_id, cluster_id);

    std::cout << "Cluster Created: " << cluster_id.get() << "\n";
    // [END bigtable_create_cluster]
  } else {
    std::cout << "\nInstance " << instance_id.get() << " does not exists.\n";
    return;
  }
}

void DeleteCluster(std::string project_id, int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"delete-cluster: <project-id> <instance-id> <cluster-id>"};
  }

  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));

  // Create an instance admin endpoint.
  google::cloud::bigtable::InstanceAdmin instance_admin(
      google::cloud::bigtable::CreateDefaultInstanceAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()));

  // [START bigtable_delete_cluster]
  std::cout << "\nDeleting Cluster: ";
  google::cloud::Status status =
      instance_admin.DeleteCluster(instance_id, cluster_id);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
  std::cout << " Done\n";
  // [END bigtable_delete_cluster]
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(std::string, int, char* [])>;

  std::map<std::string, CommandType> commands = {
      {"run", &RunInstanceOperations},
      {"create-dev-instance", &CreateDevInstance},
      {"delete-instance", &DeleteInstance},
      {"create-cluster", &CreateCluster},
      {"delete-cluster", &DeleteCluster},
  };

  for (auto&& kv : commands) {
    try {
      std::string unused;
      int fake_argc = 0;
      kv.second(unused, fake_argc, argv);
    } catch (Usage const& u) {
      command_usage += "    ";
      command_usage += u.msg;
      command_usage += "\n";
    }
  }

  if (argc < 3) {
    PrintUsage(argc, argv, "Missing command and/or project-id");
    return 1;
  }

  std::string const command_name = ConsumeArg(argc, argv);
  std::string const project_id = ConsumeArg(argc, argv);

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    PrintUsage(argc, argv, "Unknown command: " + command_name);
    return 1;
  }

  command->second(project_id, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
//! [all code]
