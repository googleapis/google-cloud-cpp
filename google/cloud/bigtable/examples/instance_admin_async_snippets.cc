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
            << command_usage << "\n";
}

void AsyncCreateInstance(cbt::InstanceAdmin instance_admin,
                         cbt::CompletionQueue cq,
                         std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{"async-create-instance: <project-id> <instance-id> <zone>"};
  }
  //! [async create instance]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string zone) {
    google::cloud::bigtable::DisplayName display_name("Put description here");
    std::string cluster_id = instance_id + "-c1";
    auto cluster_config = google::cloud::bigtable::ClusterConfig(
        zone, 3, google::cloud::bigtable::ClusterConfig::HDD);
    google::cloud::bigtable::InstanceConfig config(
        google::cloud::bigtable::InstanceId(instance_id), display_name,
        {{cluster_id, cluster_config}});
    config.set_type(google::cloud::bigtable::InstanceConfig::PRODUCTION);

    auto future = instance_admin.AsyncCreateInstance(cq, config);
    // Most applications would simply call future.get(), here we show how to
    // perform additional work while the long running operation completes.
    std::cout << "Waiting for instance creation to complete ";
    for (int i = 0; i != 100; ++i) {
      if (std::future_status::ready ==
          future.wait_for(std::chrono::seconds(2))) {
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
  //! [async create instance]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncCreateCluster(cbt::InstanceAdmin instance_admin,
                        cbt::CompletionQueue cq,
                        std::vector<std::string> argv) {
  if (argv.size() != 4U) {
    throw Usage{
        "async-create-cluster <project-id> <instance-id> <cluster-id> <zone>"};
  }
  //! [async create cluster]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string cluster_id, std::string zone) {
    auto cluster_config = google::cloud::bigtable::ClusterConfig(
        zone, 3, google::cloud::bigtable::ClusterConfig::HDD);
    auto future = instance_admin.AsyncCreateCluster(
        cq, cluster_config, google::cloud::bigtable::InstanceId(instance_id),
        google::cloud::bigtable::ClusterId(cluster_id));
    // Most applications would simply call future.get(), here we show how to
    // perform additional work while the long running operation completes.
    std::cout << "Waiting for cluster creation to complete ";
    for (int i = 0; i != 100; ++i) {
      if (std::future_status::ready ==
          future.wait_for(std::chrono::seconds(2))) {
        auto cluster = future.get();
        if (!cluster) {
          throw std::runtime_error(cluster.status().message());
        }
        std::cout << "DONE: " << cluster->name() << "\n";
        return;
      }
      std::cout << '.' << std::flush;
    }
    std::cout << "TIMEOUT\n";
  }
  //! [async create cluster]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2], argv[3]);
}

