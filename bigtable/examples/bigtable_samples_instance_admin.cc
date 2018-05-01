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

//! [create instance]
void CreateInstance(bigtable::InstanceAdmin instance_admin,
                    std::string const& project_id,
                    std::string const& instance_id) {
  // TODO(#418) implement tests and examples for CreateInstance
}
//! [create instance]

//! [list instances]
void ListInstances(bigtable::InstanceAdmin instance_admin) {
  auto instances = instance_admin.ListInstances();
  for (auto const& instance : instances) {
    std::cout << instance.name() << std::endl;
  }
}
//! [list instances]

//! [get instance]
void GetInstance(bigtable::InstanceAdmin instance_admin,
                 std::string const& project_id,
                 std::string const& instance_id) {
  // TODO(#419) implement tests and examples for GetInstance
}
//! [get instance]

//! [delete instance]
void DeleteInstance(bigtable::InstanceAdmin instance_admin,
                    std::string const& instance_id) {
  instance_admin.DeleteInstance(instance_id);
}
//! [delete instance]

//! [list cluster]
void ListClusters(bigtable::InstanceAdmin& instance_admin) {
  auto cluster_list = instance_admin.ListClusters();
  std::cout << "Cluster Name List" << std::endl;
  for (auto const& cluster : cluster_list) {
    std::cout << "Cluster Name:" << cluster.name() << std::endl;
  }
}
//! [list cluster]

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  auto print_usage = [argv]() {
    std::string const cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    auto program = cmd.substr(last_slash + 1);
    std::cerr << "Usage: " << program << " <command> <project_id> <instance_id>"
              << "\n\n"
              << "Examples:\n"
              << "  " << program << " create-instance my-project my-instance\n"
              << "  " << program << " list-clusters my-project my-instance"
              << std::endl;
  };

  if (argc != 4) {
    print_usage();
    return 1;
  }

  std::string const command = argv[1];
  std::string const project_id = argv[2];
  std::string const instance_id = argv[3];

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
    CreateInstance(instance_admin, project_id, instance_id);
  } else if (command == "list-instances") {
    ListInstances(instance_admin);
  } else if (command == "get-instance") {
    GetInstance(instance_admin, project_id, instance_id);
  } else if (command == "delete-instance") {
    DeleteInstance(instance_admin, instance_id);
  } else if (command == "list-clusters") {
    ListClusters(instance_admin);
  } else {
    std::cerr << "Unknown command: " << command << std::endl;
    print_usage();
  }

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
