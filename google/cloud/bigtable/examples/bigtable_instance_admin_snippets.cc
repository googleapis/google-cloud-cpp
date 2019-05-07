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

void CreateInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                    int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"create-instance <instance-id> <zone>"};
  }
  auto instance_id = ConsumeArg(argc, argv);
  auto zone = ConsumeArg(argc, argv);

  //! [create instance] [START bigtable_create_prod_instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string zone) {
    cbt::DisplayName display_name("Put description here");
    std::string cluster_id = instance_id + "-c1";
    auto cluster_config = cbt::ClusterConfig(zone, 3, cbt::ClusterConfig::HDD);
    cbt::InstanceConfig config(cbt::InstanceId(instance_id), display_name,
                               {{cluster_id, cluster_config}});
    config.set_type(cbt::InstanceConfig::PRODUCTION);

    future<StatusOr<google::bigtable::admin::v2::Instance>> instance_future =
        instance_admin.CreateInstance(config);
    // Show how to perform additional work while the long running operation
    // completes. The application could use future.then() instead.
    std::cout << "Waiting for instance creation to complete " << std::flush;
    instance_future.wait_for(std::chrono::seconds(1));
    std::cout << '.' << std::flush;
    auto instance = instance_future.get();
    if (!instance) {
      throw std::runtime_error(instance.status().message());
    }
    std::cout << "DONE, details=" << instance->DebugString() << "\n";
  }
  //! [create instance] [END bigtable_create_prod_instance]
  (std::move(instance_admin), instance_id, zone);
}

void CreateDevInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                       int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"create-dev-instance <instance-id> <zone>"};
  }
  auto instance_id = ConsumeArg(argc, argv);
  auto zone = ConsumeArg(argc, argv);

  //! [create dev instance] [START bigtable_create_dev_instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string zone) {
    cbt::DisplayName display_name("Put description here");
    std::string cluster_id = instance_id + "-c1";
    auto cluster_config = cbt::ClusterConfig(zone, 0, cbt::ClusterConfig::HDD);
    cbt::InstanceConfig config(cbt::InstanceId(instance_id), display_name,
                               {{cluster_id, cluster_config}});
    config.set_type(cbt::InstanceConfig::DEVELOPMENT);

    future<StatusOr<google::bigtable::admin::v2::Instance>> instance_future =
        instance_admin.CreateInstance(config);
    // Show how to perform additional work while the long running operation
    // completes. The application could use future.then() instead.
    std::cout << "Waiting for instance creation to complete " << std::flush;
    instance_future.wait_for(std::chrono::seconds(2));
    std::cout << '.' << std::flush;
    auto instance = instance_future.get();
    if (!instance) {
      throw std::runtime_error(instance.status().message());
    }
    std::cout << "DONE, details=" << instance->DebugString() << "\n";
  }
  //! [create dev instance] [END bigtable_create_dev_instance]
  (std::move(instance_admin), instance_id, zone);
}

void CreateReplicatedInstance(
    google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
    char* argv[]) {
  if (argc != 4) {
    throw Usage{"create-replicated-instance <instance-id> <zone-a> <zone-b>"};
  }
  auto instance_id = ConsumeArg(argc, argv);
  auto zone_a = ConsumeArg(argc, argv);
  auto zone_b = ConsumeArg(argc, argv);

  // [START bigtable_create_replicated_cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string zone_a, std::string zone_b) {
    cbt::DisplayName display_name("Put description here");
    auto c1 = instance_id + "-c1";
    auto c2 = instance_id + "-c2";
    cbt::InstanceConfig config(
        cbt::InstanceId(instance_id), display_name,
        {{c1, cbt::ClusterConfig(zone_a, 3, cbt::ClusterConfig::HDD)},
         {c2, cbt::ClusterConfig(zone_b, 3, cbt::ClusterConfig::HDD)}});
    config.set_type(cbt::InstanceConfig::PRODUCTION);

    future<StatusOr<google::bigtable::admin::v2::Instance>> instance_future =
        instance_admin.CreateInstance(config);
    // Show how to perform additional work while the long running operation
    // completes. The application could use future.then() instead.
    std::cout << "Waiting for instance creation to complete " << std::flush;
    instance_future.wait_for(std::chrono::seconds(1));
    std::cout << '.' << std::flush;
    auto instance = instance_future.get();
    if (!instance) {
      throw std::runtime_error(instance.status().message());
    }
    std::cout << "DONE, details=" << instance->DebugString() << "\n";
  }
  // [END bigtable_create_replicated_cluster]
  (std::move(instance_admin), instance_id, zone_a, zone_b);
}

void UpdateInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                    int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"update-instance <project-id> <instance-id>"};
  }
  auto instance_id = ConsumeArg(argc, argv);

  //! [update instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id) {
    auto instance = instance_admin.GetInstance(instance_id);
    if (!instance) {
      throw std::runtime_error(instance.status().message());
    }
    // Modify the instance and prepare the mask with modified field
    cbt::InstanceUpdateConfig instance_update_config(std::move(*instance));
    instance_update_config.set_display_name("Modified Display Name");

    future<StatusOr<google::bigtable::admin::v2::Instance>> instance_future =
        instance_admin.UpdateInstance(std::move(instance_update_config));
    instance_future
        .then([](future<StatusOr<google::bigtable::admin::v2::Instance>> f) {
          auto updated_instance = f.get();
          if (!updated_instance) {
            throw std::runtime_error(updated_instance.status().message());
          }
          std::cout << "UpdateInstance details : "
                    << updated_instance->DebugString() << "\n";
        })
        .get();  // block until done to simplify example
  }
  //! [update instance]
  (std::move(instance_admin), instance_id);
}

void ListInstances(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"list-instances <project-id>"};
  }

  //! [list instances] [START bigtable_list_instances]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin) {
    StatusOr<cbt::InstanceList> instances = instance_admin.ListInstances();
    if (!instances) {
      throw std::runtime_error(instances.status().message());
    }
    for (auto const& instance : instances->instances) {
      std::cout << instance.name() << "\n";
    }
    if (!instances->failed_locations.empty()) {
      std::cout << "The Cloud Bigtable service reports that the following "
                   "locations are temporarily unavailable and no information "
                   "about instances in these locations can be obtained:\n";
      for (auto const& failed_location : instances->failed_locations) {
        std::cout << failed_location << "\n";
      }
    }
  }
  //! [list instances] [END bigtable_list_instances]
  (std::move(instance_admin));
}

void CheckInstanceExists(google::cloud::bigtable::InstanceAdmin instance_admin,
                         int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"check-instance-exists <project-id> <instance-id>"};
  }
  auto instance_id = ConsumeArg(argc, argv);

  // [START bigtable_check_instance_exists]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id) {
    StatusOr<google::bigtable::admin::v2::Instance> instance =
        instance_admin.GetInstance(instance_id);
    if (!instance) {
      if (instance.status().code() == google::cloud::StatusCode::kNotFound) {
        throw std::runtime_error("Instance " + instance_id + " does not exist");
      }
      throw std::runtime_error(instance.status().message());
    }
    std::cout << "Instance " << instance->name() << " was found\n";
  }
  // [END bigtable_check_instance_exists]
  (std::move(instance_admin), instance_id);
}

void GetInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                 int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-instance <project-id> <instance-id>"};
  }
  auto instance_id = ConsumeArg(argc, argv);

  //! [get instance] [START bigtable_get_instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id) {
    StatusOr<google::bigtable::admin::v2::Instance> instance =
        instance_admin.GetInstance(instance_id);
    if (!instance) {
      throw std::runtime_error(instance.status().message());
    }
    std::cout << "GetInstance details : " << instance->DebugString() << "\n";
  }
  //! [get instance] [END bigtable_get_instance]
  (std::move(instance_admin), instance_id);
}

void DeleteInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                    int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"delete-instance <project-id> <instance-id>"};
  }
  auto instance_id = ConsumeArg(argc, argv);

  //! [delete instance] [START bigtable_delete_instance]
  namespace cbt = google::cloud::bigtable;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id) {
    google::cloud::Status status = instance_admin.DeleteInstance(instance_id);
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "Successfully deleted the instance " << instance_id << "\n";
  }
  //! [delete instance] [END bigtable_delete_instance]
  (std::move(instance_admin), instance_id);
}

void CreateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "create-cluster <project-id> <instance-id> <cluster-id> <zone>"};
  }
  auto instance_id = ConsumeArg(argc, argv);
  auto cluster_id = ConsumeArg(argc, argv);
  auto zone = ConsumeArg(argc, argv);

  //! [create cluster] [START bigtable_create_cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string cluster_id, std::string zone) {
    auto cluster_config = cbt::ClusterConfig(zone, 3, cbt::ClusterConfig::HDD);
    future<StatusOr<google::bigtable::admin::v2::Cluster>> cluster_future =
        instance_admin.CreateCluster(cluster_config,
                                     cbt::InstanceId(instance_id),
                                     cbt::ClusterId(cluster_id));

    // Applications can wait asynchronously, in this example we just block.
    auto cluster = cluster_future.get();
    if (!cluster) {
      throw std::runtime_error(cluster.status().message());
    }
    std::cout << "Successfully created cluster " << cluster->name() << "\n";
  }
  //! [create cluster] [END bigtable_create_cluster]
  (std::move(instance_admin), instance_id, cluster_id, zone);
}

void ListClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                  int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-clusters <project-id> <instance-id>"};
  }
  auto instance_id = ConsumeArg(argc, argv);

  //! [list clusters] [START bigtable_get_clusters]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id) {
    StatusOr<cbt::ClusterList> cluster_list =
        instance_admin.ListClusters(instance_id);
    if (!cluster_list) {
      throw std::runtime_error(cluster_list.status().message());
    }
    std::cout << "Cluster Name List\n";
    for (auto const& cluster : cluster_list->clusters) {
      std::cout << "Cluster Name:" << cluster.name() << "\n";
    }
    if (!cluster_list->failed_locations.empty()) {
      std::cout << "The Cloud Bigtable service reports that the following "
                   "locations are temporarily unavailable and no information "
                   "about clusters in these locations can be obtained:\n";
      for (auto const& failed_location : cluster_list->failed_locations) {
        std::cout << failed_location << "\n";
      }
    }
  }
  //! [list clusters] [END bigtable_get_clusters]
  (std::move(instance_admin), instance_id);
}

void ListAllClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                     int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"list-all-clusters <project-id>"};
  }

  //! [list all clusters] [START bigtable_get_clusters]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin) {
    StatusOr<cbt::ClusterList> cluster_list = instance_admin.ListClusters();
    if (!cluster_list) {
      throw std::runtime_error(cluster_list.status().message());
    }
    std::cout << "Cluster Name List\n";
    for (auto const& cluster : cluster_list->clusters) {
      std::cout << "Cluster Name:" << cluster.name() << "\n";
    }
    if (!cluster_list->failed_locations.empty()) {
      std::cout << "The Cloud Bigtable service reports that the following "
                   "locations are temporarily unavailable and no information "
                   "about clusters in these locations can be obtained:\n";
      for (auto const& failed_location : cluster_list->failed_locations) {
        std::cout << failed_location << "\n";
      }
    }
  }
  //! [list all clusters] [END bigtable_get_clusters]
  (std::move(instance_admin));
}

void UpdateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"update-cluster <project-id> <instance-id> <cluster-id>"};
  }
  auto instance_id = ConsumeArg(argc, argv);
  auto cluster_id = ConsumeArg(argc, argv);

  //! [update cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string cluster_id) {
    // GetCluster first and then modify it.
    StatusOr<google::bigtable::admin::v2::Cluster> cluster =
        instance_admin.GetCluster(cbt::InstanceId(instance_id),
                                  cbt::ClusterId(cluster_id));
    if (!cluster) {
      throw std::runtime_error(cluster.status().message());
    }

    // The state cannot be sent on updates, so clear it first.
    cluster->clear_state();
    // Set the desired cluster configuration.
    cluster->set_serve_nodes(4);
    auto modified_config = cbt::ClusterConfig(std::move(*cluster));

    StatusOr<google::bigtable::admin::v2::Cluster> modified_cluster =
        instance_admin.UpdateCluster(modified_config).get();
    if (!modified_cluster) {
      throw std::runtime_error(modified_cluster.status().message());
    }
    std::cout << "cluster details : " << cluster->DebugString() << "\n";
  }
  //! [update cluster]
  (std::move(instance_admin), instance_id, cluster_id);
}