void AsyncCreateAppProfile(cbt::InstanceAdmin instance_admin,
                           cbt::CompletionQueue cq,
                           std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-create-app-profile <project-id> <instance-id> <profile-id>"};
  }

  //! [async create app profile]
  namespace cbt = google::cloud::bigtable;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string profile_id) {
    auto config = cbt::AppProfileConfig::MultiClusterUseAny(
        cbt::AppProfileId(profile_id));
    auto future = instance_admin.AsyncCreateAppProfile(
        cq, cbt::InstanceId(instance_id), config);

    // Most applications would simply call future.get(), here we show how to
    // perform additional work while the long running operation completes.
    std::cout << "Waiting for app_profile creation to complete ";
    for (int i = 0; i != 100; ++i) {
      if (std::future_status::ready ==
          future.wait_for(std::chrono::seconds(2))) {
        auto app_profile = future.get();
        if (!app_profile) {
          throw std::runtime_error(app_profile.status().message());
        }
        std::string app_profile_detail;
        google::protobuf::TextFormat::PrintToString(*app_profile,
                                                    &app_profile_detail);
        std::cout << "DONE, app profile details: " << app_profile_detail
                  << "\n";
        return;
      }
      std::cout << '.' << std::flush;
    }
    std::cout << "TIMEOUT\n";
  }
  //! [async create app profile]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncGetInstance(cbt::InstanceAdmin instance_admin,
                      cbt::CompletionQueue cq, std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-get-instance: <project-id> <instance-id>"};
  }

  //! [async get instance]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    google::cloud::future<
        google::cloud::StatusOr<google::bigtable::admin::v2::Instance>>
        future = instance_admin.AsyncGetInstance(cq, instance_id);

    auto final = future.then(
        [](google::cloud::future<
            google::cloud::StatusOr<google::bigtable::admin::v2::Instance>>
               f) {
          auto instance = f.get();
          if (!instance) {
            throw std::runtime_error(instance.status().message());
          }
          std::string instance_detail;
          google::protobuf::TextFormat::PrintToString(*instance,
                                                      &instance_detail);
          std::cout << "GetInstance details : " << instance_detail << "\n";
          return google::cloud::Status();
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
    google::cloud::future<
        google::cloud::StatusOr<google::cloud::bigtable::v0::InstanceList>>
        future = instance_admin.AsyncListInstances(cq);

    auto final = future.then(
        [](google::cloud::future<
            google::cloud::StatusOr<google::cloud::bigtable::v0::InstanceList>>
               f) {
          auto instance_list = f.get();
          if (!instance_list) {
            throw std::runtime_error(instance_list.status().message());
          }
          for (const auto& instance : instance_list->instances) {
            std::cout << instance.name() << "\n";
          }
          if (!instance_list->failed_locations.empty()) {
            std::cout << "The Cloud Bigtable service reports that it could not "
                         "retrieve data for the following zones:\n";
            for (const auto& failed_location :
                 instance_list->failed_locations) {
              std::cout << failed_location << "\n";
            }
            std::cout << "This is typically a transient condition, try again "
                         "later.\n";
          }
          return google::cloud::Status();
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

    google::cloud::future<
        google::cloud::StatusOr<google::bigtable::admin::v2::Cluster>>
        future = instance_admin.AsyncGetCluster(cq, instance_id1, cluster_id1);

    auto final = future.then(
        [](google::cloud::future<
            google::cloud::StatusOr<google::bigtable::admin::v2::Cluster>>
               f) {
          auto cluster = f.get();
          if (!cluster) {
            throw std::runtime_error(cluster.status().message());
          }
          std::string cluster_detail;
          google::protobuf::TextFormat::PrintToString(*cluster,
                                                      &cluster_detail);
          std::cout << "GetCluster details : " << cluster_detail << "\n";
          return google::cloud::Status();
        });

    final.get();
  }
  //! [async get cluster]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncGetAppProfile(cbt::InstanceAdmin instance_admin,
                        cbt::CompletionQueue cq,
                        std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-get-app-profile: <project-id> <instance-id> <app_profile-id>"};
  }

  //! [async get app profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string app_profile_id) {
    auto final =
        instance_admin
            .AsyncGetAppProfile(cq, cbt::InstanceId(instance_id),
                                cbt::AppProfileId(app_profile_id))
            .then([](google::cloud::future<
                      StatusOr<google::bigtable::admin::v2::AppProfile>>
                         f) {
              auto app_profile = f.get();
              if (!app_profile) {
                throw std::runtime_error(app_profile.status().message());
              }
              std::string app_profile_detail;
              google::protobuf::TextFormat::PrintToString(*app_profile,
                                                          &app_profile_detail);
              std::cout << "GetAppProfile details : " << app_profile_detail
                        << "\n";
              return google::cloud::Status();
            });

    final.get();
  }
  //! [async get app profile]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncGetIamPolicy(cbt::InstanceAdmin instance_admin,
                       cbt::CompletionQueue cq, std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-get-iam-policy: <project-id> <instance-id>"};
  }

  //! [async get iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    google::cloud::future<StatusOr<google::cloud::IamPolicy>> future =
        instance_admin.AsyncGetIamPolicy(
            cq, google::cloud::bigtable::InstanceId(instance_id));

    auto final = future.then(
        [](google::cloud::future<StatusOr<google::cloud::IamPolicy>> f) {
          auto iam_policy = f.get();
          if (!iam_policy) {
            throw std::runtime_error(iam_policy.status().message());
          }
          std::cout << "IamPolicy details : " << *iam_policy << "\n";
          return google::cloud::Status();
        });

    final.get();
  }
  //! [async get iam policy]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncListClusters(cbt::InstanceAdmin instance_admin,
                       cbt::CompletionQueue cq, std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-list-clusters: <project-id> <instance-id>"};
  }

  //! [async list clusters]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    google::cloud::future<
        google::cloud::StatusOr<google::cloud::bigtable::v0::ClusterList>>
        future = instance_admin.AsyncListClusters(cq, instance_id);

    auto final = future.then(
        [](google::cloud::future<
            google::cloud::StatusOr<google::cloud::bigtable::v0::ClusterList>>
               f) {
          auto cluster_list = f.get();
          if (!cluster_list) {
            throw std::runtime_error(cluster_list.status().message());
          }
          std::cout << "Cluster Name List\n";
          for (const auto& cluster : cluster_list->clusters) {
            std::cout << cluster.name() << "\n";
          }
          if (!cluster_list->failed_locations.empty()) {
            std::cout << "The Cloud Bigtable service reports that it could not "
                         "retrieve data for the following zones:\n";
            for (const auto& failed_location : cluster_list->failed_locations) {
              std::cout << failed_location << "\n";
            }
            std::cout << "This is typically a transient condition, try again "
                         "later.\n";
          }
          return google::cloud::Status();
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
    google::cloud::future<
        google::cloud::StatusOr<google::cloud::bigtable::v0::ClusterList>>
        future = instance_admin.AsyncListClusters(cq);

    auto final = future.then(
        [](google::cloud::future<
            google::cloud::StatusOr<google::cloud::bigtable::v0::ClusterList>>
               f) {
          auto cluster_list = f.get();
          if (!cluster_list) {
            throw std::runtime_error(cluster_list.status().message());
          }
          std::cout << "Cluster Name List\n";
          for (const auto& cluster : cluster_list->clusters) {
            std::cout << cluster.name() << "\n";
          }
          if (!cluster_list->failed_locations.empty()) {
            std::cout << "The Cloud Bigtable service reports that it could not "
                         "retrieve data for the following zones:\n";
            for (const auto& failed_location : cluster_list->failed_locations) {
              std::cout << failed_location << "\n";
            }
            std::cout << "This is typically a transient condition, try again "
                         "later.\n";
          }
          return google::cloud::Status();
        });

    final.get();
  }
  //! [async list all clusters]
  (std::move(instance_admin), std::move(cq));
}

