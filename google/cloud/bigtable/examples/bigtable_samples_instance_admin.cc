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

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  if (argc < 3) {
    PrintUsage(argc, argv, "Missing command");
    return 1;
  }

  std::string const command = ConsumeArg(argc, argv);
  std::string const project_id = ConsumeArg(argc, argv);

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

  if (command == "create-instance") {
    CreateInstance(instance_admin, argc, argv);
  } else if (command == "update-instance") {
    UpdateInstance(instance_admin, argc, argv);
  } else if (command == "list-instances") {
    ListInstances(instance_admin, argc, argv);
  } else if (command == "get-instance") {
    GetInstance(instance_admin, argc, argv);
  } else if (command == "delete-instance") {
    DeleteInstance(instance_admin, argc, argv);
  } else if (command == "list-clusters") {
    ListClusters(instance_admin, argc, argv);
  } else if (command == "list-all-clusters") {
    ListAllClusters(instance_admin, argc, argv);
  } else if (command == "update-cluster") {
    UpdateCluster(instance_admin, argc, argv);
  } else if (command == "get-cluster") {
    GetCluster(instance_admin, argc, argv);
  } else if (command == "delete-cluster") {
    DeleteCluster(instance_admin, argc, argv);
  } else {
    std::string msg("Unknown_command: " + command);
    PrintUsage(argc, argv, msg);
  }

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
