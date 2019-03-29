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
#include "google/cloud/bigtable/instance_admin_client.h"
//! [bigtable includes]
#include <google/protobuf/text_format.h>

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

//! [create instance]
void CreateInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                    int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"create-instance: <instance-id> <zone>"};
  }
  std::string const instance_id = ConsumeArg(argc, argv);
  std::string const zone = ConsumeArg(argc, argv);

  google::cloud::bigtable::DisplayName display_name("Put description here");
  std::string cluster_id = instance_id + "-c1";
  auto cluster_config = google::cloud::bigtable::ClusterConfig(
      zone, 3, google::cloud::bigtable::ClusterConfig::HDD);
  google::cloud::bigtable::InstanceConfig config(
      google::cloud::bigtable::InstanceId(instance_id), display_name,
      {{cluster_id, cluster_config}});
  config.set_type(google::cloud::bigtable::InstanceConfig::PRODUCTION);

  auto future = instance_admin.CreateInstance(config);
  // Most applications would simply call future.get(), here we show how to
  // perform additional work while the long running operation completes.
  std::cout << "Waiting for instance creation to complete ";
  for (int i = 0; i != 100; ++i) {
    if (std::future_status::ready == future.wait_for(std::chrono::seconds(2))) {
      auto instance = future.get();
      if (!instance) {
        throw std::runtime_error(instance.status().message());
      }
      std::cout << "DONE: " << instance->name() << "\n";
      return;
    }
    std::cout << '.' << std::flush;
  }
  std::cout << "TIMEOUT\n";
}
//! [create instance]

//! [create dev instance]
void CreateDevInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                       int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"create-dev-instance: <instance-id> <zone>"};
  }
  std::string const instance_id = ConsumeArg(argc, argv);
  std::string const zone = ConsumeArg(argc, argv);

  google::cloud::bigtable::DisplayName display_name("Put description here");
  std::string cluster_id = instance_id + "-c1";
  auto cluster_config = google::cloud::bigtable::ClusterConfig(
      zone, 0, google::cloud::bigtable::ClusterConfig::HDD);
  google::cloud::bigtable::InstanceConfig config(
      google::cloud::bigtable::InstanceId(instance_id), display_name,
      {{cluster_id, cluster_config}});
  config.set_type(google::cloud::bigtable::InstanceConfig::DEVELOPMENT);

  auto future = instance_admin.CreateInstance(config);
  // Most applications would simply call future.get(), here we show how to
  // perform additional work while the long running operation completes.
  std::cout << "Waiting for instance creation to complete ";
  for (int i = 0; i != 100; ++i) {
    if (std::future_status::ready == future.wait_for(std::chrono::seconds(2))) {
      auto instance = future.get();
      if (!instance) {
        throw std::runtime_error(instance.status().message());
      }
      std::cout << "DONE: " << instance->name() << "\n";
      return;
    }
    std::cout << '.' << std::flush;
  }
  std::cout << "TIMEOUT\n";
}
//! [create dev instance]

//! [update instance]
void UpdateInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                    int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"update-instance: <project-id> <instance-id>"};
  }
  std::string instance_id = ConsumeArg(argc, argv);
  auto instance = instance_admin.GetInstance(instance_id);
  if (!instance) {
    throw std::runtime_error(instance.status().message());
  }
  // Modify the instance and prepare the mask with modified field
  google::cloud::bigtable::InstanceUpdateConfig instance_update_config(
      std::move(*instance));
  instance_update_config.set_display_name("Modified Display Name");

  auto future =
      instance_admin.UpdateInstance(std::move(instance_update_config));
  auto updated_instance = future.get();
  if (!updated_instance) {
    throw std::runtime_error(updated_instance.status().message());
  }
  std::string instance_detail;
  google::protobuf::TextFormat::PrintToString(*updated_instance,
                                              &instance_detail);
  std::cout << "GetInstance details : " << instance_detail << "\n";
}
//! [update instance]