void AsyncListAppProfiles(cbt::InstanceAdmin instance_admin,
                          cbt::CompletionQueue cq,
                          std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-list-app-profiles: <project-id> <instance-id>"};
  }

  //! [async list app_profiles]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    google::cloud::future<google::cloud::StatusOr<
        std::vector<google::bigtable::admin::v2::AppProfile>>>
        future = instance_admin.AsyncListAppProfiles(cq, instance_id);

    auto final = future.then(
        [](google::cloud::future<google::cloud::StatusOr<
               std::vector<google::bigtable::admin::v2::AppProfile>>>
               f) {
          auto app_profile_list = f.get();
          if (!app_profile_list) {
            throw std::runtime_error(app_profile_list.status().message());
          }
          std::cout << "AppProfile Name List\n";
          for (const auto& app_profile : *app_profile_list) {
            std::cout << app_profile.name() << "\n";
          }
          return google::cloud::Status();
        });

    final.get();
  }
  //! [async list app_profiles]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncUpdateInstance(cbt::InstanceAdmin instance_admin,
                         cbt::CompletionQueue cq,
                         std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"update-instance: <project-id> <instance-id>"};
  }

  //! [async update instance]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    auto future =
        instance_admin.AsyncGetInstance(cq, instance_id)
            .then([instance_admin,
                   cq](google::cloud::future<google::cloud::StatusOr<
                           google::bigtable::admin::v2::Instance>>
                           instance_fut) mutable {
              auto instance = instance_fut.get();
              if (!instance) {
                throw std::runtime_error(instance.status().message());
              }
              // Modify the instance and prepare the mask with modified field
              google::cloud::bigtable::InstanceUpdateConfig
                  instance_update_config(std::move(*instance));
              instance_update_config.set_display_name("Modified Display Name");

              return instance_admin.AsyncUpdateInstance(cq,
                                                        instance_update_config);
            });
    // Most applications would simply call future.get(), here we show how to
    // perform additional work while the long running operation completes.
    std::cout << "Waiting for instance update to complete ";
    for (int i = 0; i != 100; ++i) {
      if (std::future_status::ready ==
          future.wait_for(std::chrono::seconds(2))) {
        auto instance = future.get();
        if (!instance) {
          throw std::runtime_error(instance.status().message());
        }
        std::string instance_detail;
        google::protobuf::TextFormat::PrintToString(*instance,
                                                    &instance_detail);
        std::cout << "DONE, instance details: " << instance_detail << "\n";
        return;
      }
      std::cout << '.' << std::flush;
    }
    std::cout << "TIMEOUT\n";
  }
  //! [async update instance]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncUpdateCluster(cbt::InstanceAdmin instance_admin,
                        cbt::CompletionQueue cq,
                        std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{"update-cluster: <project-id> <instance-id> <cluster-id>"};
  }

  //! [async update cluster]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string cluster_id) {
    auto future =
        instance_admin
            .AsyncGetCluster(cq,
                             google::cloud::bigtable::InstanceId(instance_id),
                             google::cloud::bigtable::ClusterId(cluster_id))
            .then([instance_admin,
                   cq](google::cloud::future<google::cloud::StatusOr<
                           google::bigtable::admin::v2::Cluster>>
                           cluster_fut) mutable {
              auto cluster = cluster_fut.get();
              if (!cluster) {
                throw std::runtime_error(cluster.status().message());
              }
              // Modify the cluster.
              cluster->set_serve_nodes(4);
              auto modified_config =
                  google::cloud::bigtable::ClusterConfig(std::move(*cluster));

              return instance_admin.AsyncUpdateCluster(cq, modified_config);
            });
    // Most applications would simply call future.get(), here we show how to
    // perform additional work while the long running operation completes.
    std::cout << "Waiting for cluster update to complete ";
    for (int i = 0; i != 100; ++i) {
      if (std::future_status::ready ==
          future.wait_for(std::chrono::seconds(2))) {
        auto cluster = future.get();
        if (!cluster) {
          throw std::runtime_error(cluster.status().message());
        }
        std::string cluster_detail;
        google::protobuf::TextFormat::PrintToString(*cluster, &cluster_detail);
        std::cout << "DONE, cluster details: " << cluster_detail << "\n";
        return;
      }
      std::cout << '.' << std::flush;
    }
    std::cout << "TIMEOUT\n";
  }
  //! [async update cluster]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncUpdateAppProfile(cbt::InstanceAdmin instance_admin,
                           cbt::CompletionQueue cq,
                           std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{"update-cluster: <project-id> <instance-id> <profile-id>"};
  }

  //! [async update app profile]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string profile_id) {
    auto future = instance_admin.AsyncUpdateAppProfile(
        cq, google::cloud::bigtable::InstanceId(instance_id),
        google::cloud::bigtable::AppProfileId(profile_id),
        google::cloud::bigtable::AppProfileUpdateConfig().set_description(
            "new description"));
    // Most applications would simply call future.get(), here we show how to
    // perform additional work while the long running operation completes.
    std::cout << "Waiting for app profile update to complete ";
    for (int i = 0; i != 100; ++i) {
      if (std::future_status::ready ==
          future.wait_for(std::chrono::seconds(2))) {
        auto app_profile = future.get();
        if (!app_profile) {
          throw std::runtime_error(app_profile.status().message());
        }
        std::string app_profile_detail;
        google::protobuf::TextFormat::PrintToString(*app_profile,
                                                    &app_profile_detail);
        std::cout << "DONE, app profile details: " << app_profile_detail
                  << "\n";
        return;
      }
      std::cout << '.' << std::flush;
    }
    std::cout << "TIMEOUT\n";
  }
  //! [async update app profile]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncDeleteInstance(cbt::InstanceAdmin instance_admin,
                         cbt::CompletionQueue cq,
                         std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-delete-instance: <project-id> <instance-id> "};
  }

  //! [async-delete-instance] [START bigtable_async_delete_instance]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    google::cloud::future<google::cloud::Status> fut =
        instance_admin.AsyncDeleteInstance(instance_id, cq);

    google::cloud::Status status = fut.get();

    std::cout << status.message() << '\n';

    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << " Done\n";
  }
  //! [async-delete-instance] [END bigtable_async_delete_instance]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncDeleteCluster(cbt::InstanceAdmin instance_admin,
                        cbt::CompletionQueue cq,
                        std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-delete-cluster: <project-id> <instance-id> <cluster-id> "};
  }

  //! [async delete cluster]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string cluster_id) {
    google::cloud::future<google::cloud::Status> future =
        instance_admin.AsyncDeleteCluster(
            cq, google::cloud::bigtable::InstanceId(instance_id),
            google::cloud::bigtable::ClusterId(cluster_id));

    // Most applications would simply call future.get(), here we show how to
    // perform additional work while the long running operation completes.
    std::cout << "Waiting for cluster deletion to complete ";
    for (int i = 0; i != 100; ++i) {
      if (std::future_status::ready ==
          future.wait_for(std::chrono::seconds(2))) {
        auto res = future.get();
        if (!res.ok()) {
          throw std::runtime_error(res.message());
        }
        std::cout << "DONE, cluster deleted.\n";
        return;
      }
      std::cout << '.' << std::flush;
    }
    std::cout << "TIMEOUT\n";
  }
  //! [async delete cluster]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncDeleteAppProfile(cbt::InstanceAdmin instance_admin,
                           cbt::CompletionQueue cq,
                           std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-delete-app-profile: <project-id> <instance-id> "
        "<app-profile-id> "};
  }

  //! [async delete app profile]
  namespace cbt = google::cloud::bigtable;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string app_profile_id) {
    google::cloud::future<google::cloud::Status> future =
        instance_admin.AsyncDeleteAppProfile(cq, cbt::InstanceId(instance_id),
                                             cbt::AppProfileId(app_profile_id));

    // Most applications would simply call future.get(), here we show how to
    // perform additional work while the long running operation completes.
    std::cout << "Waiting for app profile deletion to complete ";
    for (int i = 0; i != 100; ++i) {
      if (std::future_status::ready ==
          future.wait_for(std::chrono::seconds(2))) {
        auto res = future.get();
        if (!res.ok()) {
          throw std::runtime_error(res.message());
        }
        std::cout << "DONE, app profile deleted.\n";
        return;
      }
      std::cout << '.' << std::flush;
    }
    std::cout << "TIMEOUT\n";
  }
  //! [async delete app profile]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncTestIamPermissions(cbt::InstanceAdmin instance_admin,
                             cbt::CompletionQueue cq,
                             std::vector<std::string> argv) {
  if (argv.size() < 2U) {
    throw Usage{
        "async-test-iam-permissions: <project-id> <resource-id> "
        "[permission ...]"};
  }

  //! [async test iam permissions]
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string resource, std::vector<std::string>(permissions)) {
    auto future =
        instance_admin.AsyncTestIamPermissions(cq, resource, permissions);
    // Most applications would simply call future.get(), here we show how to
    // perform additional work while the long running operation completes.
    std::cout << "Waiting for app profile update to complete ";
    for (int i = 0; i != 100; ++i) {
      if (std::future_status::ready ==
          future.wait_for(std::chrono::seconds(2))) {
        auto result = future.get();
        if (!result) {
          throw std::runtime_error(result.status().message());
        }
        std::cout << "DONE, the current user has the following permissions [";
        char const* sep = "";
        for (auto const& p : *result) {
          std::cout << sep << p;
          sep = ", ";
        }
        std::cout << "]\n";
        return;
      }
      std::cout << '.' << std::flush;
    }
    std::cout << "TIMEOUT\n";
  }
  //! [async test iam permissions]
  (std::move(instance_admin), std::move(cq), argv[1],
   std::vector<std::string>(argv.begin() + 2, argv.end()));
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(
      google::cloud::bigtable::InstanceAdmin,
      google::cloud::bigtable::CompletionQueue, std::vector<std::string>)>;

  std::map<std::string, CommandType> commands = {
      {"async-create-instance", &AsyncCreateInstance},
      {"async-create-cluster", &AsyncCreateCluster},
      {"async-create-app-profile", &AsyncCreateAppProfile},
      {"async-get-instance", &AsyncGetInstance},
      {"async-get-cluster", &AsyncGetCluster},
      {"async-get-app-profile", &AsyncGetAppProfile},
      {"async-get-iam-policy", &AsyncGetIamPolicy},
      {"async-list-instances", &AsyncListInstances},
      {"async-list-clusters", &AsyncListClusters},
      {"async-list-all-clusters", &AsyncListAllClusters},
      {"async-list-app-profiles", &AsyncListAppProfiles},
      {"async-update-instance", &AsyncUpdateInstance},
      {"async-update-cluster", &AsyncUpdateCluster},
      {"async-update-app-profile", &AsyncUpdateAppProfile},
      {"async-delete-instance", &AsyncDeleteInstance},
      {"async-delete-cluster", &AsyncDeleteCluster},
      {"async-delete-app-profile", &AsyncDeleteAppProfile},
      {"async-test-iam-permissions", &AsyncTestIamPermissions}};

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
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
//! [all code]