void GetCluster(google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
                char* argv[]) {
  if (argc != 3) {
    throw Usage{"get-cluster <project-id> <instance-id> <cluster-id>"};
  }
  auto instance_id = ConsumeArg(argc, argv);
  auto cluster_id = ConsumeArg(argc, argv);

  //! [get cluster] [START bigtable_get_cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string cluster_id) {
    StatusOr<google::bigtable::admin::v2::Cluster> cluster =
        instance_admin.GetCluster(cbt::InstanceId(instance_id),
                                  cbt::ClusterId(cluster_id));
    if (!cluster) {
      throw std::runtime_error(cluster.status().message());
    }
    std::cout << "GetCluster details : " << cluster->DebugString() << "\n";
  }
  //! [get cluster] [END bigtable_get_cluster]
  (std::move(instance_admin), instance_id, cluster_id);
}

void DeleteCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"delete-cluster: <project-id> <instance-id> <cluster-id>"};
  }
  auto instance_id = ConsumeArg(argc, argv);
  auto cluster_id = ConsumeArg(argc, argv);

  //! [delete cluster] [START bigtable_delete_cluster]
  namespace cbt = google::cloud::bigtable;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string cluster_id) {
    google::cloud::Status status = instance_admin.DeleteCluster(
        cbt::InstanceId(instance_id), cbt::ClusterId(cluster_id));
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
  }
  //! [delete cluster] [END bigtable_delete_cluster]
  (std::move(instance_admin), instance_id, cluster_id);
}

void RunInstanceOperations(
    google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
    char* argv[]) {
  if (argc != 4) {
    throw Usage{"run: <project-id> <instance-id> <cluster-id> <zone>"};
  }

  auto instance_id = ConsumeArg(argc, argv);
  auto cluster_id = ConsumeArg(argc, argv);
  auto zone = ConsumeArg(argc, argv);

  //! [run instance operations]
  namespace cbt = google::cloud::bigtable;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string cluster_id, std::string zone) {
    cbt::DisplayName display_name("Put description here");
    auto cluster_config = cbt::ClusterConfig(zone, 3, cbt::ClusterConfig::HDD);
    cbt::InstanceConfig config(cbt::InstanceId(instance_id), display_name,
                               {{cluster_id, cluster_config}});
    config.set_type(cbt::InstanceConfig::PRODUCTION);

    std::cout << "\nCreating a PRODUCTION Instance: ";
    auto future = instance_admin.CreateInstance(config).get();
    std::cout << " Done\n";

    std::cout << "\nListing Instances:\n";
    auto instances = instance_admin.ListInstances();
    if (!instances) {
      throw std::runtime_error(instances.status().message());
    }
    for (auto const& instance : instances->instances) {
      std::cout << instance.name() << "\n";
    }
    if (!instances->failed_locations.empty()) {
      std::cout << "The Cloud Bigtable service reports that the following "
                   "locations are temporarily unavailable and no information "
                   "about instances in these locations can be obtained:\n";
      for (auto const& failed_location : instances->failed_locations) {
        std::cout << failed_location << "\n";
      }
    }

    std::cout << "\nGet Instance:\n";
    auto instance = instance_admin.GetInstance(instance_id);
    if (!instance) {
      throw std::runtime_error(instance.status().message());
    }
    std::cout << "GetInstance details :\n" << instance->DebugString();

    std::cout << "\nListing Clusters:\n";
    auto cluster_list = instance_admin.ListClusters(instance_id);
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

    std::cout << "\nDeleting Instance: ";
    google::cloud::Status status = instance_admin.DeleteInstance(instance_id);
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << " Done\n";
  }
  //! [run instance operations]
  (std::move(instance_admin), instance_id, cluster_id, zone);
}

void CreateAppProfile(google::cloud::bigtable::InstanceAdmin instance_admin,
                      int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"create-app-profile: <project-id> <instance-id> <profile-id>"};
  }
  auto instance_id = ConsumeArg(argc, argv);
  auto profile_id = ConsumeArg(argc, argv);

  //! [create app profile] [START bigtable_create_app_profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string profile_id) {
    auto config = cbt::AppProfileConfig::MultiClusterUseAny(
        cbt::AppProfileId(profile_id));
    StatusOr<google::bigtable::admin::v2::AppProfile> profile =
        instance_admin.CreateAppProfile(cbt::InstanceId(instance_id), config);
    if (!profile) {
      throw std::runtime_error(profile.status().message());
    }
    std::cout << "New profile created with name=" << profile->name() << "\n";
  }
  //! [create app profile] [END bigtable_create_app_profile]
  (std::move(instance_admin), instance_id, profile_id);
}