//! [list instances]
void ListInstances(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"list-instances: <project-id>"};
  }
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
}
//! [list instances]

//! [get instance] [START bigtable_get_instance]
void GetInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                 int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-instance: <project-id> <instance-id>"};
  }
  std::string instance_id = ConsumeArg(argc, argv);
  auto instance = instance_admin.GetInstance(instance_id);
  if (!instance) {
    throw std::runtime_error(instance.status().message());
  }
  std::string instance_detail;
  google::protobuf::TextFormat::PrintToString(*instance, &instance_detail);
  std::cout << "GetInstance details : " << instance_detail << "\n";
}
//! [get instance] [END bigtable_get_instance]

//! [delete instance]
void DeleteInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                    int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"delete-instance: <project-id> <instance-id>"};
  }
  std::string instance_id = ConsumeArg(argc, argv);
  google::cloud::Status status = instance_admin.DeleteInstance(instance_id);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
}
//! [delete instance]

//! [create cluster] [START bigtable_create_cluster]
// Before creating cluster, need to create a production instance first,
// then create cluster on it.
void CreateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "create-cluster: <project-id> <instance-id> <cluster-id> <zone>"};
  }

  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  std::string const zone = ConsumeArg(argc, argv);

  auto cluster_config = google::cloud::bigtable::ClusterConfig(
      zone, 3, google::cloud::bigtable::ClusterConfig::HDD);
  auto cluster =
      instance_admin.CreateCluster(cluster_config, instance_id, cluster_id);
  std::cout << "Cluster Created " << cluster_id.get() << "\n";
}
//! [create cluster] [END bigtable_create_cluster]

//! [list clusters]
void ListClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                  int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-clusters <project-id> <instance-id>"};
  }

  auto cluster_list = instance_admin.ListClusters(ConsumeArg(argc, argv));
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
//! [list clusters]

//! [list all clusters] [START bigtable_get_clusters]
void ListAllClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                     int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"list-all-clusters <project-id>"};
  }

  auto cluster_list = instance_admin.ListClusters();
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

//! [update cluster]
void UpdateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"update-cluster: <project-id> <instance-id> <cluster-id>"};
  }
  // GetCluster first and then modify it.
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  auto cluster = instance_admin.GetCluster(instance_id, cluster_id);
  if (!cluster) {
    throw std::runtime_error(cluster.status().message());
  }

  // Modify the cluster.
  cluster->set_serve_nodes(4);
  auto modified_config =
      google::cloud::bigtable::ClusterConfig(std::move(*cluster));

  auto modified_cluster = instance_admin.UpdateCluster(modified_config).get();
  if (!modified_cluster) {
    throw std::runtime_error(modified_cluster.status().message());
  }
  std::string cluster_detail;
  google::protobuf::TextFormat::PrintToString(*modified_cluster,
                                              &cluster_detail);
  std::cout << "cluster details : " << cluster_detail << "\n";
}
//! [update cluster]

