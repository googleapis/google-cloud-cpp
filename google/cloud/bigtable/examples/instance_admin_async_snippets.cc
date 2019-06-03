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

std::string command_usage;

void PrintUsage(std::string const& cmd, std::string const& msg) {
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void AsyncCreateInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                         google::cloud::bigtable::CompletionQueue cq,
                         std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{"async-create-instance: <project-id> <instance-id> <zone>"};
  }
  //! [async create instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string zone) {
    std::string display_name("Put description here");
    std::string cluster_id = instance_id + "-c1";
    cbt::ClusterConfig cluster_config =
        cbt::ClusterConfig(zone, 3, cbt::ClusterConfig::HDD);
    cbt::InstanceConfig config(instance_id, display_name,
                               {{cluster_id, cluster_config}});
    config.set_type(cbt::InstanceConfig::PRODUCTION);

    future<StatusOr<google::bigtable::admin::v2::Instance>> instance_future =
        instance_admin.AsyncCreateInstance(cq, config);
    // Show how to perform additional work while the long running operation
    // completes. The application could use future.then() instead.
    std::cout << "Waiting for instance creation to complete " << std::flush;
    instance_future.wait_for(std::chrono::seconds(2));
    std::cout << '.' << std::flush;
    auto instance = instance_future.get();
    if (!instance) {
      throw std::runtime_error(instance.status().message());
    }
    std::cout << "DONE, details=" << instance->name() << "\n";
  }
  //! [async create instance]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncCreateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                        google::cloud::bigtable::CompletionQueue cq,
                        std::vector<std::string> argv) {
  if (argv.size() != 4U) {
    throw Usage{
        "async-create-cluster <project-id> <instance-id> <cluster-id> <zone>"};
  }
  //! [async create cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string cluster_id, std::string zone) {
    cbt::ClusterConfig cluster_config =
        cbt::ClusterConfig(zone, 3, cbt::ClusterConfig::HDD);
    future<StatusOr<google::bigtable::admin::v2::Cluster>> cluster_future =
        instance_admin.AsyncCreateCluster(cq, cluster_config, instance_id,
                                          cluster_id);
    // Show how to perform additional work while the long running operation
    // completes. The application could use future.then() instead.
    std::cout << "Waiting for cluster creation to complete " << std::flush;
    cluster_future.wait_for(std::chrono::seconds(2));
    std::cout << '.' << std::flush;
    auto cluster = cluster_future.get();
    if (!cluster) {
      throw std::runtime_error(cluster.status().message());
    }
    std::cout << "DONE, details=" << cluster->DebugString() << "\n";
  }
  //! [async create cluster]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2], argv[3]);
}

void AsyncCreateAppProfile(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-create-app-profile <project-id> <instance-id> <profile-id>"};
  }

  //! [async create app profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string profile_id) {
    cbt::AppProfileConfig config =
        cbt::AppProfileConfig::MultiClusterUseAny(profile_id);
    future<StatusOr<google::bigtable::admin::v2::AppProfile>> profile_future =
        instance_admin.AsyncCreateAppProfile(cq, instance_id, config);

    // Show how to perform additional work while the long running operation
    // completes. The application could use profile_future.then() instead.
    std::cout << "Waiting for app_profile creation to complete " << std::flush;
    profile_future.wait_for(std::chrono::seconds(2));
    std::cout << '.' << std::flush;
    auto app_profile = profile_future.get();
    if (!app_profile) {
      throw std::runtime_error(app_profile.status().message());
    }
    std::cout << "DONE, details=" << app_profile->DebugString() << "\n";
  }
  //! [async create app profile]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncGetInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                      google::cloud::bigtable::CompletionQueue cq,
                      std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-get-instance: <project-id> <instance-id>"};
  }

  //! [async get instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    future<StatusOr<google::bigtable::admin::v2::Instance>> instance_future =
        instance_admin.AsyncGetInstance(cq, instance_id);

    future<google::cloud::Status> final = instance_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Instance>> f) {
          auto instance = f.get();
          if (!instance) {
            throw std::runtime_error(instance.status().message());
          }
          std::cout << "GetInstance details : " << instance->DebugString()
                    << "\n";
          return google::cloud::Status();
        });

    final.get();  // block to keep the example simple
  }
  //! [async get instance]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncListInstances(google::cloud::bigtable::InstanceAdmin instance_admin,
                        google::cloud::bigtable::CompletionQueue cq,
                        std::vector<std::string> argv) {
  if (argv.size() != 1U) {
    throw Usage{"async-list-instances: <project-id>"};
  }

  //! [async list instances]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq) {
    future<google::cloud::StatusOr<cbt::InstanceList>> instances_future =
        instance_admin.AsyncListInstances(cq);

    future<void> final =
        instances_future.then([](future<StatusOr<cbt::InstanceList>> f) {
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
        });
    final.get();  // block to keep the example simple
  }
  //! [async list instances]
  (std::move(instance_admin), std::move(cq));
}

void AsyncGetCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                     google::cloud::bigtable::CompletionQueue cq,
                     std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{"async-get-cluster: <project-id> <instance-id> <cluster-id>"};
  }

  //! [async get cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string cluster_id) {
    future<StatusOr<google::bigtable::admin::v2::Cluster>> cluster_future =
        instance_admin.AsyncGetCluster(cq, instance_id, cluster_id);

    future<google::cloud::Status> final = cluster_future.then(
        [](google::cloud::future<
            google::cloud::StatusOr<google::bigtable::admin::v2::Cluster>>
               f) {
          StatusOr<google::bigtable::admin::v2::Cluster> cluster = f.get();
          if (!cluster) {
            throw std::runtime_error(cluster.status().message());
          }
          std::string cluster_detail;
          google::protobuf::TextFormat::PrintToString(*cluster,
                                                      &cluster_detail);
          std::cout << "GetCluster details : " << cluster_detail << "\n";
          return google::cloud::Status();
        });

    final.get();  // block to keep the example simple
  }
  //! [async get cluster]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncGetAppProfile(google::cloud::bigtable::InstanceAdmin instance_admin,
                        google::cloud::bigtable::CompletionQueue cq,
                        std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-get-app-profile: <project-id> <instance-id> <app_profile-id>"};
  }

  //! [async get app profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string app_profile_id) {
    future<void> final =
        instance_admin.AsyncGetAppProfile(cq, instance_id, app_profile_id)
            .then([](google::cloud::future<
                      StatusOr<google::bigtable::admin::v2::AppProfile>>
                         f) {
              StatusOr<google::bigtable::admin::v2::AppProfile> app_profile =
                  f.get();
              if (!app_profile) {
                throw std::runtime_error(app_profile.status().message());
              }
              std::string app_profile_detail;
              google::protobuf::TextFormat::PrintToString(*app_profile,
                                                          &app_profile_detail);
              std::cout << "GetAppProfile details : " << app_profile_detail
                        << "\n";
            });

    final.get();  // block to keep the example simple
  }
  //! [async get app profile]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncGetIamPolicy(google::cloud::bigtable::InstanceAdmin instance_admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-get-iam-policy: <project-id> <instance-id>"};
  }

  //! [async get iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    future<StatusOr<google::cloud::IamPolicy>> policy_future =
        instance_admin.AsyncGetIamPolicy(cq, instance_id);

    future<void> final = policy_future.then(
        [](google::cloud::future<StatusOr<google::cloud::IamPolicy>> f) {
          StatusOr<google::cloud::IamPolicy> iam_policy = f.get();
          if (!iam_policy) {
            throw std::runtime_error(iam_policy.status().message());
          }
          std::cout << "IamPolicy details : " << *iam_policy << "\n";
        });

    final.get();  // block to keep the example simple
  }
  //! [async get iam policy]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncListClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-list-clusters: <project-id> <instance-id>"};
  }

  //! [async list clusters]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    future<StatusOr<cbt::ClusterList>> clusters_future =
        instance_admin.AsyncListClusters(cq, instance_id);

    future<void> final =
        clusters_future.then([](future<StatusOr<cbt::ClusterList>> f) {
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
        });

    final.get();  // block to keep the example simple
  }
  //! [async list clusters]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncListAllClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                          google::cloud::bigtable::CompletionQueue cq,
                          std::vector<std::string> argv) {
  if (argv.size() != 1U) {
    throw Usage{"async-list-all-clusters: <project-id>"};
  }

  //! [async list all clusters]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq) {
    future<StatusOr<cbt::ClusterList>> clusters_future =
        instance_admin.AsyncListClusters(cq);

    future<void> final =
        clusters_future.then([](future<StatusOr<cbt::ClusterList>> f) {
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
        });

    final.get();  // block to keep the example simple
  }
  //! [async list all clusters]
  (std::move(instance_admin), std::move(cq));
}