void CreateAppProfileCluster(
    google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
    char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "create-app-profile-cluster: <project-id> <instance-id> <profile-id>"
        " <cluster-id>"};
  }
  auto instance_id = ConsumeArg(argc, argv);
  auto profile_id = ConsumeArg(argc, argv);
  auto cluster_id = ConsumeArg(argc, argv);

  //! [create app profile cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string profile_id, std::string cluster_id) {
    auto config = cbt::AppProfileConfig::SingleClusterRouting(
        cbt::AppProfileId(profile_id), cbt::ClusterId(cluster_id));
    StatusOr<google::bigtable::admin::v2::AppProfile> profile =
        instance_admin.CreateAppProfile(cbt::InstanceId(instance_id), config);
    if (!profile) {
      throw std::runtime_error(profile.status().message());
    }
    std::cout << "New profile created with name=" << profile->name() << "\n";
  }
  //! [create app profile cluster]
  (std::move(instance_admin), instance_id, profile_id, cluster_id);
}

void GetAppProfile(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"get-app-profile <project-id> <instance-id> <profile-id>"};
  }
  auto instance_id = ConsumeArg(argc, argv);
  auto profile_id = ConsumeArg(argc, argv);

  //! [get app profile] [START bigtable_get_app_profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string profile_id) {
    StatusOr<google::bigtable::admin::v2::AppProfile> profile =
        instance_admin.GetAppProfile(cbt::InstanceId(instance_id),
                                     cbt::AppProfileId(profile_id));
    if (!profile) {
      throw std::runtime_error(profile.status().message());
    }
    std::cout << "Application Profile details=" << profile->DebugString()
              << "\n";
  }
  //! [get app profile] [END bigtable_get_app_profile]
  (std::move(instance_admin), instance_id, profile_id);
}

void UpdateAppProfileDescription(
    google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
    char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "update-app-profile-description: <project-id> <instance-id>"
        " <profile-id> <new-description>"};
  }

  auto instance_id = ConsumeArg(argc, argv);
  auto profile_id = ConsumeArg(argc, argv);
  auto description = ConsumeArg(argc, argv);

  //! [update app profile description] [START bigtable_update_app_profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string profile_id, std::string description) {
    future<StatusOr<google::bigtable::admin::v2::AppProfile>> profile_future =
        instance_admin.UpdateAppProfile(
            cbt::InstanceId(instance_id), cbt::AppProfileId(profile_id),
            cbt::AppProfileUpdateConfig().set_description(description));
    auto profile = profile_future.get();
    if (!profile) {
      throw std::runtime_error(profile.status().message());
    }
    std::cout << "Updated AppProfile: " << profile->DebugString() << "\n";
  }
  //! [update app profile description] [END bigtable_update_app_profile]
  (std::move(instance_admin), instance_id, profile_id, description);
}

void UpdateAppProfileRoutingAny(
    google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
    char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "update-app-profile-routing-any: <project-id> <instance-id>"
        " <profile-id>"};
  }

  auto instance_id = ConsumeArg(argc, argv);
  auto profile_id = ConsumeArg(argc, argv);

  //! [update app profile routing any] [START bigtable_update_app_profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string profile_id) {
    future<StatusOr<google::bigtable::admin::v2::AppProfile>> profile_future =
        instance_admin.UpdateAppProfile(cbt::InstanceId(instance_id),
                                        cbt::AppProfileId(profile_id),
                                        cbt::AppProfileUpdateConfig()
                                            .set_multi_cluster_use_any()
                                            .set_ignore_warnings(true));
    auto profile = profile_future.get();
    if (!profile) {
      throw std::runtime_error(profile.status().message());
    }
    std::cout << "Updated AppProfile: " << profile->DebugString() << "\n";
  }
  //! [update app profile routing any] [END bigtable_update_app_profile]
  (std::move(instance_admin), instance_id, profile_id);
}