//! [get cluster] [START bigtable_get_cluster]
void GetCluster(google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
                char* argv[]) {
  if (argc != 3) {
    throw Usage{"get-cluster: <project-id> <instance-id> <cluster-id>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  auto cluster = instance_admin.GetCluster(instance_id, cluster_id);
  if (!cluster) {
    throw std::runtime_error(cluster.status().message());
  }
  std::string cluster_detail;
  google::protobuf::TextFormat::PrintToString(*cluster, &cluster_detail);
  std::cout << "GetCluster details : " << cluster_detail << "\n";
}
//! [get cluster] [END bigtable_get_cluster]

//! [delete cluster] [START bigtable_delete_cluster]
void DeleteCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"delete-cluster: <project-id> <instance-id> <cluster-id>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  google::cloud::Status status =
      instance_admin.DeleteCluster(instance_id, cluster_id);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
}
//! [delete cluster] [END bigtable_delete_cluster]

//! [run instance operations]
void RunInstanceOperations(
    google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
    char* argv[]) {
  if (argc != 4) {
    throw Usage{"run: <project-id> <instance-id> <cluster-id> <zone>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  std::string const zone = ConsumeArg(argc, argv);

  google::cloud::bigtable::DisplayName display_name("Put description here");
  auto cluster_config = google::cloud::bigtable::ClusterConfig(
      zone, 3, google::cloud::bigtable::ClusterConfig::HDD);
  google::cloud::bigtable::InstanceConfig config(
      google::cloud::bigtable::InstanceId(instance_id), display_name,
      {{cluster_id.get(), cluster_config}});
  config.set_type(google::cloud::bigtable::InstanceConfig::PRODUCTION);

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
  auto instance = instance_admin.GetInstance(instance_id.get());
  if (!instance) {
    throw std::runtime_error(instance.status().message());
  }
  std::string instance_detail;
  google::protobuf::TextFormat::PrintToString(*instance, &instance_detail);
  std::cout << "GetInstance details :\n" << instance_detail;

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

  std::cout << "\nDeleting Instance: ";
  google::cloud::Status status =
      instance_admin.DeleteInstance(instance_id.get());
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
  std::cout << " Done\n";
}
//! [run instance operations]

//! [create app profile] [START bigtable_create_app_profile]
void CreateAppProfile(google::cloud::bigtable::InstanceAdmin instance_admin,
                      int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"create-app-profile: <project-id> <instance-id> <profile-id>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::AppProfileId profile_id(ConsumeArg(argc, argv));
  auto config =
      google::cloud::bigtable::AppProfileConfig::MultiClusterUseAny(profile_id);
  auto profile = instance_admin.CreateAppProfile(instance_id, config);
  if (!profile) {
    throw std::runtime_error(profile.status().message());
  }
  std::cout << "New profile created with name=" << profile->name() << "\n";
}
//! [create app profile] [END bigtable_create_app_profile]

//! [create app profile cluster]
void CreateAppProfileCluster(
    google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
    char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "create-app-profile-cluster: <project-id> <instance-id> <profile-id>"
        " <cluster-id>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::AppProfileId profile_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  auto config = google::cloud::bigtable::AppProfileConfig::SingleClusterRouting(
      profile_id, cluster_id);
  auto profile = instance_admin.CreateAppProfile(instance_id, config);
  if (!profile) {
    throw std::runtime_error(profile.status().message());
  }
  std::cout << "New profile created with name=" << profile->name() << "\n";
}
//! [create app profile cluster]

//! [get app profile] [START bigtable_get_app_profile]
void GetAppProfile(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"get-app-profile: <project-id> <instance-id> <profile-id>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::AppProfileId profile_id(ConsumeArg(argc, argv));
  auto profile = instance_admin.GetAppProfile(instance_id, profile_id);
  if (!profile) {
    throw std::runtime_error(profile.status().message());
  }
  std::string detail;
  google::protobuf::TextFormat::PrintToString(*profile, &detail);
  std::cout << "Application Profile details=" << detail << "\n";
}
//! [get app profile] [END bigtable_get_app_profile]

//! [update app profile description]
void UpdateAppProfileDescription(
    google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
    char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "update-app-profile-description: <project-id> <instance-id>"
        " <profile-id> <new-description>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::AppProfileId profile_id(ConsumeArg(argc, argv));
  std::string description = ConsumeArg(argc, argv);
  auto profile_future = instance_admin.UpdateAppProfile(
      instance_id, profile_id,
      google::cloud::bigtable::AppProfileUpdateConfig().set_description(
          description));
  auto profile = profile_future.get();
  if (!profile) {
    throw std::runtime_error(profile.status().message());
  }
  std::string detail;
  google::protobuf::TextFormat::PrintToString(*profile, &detail);
  std::cout << "Application Profile details=" << detail << "\n";
}
//! [update app profile description]

//! [update app profile routing any] [START bigtable_update_app_profile]
void UpdateAppProfileRoutingAny(
    google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
    char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "update-app-profile-routing-any: <project-id> <instance-id>"
        " <profile-id>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::AppProfileId profile_id(ConsumeArg(argc, argv));
  auto profile_future = instance_admin.UpdateAppProfile(
      instance_id, profile_id,
      google::cloud::bigtable::AppProfileUpdateConfig()
          .set_multi_cluster_use_any()
          .set_ignore_warnings(true));
  auto profile = profile_future.get();
  if (!profile) {
    throw std::runtime_error(profile.status().message());
  }
  std::string detail;
  google::protobuf::TextFormat::PrintToString(*profile, &detail);
  std::cout << "Application Profile details=" << detail << "\n";
}
//! [update app profile routing any] [END bigtable_update_app_profile]

//! [update app profile routing]
void UpdateAppProfileRoutingSingleCluster(
    google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
    char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "update-app-profile-routing: <project-id> <instance-id> <profile-id>"
        " <cluster-id>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::AppProfileId profile_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  auto profile_future = instance_admin.UpdateAppProfile(
      instance_id, profile_id,
      google::cloud::bigtable::AppProfileUpdateConfig()
          .set_single_cluster_routing(cluster_id)
          .set_ignore_warnings(true));
  auto profile = profile_future.get();
  if (!profile) {
    throw std::runtime_error(profile.status().message());
  }
  std::string detail;
  google::protobuf::TextFormat::PrintToString(*profile, &detail);
  std::cout << "Application Profile details=" << detail << "\n";
}
//! [update app profile routing]

//! [list app profiles] [START bigtable_get_app_profiles]
void ListAppProfiles(google::cloud::bigtable::InstanceAdmin instance_admin,
                     int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-app-profiles: <project-id> <instance-id>"};
  }
  std::string instance_id(ConsumeArg(argc, argv));
  auto profiles = instance_admin.ListAppProfiles(instance_id);
  if (!profiles) {
    throw std::runtime_error(profiles.status().message());
  }
  std::cout << "The " << instance_id << " instance has " << profiles->size()
            << " application profiles\n";
  for (auto const& profile : *profiles) {
    std::string detail;
    google::protobuf::TextFormat::PrintToString(profile, &detail);
    std::cout << detail << "\n";
  }
}
//! [list app profiles] [END bigtable_get_app_profiles]

