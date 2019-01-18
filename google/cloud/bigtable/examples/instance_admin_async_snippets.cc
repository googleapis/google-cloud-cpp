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
namespace cbt = google::cloud::bigtable;

struct Usage {
  std::string msg;
};

std::string command_usage;

void PrintUsage(std::string const& cmd, std::string const& msg) {
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << std::endl;
}

void AsyncGetInstance(cbt::InstanceAdmin instance_admin,
                      cbt::CompletionQueue cq, std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-get-instance: <project-id> <instance-id>"};
  }

  //! [async get instance]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    google::cloud::future<google::bigtable::admin::v2::Instance> future =
        instance_admin.AsyncGetInstance(cq, instance_id);

    auto final = future.then(
        [](google::cloud::future<google::bigtable::admin::v2::Instance> f) {
          auto instance = f.get();
          std::string instance_detail;
          google::protobuf::TextFormat::PrintToString(instance,
                                                      &instance_detail);
          std::cout << "GetInstance details : " << instance_detail << std::endl;
        });

    final.get();
  }
  //! [async get instance]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncListInstances(cbt::InstanceAdmin instance_admin,
                        cbt::CompletionQueue cq,
                        std::vector<std::string> argv) {
  if (argv.size() != 1U) {
    throw Usage{"async-list-instances: <project-id>"};
  }

  //! [async list instances]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq) {
    google::cloud::future<google::cloud::bigtable::v0::InstanceList> future =
        instance_admin.AsyncListInstances(cq);

    auto final = future.then(
        [](google::cloud::future<google::cloud::bigtable::v0::InstanceList> f) {
          auto instance_list = f.get();
          for (const auto& instance : instance_list.instances) {
            std::cout << instance.name() << std::endl;
          }
          if (!instance_list.failed_locations.empty()) {
            std::cout << "The Cloud Bigtable service reports that it could not "
                         "retrieve data for the following zones:\n";
            for (const auto& failed_location : instance_list.failed_locations) {
              std::cout << failed_location << std::endl;
            }
            std::cout
                << "This is typically a transient condition, try again later."
                << std::endl;
          }
        });
    final.get();
  }
  //! [async list instances]
  (std::move(instance_admin), std::move(cq));
}

void AsyncGetCluster(cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
                     std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{"async-get-cluster: <project-id> <instance-id> <cluster-id>"};
  }

  //! [async get cluster]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string cluster_id) {
    google::cloud::bigtable::InstanceId instance_id1(instance_id);
    google::cloud::bigtable::ClusterId cluster_id1(cluster_id);

    google::cloud::future<google::bigtable::admin::v2::Cluster> future =
        instance_admin.AsyncGetCluster(cq, instance_id1, cluster_id1);

    auto final = future.then(
        [](google::cloud::future<google::bigtable::admin::v2::Cluster> f) {
          auto cluster = f.get();
          std::string cluster_detail;
          google::protobuf::TextFormat::PrintToString(cluster, &cluster_detail);
          std::cout << "GetCluster details : " << cluster_detail << std::endl;
        });

    final.get();
  }
  //! [async get cluster]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncListClusters(cbt::InstanceAdmin instance_admin,
                       cbt::CompletionQueue cq, std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-list-clusters: <project-id> <instance-id>"};
  }

  //! [async list clusters]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    google::cloud::future<google::cloud::bigtable::v0::ClusterList> future =
        instance_admin.AsyncListClusters(cq, instance_id);

    auto final = future.then(
        [](google::cloud::future<google::cloud::bigtable::v0::ClusterList> f) {
          auto cluster_list = f.get();
          std::cout << "Cluster Name List" << std::endl;
          for (const auto& cluster : cluster_list.clusters) {
            std::cout << cluster.name() << std::endl;
          }
          if (!cluster_list.failed_locations.empty()) {
            std::cout << "The Cloud Bigtable service reports that it could not "
                         "retrieve data for the following zones:\n";
            for (const auto& failed_location : cluster_list.failed_locations) {
              std::cout << failed_location << std::endl;
            }
            std::cout
                << "This is typically a transient condition, try again later."
                << std::endl;
          }
        });

    final.get();
  }
  //! [async list clusters]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncListAllClusters(cbt::InstanceAdmin instance_admin,
                          cbt::CompletionQueue cq,
                          std::vector<std::string> argv) {
  if (argv.size() != 1U) {
    throw Usage{"async-list-all-clusters: <project-id>"};
  }

  //! [async list all clusters]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq) {
    google::cloud::future<google::cloud::bigtable::v0::ClusterList> future =
        instance_admin.AsyncListClusters(cq);

    auto final = future.then(
        [](google::cloud::future<google::cloud::bigtable::v0::ClusterList> f) {
          auto cluster_list = f.get();
          std::cout << "Cluster Name List" << std::endl;
          for (const auto& cluster : cluster_list.clusters) {
            std::cout << cluster.name() << std::endl;
          }
          if (!cluster_list.failed_locations.empty()) {
            std::cout << "The Cloud Bigtable service reports that it could not "
                         "retrieve data for the following zones:\n";
            for (const auto& failed_location : cluster_list.failed_locations) {
              std::cout << failed_location << std::endl;
            }
            std::cout
                << "This is typically a transient condition, try again later."
                << std::endl;
          }
        });

    final.get();
  }
  //! [async list all clusters]
  (std::move(instance_admin), std::move(cq));
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(
      google::cloud::bigtable::InstanceAdmin,
      google::cloud::bigtable::CompletionQueue, std::vector<std::string>)>;

  std::map<std::string, CommandType> commands = {
      {"async-get-instance", &AsyncGetInstance},
      {"async-get-cluster", &AsyncGetCluster},
      {"async-list-instances", &AsyncListInstances},
      {"async-list-clusters", &AsyncListClusters},
      {"async-list-all-clusters", &AsyncListAllClusters}};

  google::cloud::bigtable::CompletionQueue cq;

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
        kv.second(unused, cq, {});
      } catch (Usage const& u) {
        command_usage += "    ";
        command_usage += u.msg;
        command_usage += "\n";
      }
    }
  }

  if (argc < 3) {
    PrintUsage(argv[0], "Missing command and/or project-id");
    return 1;
  }

  std::vector<std::string> args;
  args.emplace_back(argv[0]);
  std::string const command_name = argv[1];
  std::string const project_id = argv[2];
  std::transform(argv + 3, argv + argc, std::back_inserter(args),
                 [](char* x) { return std::string(x); });

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    PrintUsage(argv[0], "Unknown command: " + command_name);
    return 1;
  }

  // Start a thread to run the completion queue event loop.
  std::thread runner([&cq] { cq.Run(); });

  // Create an instance admin endpoint.
  //! [connect instance admin]
  google::cloud::bigtable::InstanceAdmin instance_admin(
      google::cloud::bigtable::CreateDefaultInstanceAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()));
  //! [connect instance admin]

  command->second(instance_admin, cq, args);

  // Shutdown the completion queue event loop and join the thread.
  cq.Shutdown();
  runner.join();

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argv[0], ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
//! [all code]