void UpdateAppProfileRoutingSingleCluster(
    google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
    char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "update-app-profile-routing: <project-id> <instance-id> <profile-id>"
        " <cluster-id>"};
  }

  auto instance_id = ConsumeArg(argc, argv);
  auto profile_id = ConsumeArg(argc, argv);
  auto cluster_id = ConsumeArg(argc, argv);

  //! [update app profile routing]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string profile_id, std::string cluster_id) {
    future<StatusOr<google::bigtable::admin::v2::AppProfile>> profile_future =
        instance_admin.UpdateAppProfile(
            cbt::InstanceId(instance_id), cbt::AppProfileId(profile_id),
            cbt::AppProfileUpdateConfig()
                .set_single_cluster_routing(cbt::ClusterId(cluster_id))
                .set_ignore_warnings(true));
    auto profile = profile_future.get();
    if (!profile) {
      throw std::runtime_error(profile.status().message());
    }
    std::cout << "Updated AppProfile: " << profile->DebugString() << "\n";
  }
  //! [update app profile routing]
  (std::move(instance_admin), instance_id, profile_id, cluster_id);
}

void ListAppProfiles(google::cloud::bigtable::InstanceAdmin instance_admin,
                     int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-app-profiles: <project-id> <instance-id>"};
  }

  auto instance_id = ConsumeArg(argc, argv);

  //! [list app profiles]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id) {
    StatusOr<std::vector<google::bigtable::admin::v2::AppProfile>> profiles =
        instance_admin.ListAppProfiles(instance_id);
    if (!profiles) {
      throw std::runtime_error(profiles.status().message());
    }
    std::cout << "The " << instance_id << " instance has " << profiles->size()
              << " application profiles\n";
    for (auto const& profile : *profiles) {
      std::cout << profile.DebugString() << "\n";
    }
  }
  //! [list app profiles]
  (std::move(instance_admin), instance_id);
}

void DeleteAppProfile(google::cloud::bigtable::InstanceAdmin instance_admin,
                      int argc, char* argv[]) {
  std::string basic_usage =
      "delete-app-profile: <project-id> <instance-id> <profile-id>"
      " [ignore-warnings (default: true)]";
  if (argc < 3) {
    throw Usage{basic_usage};
  }

  auto instance_id = ConsumeArg(argc, argv);
  auto profile_id = ConsumeArg(argc, argv);
  bool ignore_warnings = true;
  if (argc >= 2) {
    std::string arg = ConsumeArg(argc, argv);
    if (arg == "true") {
      ignore_warnings = true;
    } else if (arg == "false") {
      ignore_warnings = false;
    } else {
      auto msg = basic_usage;
      msg +=
          "\ndelete-app-profile: ignore-warnings parameter must be either"
          " 'true' or 'false'";
      throw Usage{msg};
    }
  }

  //! [delete app profile] [START bigtable_delete_app_profile]
  namespace cbt = google::cloud::bigtable;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string profile_id, bool ignore_warnings) {
    google::cloud::Status status = instance_admin.DeleteAppProfile(
        cbt::InstanceId(instance_id), cbt::AppProfileId(profile_id),
        ignore_warnings);
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "Application Profile deleted\n";
  }
  //! [delete app profile] [END bigtable_delete_app_profile]
  (std::move(instance_admin), instance_id, profile_id, ignore_warnings);
}

void GetIamPolicy(google::cloud::bigtable::InstanceAdmin instance_admin,
                  int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-iam-policy: <project-id> <instance-id>"};
  }

  std::string instance_id = ConsumeArg(argc, argv);

  //! [get iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id) {
    StatusOr<google::cloud::IamPolicy> policy =
        instance_admin.GetIamPolicy(instance_id);
    if (!policy) {
      throw std::runtime_error(policy.status().message());
    }
    std::cout << "The IAM Policy for " << instance_id << " is\n";
    for (auto const& kv : policy->bindings) {
      std::cout << "role " << kv.first << " includes [";
      char const* sep = "";
      for (auto const& member : kv.second) {
        std::cout << sep << member;
        sep = ", ";
      }
      std::cout << "]\n";
    }
  }
  //! [get iam policy]
  (std::move(instance_admin), std::move(instance_id));
}

