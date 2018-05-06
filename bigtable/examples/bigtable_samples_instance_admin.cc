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

#include "bigtable/client/instance_admin.h"
#include "bigtable/client/instance_admin_client.h"

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
void CreateInstance(bigtable::InstanceAdmin instance_admin, int argc,
                    char* argv[]) {
  if (argc != 3) {
    throw Usage{"create-instance: <instance-id> <zone>"};
  }
  std::string const instance_id = ConsumeArg(argc, argv);
  std::string const zone = ConsumeArg(argc, argv);

  bigtable::DisplayName display_name("Put description here");
  std::string cluster_id = instance_id + "-c1";
  auto cluster_config =
      bigtable::ClusterConfig(zone, 0, bigtable::ClusterConfig::HDD);
  bigtable::InstanceConfig config(bigtable::InstanceId(instance_id),
                                  display_name, {{cluster_id, cluster_config}});
  config.set_type(bigtable::InstanceConfig::DEVELOPMENT);

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

//! [list instances]
void ListInstances(bigtable::InstanceAdmin instance_admin, int argc,
                   char* argv[]) {
  auto instances = instance_admin.ListInstances();
  for (auto const& instance : instances) {
    std::cout << instance.name() << std::endl;
  }
}
//! [list instances]

//! [get instance]
void GetInstance(bigtable::InstanceAdmin instance_admin, int argc,
                 char* argv[]) {
  // TODO(#419) implement tests and examples for GetInstance
}
//! [get instance]

//! [delete instance]
void DeleteInstance(bigtable::InstanceAdmin instance_admin, int argc,
                    char* argv[]) {
  if (argc != 2) {
    throw Usage{"delete-instance: <project-id> <instance-id>"};
  }
  std::string instance_id = ConsumeArg(argc, argv);
  instance_admin.DeleteInstance(instance_id);
}
//! [delete instance]

//! [list clusters]
void ListClusters(bigtable::InstanceAdmin instance_admin, int argc,
                  char* argv[]) {
  auto cluster_list = instance_admin.ListClusters();
  std::cout << "Cluster Name List" << std::endl;
  for (auto const& cluster : cluster_list) {
    std::cout << "Cluster Name:" << cluster.name() << std::endl;
  }
}
//! [list clusters]

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
  std::shared_ptr<bigtable::InstanceAdminClient> instance_admin_client(
      bigtable::CreateDefaultInstanceAdminClient(project_id,
                                                 bigtable::ClientOptions()));
  //! [connect instance admin client]

  // Connect to the Cloud Bigtable endpoint.
  //! [connect instance admin]
  bigtable::InstanceAdmin instance_admin(instance_admin_client);
  //! [connect instance admin]

  if (command == "create-instance") {
    CreateInstance(instance_admin, argc, argv);
  } else if (command == "list-instances") {
    ListInstances(instance_admin, argc, argv);
  } else if (command == "get-instance") {
    GetInstance(instance_admin, argc, argv);
  } else if (command == "delete-instance") {
    DeleteInstance(instance_admin, argc, argv);
  } else if (command == "list-clusters") {
    ListClusters(instance_admin, argc, argv);
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
