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

namespace btproto = ::google::bigtable::admin::v2;

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

void PrintUsage(int argc, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program
            << " <command> <project_id> [arguments]\n\n"
            << "Examples:\n";
  for (auto example : {"create-instance my-project my-instance us-central1-f",
                       "list-clusters my-project my-instance"}) {
    std::cerr << "  " << program << " " << example << "\n";
  }
  std::cerr << std::flush;
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
      std::cout << "DONE: " << future.get().name() << std::endl;
      return;
    }
    std::cout << '.' << std::flush;
  }
  std::cout << "TIMEOUT" << std::endl;
}
//! [create instance]

//! [update instance]
void UpdateInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                    int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"update-instance: <project-id> <instance-id>"};
  }
  std::string instance_id = ConsumeArg(argc, argv);
  auto instance = instance_admin.GetInstance(instance_id);
  // Modify the instance and prepare the mask with modified field
  google::cloud::bigtable::InstanceUpdateConfig instance_update_config(
      std::move(instance));
  instance_update_config.set_display_name("Modified Display Name");

  auto future =
      instance_admin.UpdateInstance(std::move(instance_update_config));
  std::string instance_detail;
  google::protobuf::TextFormat::PrintToString(future.get(), &instance_detail);
  std::cout << "GetInstance details : " << instance_detail << std::endl;
}
//! [update instance]

//! [list instances]
void ListInstances(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  auto instances = instance_admin.ListInstances();
  for (auto const& instance : instances) {
    std::cout << instance.name() << std::endl;
  }
}
//! [list instances]

//! [get instance]
void GetInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                 int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-instance: <project-id> <instance-id>"};
  }
  std::string instance_id = ConsumeArg(argc, argv);
  auto instance = instance_admin.GetInstance(instance_id);
  std::string instance_detail;
  google::protobuf::TextFormat::PrintToString(instance, &instance_detail);
  std::cout << "GetInstance details : " << instance_detail << std::endl;
}
//! [get instance]

//! [delete instance]
void DeleteInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                    int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"delete-instance: <project-id> <instance-id>"};
  }
  std::string instance_id = ConsumeArg(argc, argv);
  instance_admin.DeleteInstance(instance_id);
}
//! [delete instance]

//! [create cluster]
// Before creating cluster, need to create instance first, then create cluster
// on it.
void CreateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"create-cluster: <project-id> <instance-id> <cluster-id>"};
  }

  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  auto location = "us-central1-a";
  auto cluster_config = google::cloud::bigtable::ClusterConfig(
      location, 3, google::cloud::bigtable::ClusterConfig::HDD);
  auto cluster =
      instance_admin.CreateCluster(cluster_config, instance_id, cluster_id);
  std::cout << "Cluster Created " << cluster.get().name() << std::endl;
}
//! [create cluster]

//! [list clusters]
void ListClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                  int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-clusters <project-id> <instance-id>"};
  }

  auto cluster_list = instance_admin.ListClusters(ConsumeArg(argc, argv));
  std::cout << "Cluster Name List" << std::endl;
  for (auto const& cluster : cluster_list) {
    std::cout << "Cluster Name:" << cluster.name() << std::endl;
  }
}
//! [list clusters]

//! [list all clusters]
void ListAllClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                     int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"list-all-clusters <project-id>"};
  }

  auto cluster_list = instance_admin.ListClusters();
  std::cout << "Cluster Name List" << std::endl;
  for (auto const& cluster : cluster_list) {
    std::cout << "Cluster Name:" << cluster.name() << std::endl;
  }
}
//! [list all clusters]

//! [update cluster]
void UpdateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"update-cluster: <project-id> <instance-id> <cluster-id>"};
  }
  // CreateCluster or GetCluster first and then modify it
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  auto cluster_config = google::cloud::bigtable::ClusterConfig(
      "us-central1-f", 0, google::cloud::bigtable::ClusterConfig::HDD);
  auto cluster =
      instance_admin.CreateCluster(cluster_config, instance_id, cluster_id)
          .get();

  // Modify the cluster
  cluster.set_serve_nodes(2);
  auto modified_config =
      google::cloud::bigtable::ClusterConfig(std::move(cluster));

  auto modified_cluster = instance_admin.UpdateCluster(cluster_config).get();

  std::string cluster_detail;
  google::protobuf::TextFormat::PrintToString(modified_cluster,
                                              &cluster_detail);
  std::cout << "cluster details : " << cluster_detail << std::endl;
}
//! [update cluster]

//! [get cluster]
void GetCluster(google::cloud::bigtable::InstanceAdmin instance_admin, int argc,
                char* argv[]) {
  if (argc != 3) {
    throw Usage{"get-cluster: <project-id> <instance-id> <cluster-id>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  auto cluster = instance_admin.GetCluster(instance_id, cluster_id);
  std::string cluster_detail;
  google::protobuf::TextFormat::PrintToString(cluster, &cluster_detail);
  std::cout << "GetCluster details : " << cluster_detail << std::endl;
}
//! [get cluster]

//! [delete cluster]
void DeleteCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"delete-cluster: <project-id> <instance-id> <cluster-id>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::ClusterId cluster_id(ConsumeArg(argc, argv));
  instance_admin.DeleteCluster(instance_id, cluster_id);
}
//! [delete cluster]

//! [create app profile]
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
  std::cout << "New profile created with name=" << profile.name() << std::endl;
}
//! [create app profile]

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
  std::cout << "New profile created with name=" << profile.name() << std::endl;
}
//! [create app profile cluster]

//! [get app profile]
void GetAppProfile(google::cloud::bigtable::InstanceAdmin instance_admin,
                   int argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{"get-app-profile: <project-id> <instance-id> <profile-id>"};
  }
  google::cloud::bigtable::InstanceId instance_id(ConsumeArg(argc, argv));
  google::cloud::bigtable::AppProfileId profile_id(ConsumeArg(argc, argv));
  auto profile = instance_admin.GetAppProfile(instance_id, profile_id);
  std::string detail;
  google::protobuf::TextFormat::PrintToString(profile, &detail);
  std::cout << "Application Profile details=" << detail << std::endl;
}
//! [get app profile]

//! [list app profiles]
void ListAppProfiles(google::cloud::bigtable::InstanceAdmin instance_admin,
                     int argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-app-profiles: <project-id> <instance-id>"};
  }
  std::string instance_id(ConsumeArg(argc, argv));
  auto profiles = instance_admin.ListAppProfiles(instance_id);
  std::cout << "The " << instance_id << " instance has " << profiles.size()
            << " application profiles" << std::endl;
  for (auto const& profile : profiles) {
    std::string detail;
    google::protobuf::TextFormat::PrintToString(profile, &detail);
    std::cout << detail << std::endl;
  }
}
//! [list app profiles]

//! [delete app profile]
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
  instance_admin.DeleteAppProfile(instance_id, profile_id, ignore_warnings);
  std::cout << "Application Profile deleted" << std::endl;
}
//! [delete app profile]

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
      {"list-app-profiles", &ListAppProfiles},
      {"delete-app-profile", &DeleteAppProfile},
  };

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

  // Connect to the Cloud Bigtable admin endpoint.
  //! [connect instance admin client]
  auto instance_admin_client(
      google::cloud::bigtable::CreateDefaultInstanceAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()));
  //! [connect instance admin client]

  // Connect to the Cloud Bigtable endpoint.
  //! [connect instance admin]
  google::cloud::bigtable::InstanceAdmin instance_admin(instance_admin_client);
  //! [connect instance admin]

  command->second(instance_admin, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