void SetIamPolicy(google::cloud::bigtable::InstanceAdmin instance_admin,
                  int argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "set-iam-policy: <project-id> <instance-id>"
        " <permission> <new-member>\n"
        "        Example: set-iam-policy my-project my-instance"
        " roles/bigtable.user user:my-user@example.com"};
  }
  std::string instance_id = ConsumeArg(argc, argv);
  std::string role = ConsumeArg(argc, argv);
  std::string member = ConsumeArg(argc, argv);

  //! [set iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string instance_id,
     std::string role, std::string member) {
    StatusOr<google::cloud::IamPolicy> current =
        instance_admin.GetIamPolicy(instance_id);
    if (!current) {
      throw std::runtime_error(current.status().message());
    }
    auto bindings = current->bindings;
    bindings.AddMember(role, member);
    StatusOr<google::cloud::IamPolicy> policy =
        instance_admin.SetIamPolicy(instance_id, bindings, current->etag);
    if (!policy) {
      throw std::runtime_error(policy.status().message());
    }
    std::cout << "The IAM Policy for " << instance_id << " is\n";
    for (auto const& kv : policy->bindings) {
      std::cout << "role " << kv.first << " includes [";
      char const* sep = "";
      for (auto const& m : kv.second) {
        std::cout << sep << m;
        sep = ", ";
      }
      std::cout << "]\n";
    }
  }
  //! [set iam policy]
  (std::move(instance_admin), std::move(instance_id), std::move(role),
   std::move(member));
}

void TestIamPermissions(google::cloud::bigtable::InstanceAdmin instance_admin,
                        int argc, char* argv[]) {
  if (argc < 2) {
    throw Usage{
        "test-iam-permissions: <project-id> <resource-id>"
        " [permission ...]"};
  }
  std::string resource = ConsumeArg(argc, argv);
  std::vector<std::string> permissions;
  while (argc > 1) {
    permissions.emplace_back(ConsumeArg(argc, argv));
  }

  //! [test iam permissions]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string resource,
     std::vector<std::string> permissions) {
    StatusOr<std::vector<std::string>> result =
        instance_admin.TestIamPermissions(resource, permissions);
    if (!result) {
      throw std::runtime_error(result.status().message());
    }
    std::cout << "The current user has the following permissions [";
    char const* sep = "";
    for (auto const& p : *result) {
      std::cout << sep << p;
      sep = ", ";
    }
    std::cout << "]\n";
  }
  //! [test iam permissions]
  (std::move(instance_admin), std::move(resource), std::move(permissions));
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType =
      std::function<void(google::cloud::bigtable::InstanceAdmin, int, char*[])>;

  std::map<std::string, CommandType> commands = {
      {"create-instance", &CreateInstance},
      {"create-replicated-instance", &CreateReplicatedInstance},
      {"update-instance", &UpdateInstance},
      {"list-instances", &ListInstances},
      {"check-instance-exists", &CheckInstanceExists},
      {"get-instance", &GetInstance},
      {"delete-instance", &DeleteInstance},
      {"create-cluster", &CreateCluster},
      {"list-clusters", &ListClusters},
      {"list-all-clusters", &ListAllClusters},
      {"update-cluster", &UpdateCluster},
      {"get-cluster", &GetCluster},
      {"delete-cluster", &DeleteCluster},
      {"create-app-profile", &CreateAppProfile},
      {"create-app-profile-cluster", &CreateAppProfileCluster},
      {"get-app-profile", &GetAppProfile},
      {"update-app-profile-description", &UpdateAppProfileDescription},
      {"update-app-profile-routing-any", &UpdateAppProfileRoutingAny},
      {"update-app-profile-routing", &UpdateAppProfileRoutingSingleCluster},
      {"list-app-profiles", &ListAppProfiles},
      {"delete-app-profile", &DeleteAppProfile},
      {"get-iam-policy", &GetIamPolicy},
      {"set-iam-policy", &SetIamPolicy},
      {"test-iam-permissions", &TestIamPermissions},
      {"run", &RunInstanceOperations},
      {"create-dev-instance", &CreateDevInstance},
  };

  {
    // Force each command to generate its Usage string, so we can provide a good
    // usage string for the whole program. We need to create an InstanceAdmin
    // object to do this, but that object is never used, it is passed to the
    // commands, without any calls made to it.
    google::cloud::bigtable::InstanceAdmin unused(
        google::cloud::bigtable::CreateDefaultInstanceAdminClient(
            "unused-project", google::cloud::bigtable::ClientOptions()));
    for (auto&& kv : commands) {
      try {
        int fake_argc = 0;
        kv.second(unused, fake_argc, argv);
      } catch (Usage const& u) {
        command_usage += "    ";
        command_usage += u.msg;
        command_usage += "\n";
      }
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

  // Create an instance admin endpoint.
  //! [connect instance admin]
  google::cloud::bigtable::InstanceAdmin instance_admin(
      google::cloud::bigtable::CreateDefaultInstanceAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()));
  //! [connect instance admin]

  command->second(instance_admin, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
//! [all code]