//! [delete app profile] [START bigtable_delete_app_profile]
void DeleteAppProfile(google::cloud::bigtable::InstanceAdmin instance_admin,
                      int argc, char* argv[]) {
  std::string basic_usage =
      "delete-app-profile: <project-id> <instance-id> <profile-id>"
      " [ignore-warnings (default: true)]";
  if (argc < 3) {
    throw Usage{basic_usage};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::AppProfileId profile_id(ConsumeArg(argc, argv));
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
  google::cloud::Status status =
      instance_admin.DeleteAppProfile(instance_id, profile_id, ignore_warnings);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
  std::cout << "Application Profile deleted\n";
}
//! [delete app profile] [END bigtable_delete_app_profile]

//! [get iam policy]
void GetIamPolicy(google::cloud::bigtable::InstanceAdmin instance_admin,
                  int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-iam-policy: <project-id> <instance-id>"};
  }
  std::string instance_id = ConsumeArg(argc, argv);
  auto policy = instance_admin.GetIamPolicy(instance_id);
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

//! [set iam policy]
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
  auto current = instance_admin.GetIamPolicy(instance_id);
  if (!current) {
    throw std::runtime_error(current.status().message());
  }
  auto bindings = current->bindings;
  bindings.AddMember(role, member);
  auto policy =
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

//! [test iam permissions]
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
    permissions.push_back(ConsumeArg(argc, argv));
  }
  auto result = instance_admin.TestIamPermissions(resource, permissions);
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

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(google::cloud::bigtable::InstanceAdmin,
                                         int, char* [])>;

  std::map<std::string, CommandType> commands = {
      {"create-instance", &CreateInstance},
      {"update-instance", &UpdateInstance},
      {"list-instances", &ListInstances},
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