void AsyncListAppProfiles(google::cloud::bigtable::InstanceAdmin instance_admin,
                          google::cloud::bigtable::CompletionQueue cq,
                          std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-list-app-profiles: <project-id> <instance-id>"};
  }

  //! [async list app_profiles]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    future<StatusOr<std::vector<google::bigtable::admin::v2::AppProfile>>>
        profiles_future = instance_admin.AsyncListAppProfiles(cq, instance_id);

    future<void> final = profiles_future.then(
        [](future<
            StatusOr<std::vector<google::bigtable::admin::v2::AppProfile>>>
               f) {
          auto app_profile_list = f.get();
          if (!app_profile_list) {
            throw std::runtime_error(app_profile_list.status().message());
          }
          std::cout << "AppProfile Name List\n";
          for (const auto& app_profile : *app_profile_list) {
            std::cout << app_profile.name() << "\n";
          }
        });

    final.get();  // block to keep the example simple
  }
  //! [async list app_profiles]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncUpdateInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                         google::cloud::bigtable::CompletionQueue cq,
                         std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"update-instance: <project-id> <instance-id>"};
  }

  //! [async update instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    // Chain a AsyncGetInstance() + AsyncUpdateInstance().
    future<void> final =
        instance_admin.AsyncGetInstance(cq, instance_id)
            .then([instance_admin,
                   cq](future<StatusOr<google::bigtable::admin::v2::Instance>>
                           f) mutable {
              auto instance = f.get();
              if (!instance) {
                throw std::runtime_error(instance.status().message());
              }
              // Modify the instance and prepare the mask with modified
              // field
              cbt::InstanceUpdateConfig instance_update_config(
                  std::move(*instance));
              instance_update_config.set_display_name("Modified Display Name");

              return instance_admin.AsyncUpdateInstance(cq,
                                                        instance_update_config);
            })
            .then(
                [](future<StatusOr<google::bigtable::admin::v2::Instance>> f) {
                  auto instance = f.get();
                  if (!instance) {
                    throw std::runtime_error(instance.status().message());
                  }
                  std::cout
                      << "DONE, instance details: " << instance->DebugString()
                      << "\n";
                });
    final.get();  // block to keep example simple.
  }
  //! [async update instance]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncUpdateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                        google::cloud::bigtable::CompletionQueue cq,
                        std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{"async-update-cluster <project-id> <instance-id> <cluster-id>"};
  }

  //! [async update cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string cluster_id) {
    future<void> final =
        instance_admin.AsyncGetCluster(cq, instance_id, cluster_id)
            .then([instance_admin,
                   cq](future<StatusOr<google::bigtable::admin::v2::Cluster>>
                           f) mutable {
              auto cluster = f.get();
              if (!cluster) {
                return google::cloud::make_ready_future(
                    StatusOr<google::bigtable::admin::v2::Cluster>(
                        cluster.status()));
              }
              // The state cannot be sent on updates, so clear it first.
              cluster->clear_state();
              // Set the desired cluster configuration.
              cluster->set_serve_nodes(4);
              cbt::ClusterConfig modified_config =
                  cbt::ClusterConfig(std::move(*cluster));

              return instance_admin.AsyncUpdateCluster(cq, modified_config);
            })
            .then([](future<StatusOr<google::bigtable::admin::v2::Cluster>> f) {
              auto cluster = f.get();
              if (!cluster) {
                throw std::runtime_error(cluster.status().message());
              }
              std::cout << "DONE, details=" << cluster->DebugString() << "\n";
            });
    final.get();  // block to keep example simple.
  }
  //! [async update cluster]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncUpdateAppProfile(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{"update-cluster: <project-id> <instance-id> <profile-id>"};
  }

  //! [async update app profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string profile_id) {
    future<StatusOr<google::bigtable::admin::v2::AppProfile>> profile_future =
        instance_admin.AsyncUpdateAppProfile(
            cq, instance_id, profile_id,
            cbt::AppProfileUpdateConfig()
                .set_description("new description")
                .set_ignore_warnings(true));

    // Show how to perform additional work while the long running operation
    // completes. The application could use profile_future.then() instead.
    std::cout << "Waiting for app profile update to complete " << std::flush;
    profile_future.wait_for(std::chrono::seconds(2));
    std::cout << '.' << std::flush;
    auto app_profile = profile_future.get();
    if (!app_profile) {
      throw std::runtime_error(app_profile.status().message());
    }
    std::cout << "DONE, details=" << app_profile->DebugString() << "\n";
  }
  //! [async update app profile]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncDeleteInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                         google::cloud::bigtable::CompletionQueue cq,
                         std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-delete-instance: <project-id> <instance-id> "};
  }

  //! [async-delete-instance] [START bigtable_async_delete_instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id) {
    future<google::cloud::Status> status_future =
        instance_admin.AsyncDeleteInstance(instance_id, cq);

    google::cloud::Status status = status_future.get();
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "Instance " << instance_id << " successfully deleted\n";
  }
  //! [async-delete-instance] [END bigtable_async_delete_instance]
  (std::move(instance_admin), std::move(cq), argv[1]);
}

void AsyncDeleteCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                        google::cloud::bigtable::CompletionQueue cq,
                        std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-delete-cluster: <project-id> <instance-id> <cluster-id> "};
  }

  //! [async delete cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string cluster_id) {
    future<google::cloud::Status> status_future =
        instance_admin.AsyncDeleteCluster(cq, instance_id, cluster_id);

    google::cloud::Status status = status_future.get();
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "Cluster " << cluster_id << " successfully deleted\n";
  }
  //! [async delete cluster]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncDeleteAppProfile(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-delete-app-profile <project-id> <instance-id> "
        "<app-profile-id> "};
  }

  //! [async delete app profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string app_profile_id) {
    future<google::cloud::Status> status_future =
        instance_admin.AsyncDeleteAppProfile(cq, instance_id, app_profile_id,
                                             /*ignore_warnings=*/true);

    google::cloud::Status status = status_future.get();
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "Profile " << app_profile_id << " successfully deleted\n";
  }
  //! [async delete app profile]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2]);
}

void AsyncSetIamPolicy(google::cloud::bigtable::InstanceAdmin instance_admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> argv) {
  if (argv.size() < 2U) {
    throw Usage{
        "async-set-iam-policy: <project-id> <instance-id>"
        " <permission> <new-member>\n"
        "        Example: set-iam-policy my-project my-instance"
        " roles/bigtable.user user:my-user@example.com"};
  }

  //! [async set iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::IamPolicy;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string instance_id, std::string role, std::string member) {
    future<StatusOr<IamPolicy>> updated_future =
        instance_admin.AsyncGetIamPolicy(cq, instance_id)
            .then([cq, instance_admin, role, member, instance_id](
                      future<StatusOr<IamPolicy>> current_future) mutable {
              auto current = current_future.get();
              if (!current) {
                return google::cloud::make_ready_future<StatusOr<IamPolicy>>(
                    current.status());
              }
              google::cloud::IamBindings bindings = current->bindings;
              bindings.AddMember(role, member);
              return instance_admin.AsyncSetIamPolicy(cq, instance_id, bindings,
                                                      current->etag);
            });
    // Show how to perform additional work while the long running operation
    // completes. The application could use future.then() instead.
    std::cout << "Waiting for IAM policy update to complete " << std::flush;
    updated_future.wait_for(std::chrono::seconds(2));
    auto result = updated_future.get();
    std::cout << '.' << std::flush;
    if (!result) {
      throw std::runtime_error(result.status().message());
    }
    std::cout << "DONE, the IAM Policy for " << instance_id << " is\n";
    for (auto const& kv : result->bindings) {
      std::cout << "role " << kv.first << " includes [";
      char const* sep = "";
      for (auto const& m : kv.second) {
        std::cout << sep << m;
        sep = ", ";
      }
      std::cout << "]\n";
    }
  }
  //! [async set iam policy]
  (std::move(instance_admin), std::move(cq), argv[1], argv[2], argv[3]);
}

void AsyncTestIamPermissions(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> argv) {
  if (argv.size() < 2U) {
    throw Usage{
        "async-test-iam-permissions: <project-id> <resource-id> "
        "[permission ...]"};
  }

  //! [async test iam permissions]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string resource, std::vector<std::string>(permissions)) {
    future<StatusOr<std::vector<std::string>>> permissions_future =
        instance_admin.AsyncTestIamPermissions(cq, resource, permissions);
    // Show how to perform additional work while the long running operation
    // completes. The application could use permissions_future.then() instead.
    std::cout << "Waiting for app profile update to complete " << std::flush;
    permissions_future.wait_for(std::chrono::seconds(2));
    std::cout << '.' << std::flush;
    auto result = permissions_future.get();
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
      {"async-set-iam-policy", &AsyncSetIamPolicy},
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
