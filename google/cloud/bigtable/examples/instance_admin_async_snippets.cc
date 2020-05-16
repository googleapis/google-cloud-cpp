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

#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/instance_admin.h"
#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/crash_handler.h"
#include <sstream>

namespace {

using google::cloud::bigtable::examples::Usage;

void AsyncCreateInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                         google::cloud::bigtable::CompletionQueue cq,
                         std::vector<std::string> const& argv) {
  //! [async create instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id, std::string const& zone) {
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
    if (!instance) throw std::runtime_error(instance.status().message());
    std::cout << "DONE, details=" << instance->name() << "\n";
  }
  //! [async create instance]
  (std::move(instance_admin), std::move(cq), argv.at(0), argv.at(1));
}

void AsyncCreateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                        google::cloud::bigtable::CompletionQueue cq,
                        std::vector<std::string> const& argv) {
  //! [async create cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id, std::string const& cluster_id,
     std::string const& zone) {
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
    if (!cluster) throw std::runtime_error(cluster.status().message());
    std::cout << "DONE, details=" << cluster->DebugString() << "\n";
  }
  //! [async create cluster]
  (std::move(instance_admin), std::move(cq), argv.at(0), argv.at(1),
   argv.at(2));
}

void AsyncCreateAppProfile(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> const& argv) {
  //! [async create app profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id, std::string const& profile_id) {
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
    if (!app_profile) throw std::runtime_error(app_profile.status().message());
    std::cout << "DONE, details=" << app_profile->DebugString() << "\n";
  }
  //! [async create app profile]
  (std::move(instance_admin), std::move(cq), argv.at(0), argv.at(1));
}

void AsyncGetInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                      google::cloud::bigtable::CompletionQueue cq,
                      std::vector<std::string> const& argv) {
  //! [async get instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id) {
    future<StatusOr<google::bigtable::admin::v2::Instance>> instance_future =
        instance_admin.AsyncGetInstance(cq, instance_id);

    future<google::cloud::Status> final = instance_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Instance>> f) {
          auto instance = f.get();
          if (!instance) throw std::runtime_error(instance.status().message());
          std::cout << "GetInstance details : " << instance->DebugString()
                    << "\n";
          return google::cloud::Status();
        });

    final.get();  // block to keep the example simple
  }
  //! [async get instance]
  (std::move(instance_admin), std::move(cq), argv.at(0));
}

void AsyncListInstances(google::cloud::bigtable::InstanceAdmin instance_admin,
                        google::cloud::bigtable::CompletionQueue cq,
                        std::vector<std::string> const&) {
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
                     std::vector<std::string> const& argv) {
  //! [async get cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id, std::string const& cluster_id) {
    future<StatusOr<google::bigtable::admin::v2::Cluster>> cluster_future =
        instance_admin.AsyncGetCluster(cq, instance_id, cluster_id);

    future<google::cloud::Status> final = cluster_future.then(
        [](future<StatusOr<google::bigtable::admin::v2::Cluster>> f) {
          StatusOr<google::bigtable::admin::v2::Cluster> cluster = f.get();
          if (!cluster) throw std::runtime_error(cluster.status().message());
          std::cout << "GetCluster details : " << cluster->DebugString()
                    << "\n";
          return google::cloud::Status();
        });

    final.get();  // block to keep the example simple
  }
  //! [async get cluster]
  (std::move(instance_admin), std::move(cq), argv.at(0), argv.at(1));
}

void AsyncGetAppProfile(google::cloud::bigtable::InstanceAdmin instance_admin,
                        google::cloud::bigtable::CompletionQueue cq,
                        std::vector<std::string> const& argv) {
  //! [async get app profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id, std::string const& app_profile_id) {
    future<void> final =
        instance_admin.AsyncGetAppProfile(cq, instance_id, app_profile_id)
            .then([](future<StatusOr<google::bigtable::admin::v2::AppProfile>>
                         f) {
              auto app_profile = f.get();
              if (!app_profile) {
                throw std::runtime_error(app_profile.status().message());
              }
              std::cout << "GetAppProfile details : "
                        << app_profile->DebugString() << "\n";
            });

    final.get();  // block to keep the example simple
  }
  //! [async get app profile]
  (std::move(instance_admin), std::move(cq), argv.at(0), argv.at(1));
}

void AsyncGetIamPolicy(google::cloud::bigtable::InstanceAdmin instance_admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> const& argv) {
  //! [async get iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id) {
    future<StatusOr<google::cloud::IamPolicy>> policy_future =
        instance_admin.AsyncGetIamPolicy(cq, instance_id);

    future<void> final =
        policy_future.then([](future<StatusOr<google::cloud::IamPolicy>> f) {
          StatusOr<google::cloud::IamPolicy> iam_policy = f.get();
          if (!iam_policy) {
            throw std::runtime_error(iam_policy.status().message());
          }
          std::cout << "IamPolicy details : " << *iam_policy << "\n";
        });

    final.get();  // block to keep the example simple
  }
  //! [async get iam policy]
  (std::move(instance_admin), std::move(cq), argv.at(0));
}

void AsyncGetNativeIamPolicy(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> const& argv) {
  //! [async get native iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  using google::iam::v1::Policy;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id) {
    future<StatusOr<google::iam::v1::Policy>> policy_future =
        instance_admin.AsyncGetNativeIamPolicy(cq, instance_id);

    future<void> final =
        policy_future.then([](future<StatusOr<google::iam::v1::Policy>> f) {
          StatusOr<google::iam::v1::Policy> iam = f.get();
          if (!iam) throw std::runtime_error(iam.status().message());
          std::cout << "IamPolicy details : " << iam->DebugString() << "\n";
        });

    final.get();  // block to keep the example simple
  }
  //! [async get native iam policy]
  (std::move(instance_admin), std::move(cq), argv.at(0));
}

void AsyncListClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> const& argv) {
  //! [async list clusters]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id) {
    future<StatusOr<cbt::ClusterList>> clusters_future =
        instance_admin.AsyncListClusters(cq, instance_id);

    future<void> final =
        clusters_future.then([](future<StatusOr<cbt::ClusterList>> f) {
          auto clusters = f.get();
          if (!clusters) throw std::runtime_error(clusters.status().message());
          std::cout << "Cluster Name List\n";
          for (const auto& cluster : clusters->clusters) {
            std::cout << cluster.name() << "\n";
          }
          if (!clusters->failed_locations.empty()) {
            std::cout << "The Cloud Bigtable service reports that it could not "
                         "retrieve data for the following zones:\n";
            for (const auto& failed_location : clusters->failed_locations) {
              std::cout << failed_location << "\n";
            }
            std::cout << "This is typically a transient condition, try again "
                         "later.\n";
          }
        });

    final.get();  // block to keep the example simple
  }
  //! [async list clusters]
  (std::move(instance_admin), std::move(cq), argv.at(0));
}

void AsyncListAllClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                          google::cloud::bigtable::CompletionQueue cq,
                          std::vector<std::string> const&) {
  //! [async list all clusters]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq) {
    future<StatusOr<cbt::ClusterList>> clusters_future =
        instance_admin.AsyncListClusters(cq);

    future<void> final =
        clusters_future.then([](future<StatusOr<cbt::ClusterList>> f) {
          auto clusters = f.get();
          if (!clusters) throw std::runtime_error(clusters.status().message());
          std::cout << "Cluster Name List\n";
          for (const auto& cluster : clusters->clusters) {
            std::cout << cluster.name() << "\n";
          }
          if (!clusters->failed_locations.empty()) {
            std::cout << "The Cloud Bigtable service reports that it could not "
                         "retrieve data for the following zones:\n";
            for (const auto& failed_location : clusters->failed_locations) {
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
                          std::vector<std::string> const& argv) {
  //! [async list app_profiles]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id) {
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
  (std::move(instance_admin), std::move(cq), argv.at(0));
}

void AsyncUpdateInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                         google::cloud::bigtable::CompletionQueue cq,
                         std::vector<std::string> const& argv) {
  //! [async update instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id) {
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
  (std::move(instance_admin), std::move(cq), argv.at(0));
}

void AsyncUpdateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                        google::cloud::bigtable::CompletionQueue cq,
                        std::vector<std::string> const& argv) {
  //! [async update cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id, std::string const& cluster_id) {
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
  (std::move(instance_admin), std::move(cq), argv.at(0), argv.at(1));
}

void AsyncUpdateAppProfile(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> const& argv) {
  //! [async update app profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id, std::string const& profile_id) {
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
    if (!app_profile) throw std::runtime_error(app_profile.status().message());
    std::cout << "DONE, details=" << app_profile->DebugString() << "\n";
  }
  //! [async update app profile]
  (std::move(instance_admin), std::move(cq), argv.at(0), argv.at(1));
}

void AsyncDeleteInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                         google::cloud::bigtable::CompletionQueue cq,
                         std::vector<std::string> const& argv) {
  //! [async-delete-instance] [START bigtable_async_delete_instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id) {
    future<google::cloud::Status> status_future =
        instance_admin.AsyncDeleteInstance(instance_id, cq);

    google::cloud::Status status = status_future.get();
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Instance " << instance_id << " successfully deleted\n";
  }
  //! [async-delete-instance] [END bigtable_async_delete_instance]
  (std::move(instance_admin), std::move(cq), argv.at(0));
}

void AsyncDeleteCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                        google::cloud::bigtable::CompletionQueue cq,
                        std::vector<std::string> const& argv) {
  //! [async delete cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id, std::string const& cluster_id) {
    future<google::cloud::Status> status_future =
        instance_admin.AsyncDeleteCluster(cq, instance_id, cluster_id);

    google::cloud::Status status = status_future.get();
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Cluster " << cluster_id << " successfully deleted\n";
  }
  //! [async delete cluster]
  (std::move(instance_admin), std::move(cq), argv.at(0), argv.at(1));
}

void AsyncDeleteAppProfile(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> const& argv) {
  //! [async delete app profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id, std::string const& app_profile_id) {
    future<google::cloud::Status> status_future =
        instance_admin.AsyncDeleteAppProfile(cq, instance_id, app_profile_id,
                                             /*ignore_warnings=*/true);

    google::cloud::Status status = status_future.get();
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Profile " << app_profile_id << " successfully deleted\n";
  }
  //! [async delete app profile]
  (std::move(instance_admin), std::move(cq), argv.at(0), argv.at(1));
}

void AsyncSetIamPolicy(google::cloud::bigtable::InstanceAdmin instance_admin,
                       google::cloud::bigtable::CompletionQueue cq,
                       std::vector<std::string> const& argv) {
  //! [async set iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::IamPolicy;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id, std::string const& role,
     std::string const& member) {
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
    if (!result) throw std::runtime_error(result.status().message());
    std::cout << "DONE, the IAM Policy for " << instance_id << " is\n"
              << *result << "\n";
  }
  //! [async set iam policy]
  (std::move(instance_admin), std::move(cq), argv.at(0), argv.at(1),
   argv.at(2));
}

void AsyncSetNativeIamPolicy(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> const& argv) {
  //! [async set native iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  using google::iam::v1::Policy;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& instance_id, std::string const& role,
     std::string const& member) {
    future<StatusOr<google::iam::v1::Policy>> updated_future =
        instance_admin.AsyncGetNativeIamPolicy(cq, instance_id)
            .then([cq, instance_admin, role, member,
                   instance_id](future<StatusOr<google::iam::v1::Policy>>
                                    current_future) mutable {
              auto current = current_future.get();
              if (!current) {
                return google::cloud::make_ready_future<
                    StatusOr<google::iam::v1::Policy>>(current.status());
              }
              // This example adds the member to all existing bindings for
              // that role. If there are no such bindgs, it adds a new one.
              // This might not be what the user wants, e.g. in case of
              // conditional bindings.
              size_t num_added = 0;
              for (auto& binding : *current->mutable_bindings()) {
                if (binding.role() == role) {
                  binding.add_members(member);
                  ++num_added;
                }
              }
              if (num_added == 0) {
                *current->add_bindings() = cbt::IamBinding(role, {member});
              }
              return instance_admin.AsyncSetIamPolicy(cq, instance_id,
                                                      *current);
            });
    // Show how to perform additional work while the long running operation
    // completes. The application could use future.then() instead.
    std::cout << "Waiting for IAM policy update to complete " << std::flush;
    updated_future.wait_for(std::chrono::seconds(2));
    auto result = updated_future.get();
    std::cout << '.' << std::flush;
    if (!result) throw std::runtime_error(result.status().message());
    using cbt::operator<<;
    std::cout << "DONE, the IAM Policy for " << instance_id << " is\n"
              << *result << "\n";
  }
  //! [async set native iam policy]
  (std::move(instance_admin), std::move(cq), argv.at(0), argv.at(1),
   argv.at(2));
}

void AsyncTestIamPermissions(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    google::cloud::bigtable::CompletionQueue cq,
    std::vector<std::string> const& argv) {
  //! [async test iam permissions]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, cbt::CompletionQueue cq,
     std::string const& resource, std::vector<std::string> const& permissions) {
    future<StatusOr<std::vector<std::string>>> permissions_future =
        instance_admin.AsyncTestIamPermissions(cq, resource, permissions);
    // Show how to perform additional work while the long running operation
    // completes. The application could use permissions_future.then() instead.
    std::cout << "Waiting for TestIamPermissions " << std::flush;
    permissions_future.wait_for(std::chrono::seconds(2));
    std::cout << '.' << std::flush;
    auto result = permissions_future.get();
    if (!result) throw std::runtime_error(result.status().message());
    std::cout << "DONE, the current user has the following permissions [";
    char const* sep = "";
    for (auto const& p : *result) {
      std::cout << sep << p;
      sep = ", ";
    }
    std::cout << "]\n";
  }
  //! [async test iam permissions]
  (std::move(instance_admin), std::move(cq), argv.at(0),
   {argv.begin() + 1, argv.end()});
}

void AsyncTestIamPermissionsCommand(std::vector<std::string> const& argv) {
  if (argv.size() < 2) {
    throw Usage{
        "async-test-iam-permissions <project-id> <resource-id>"
        "<permission> [permission ...]"};
  }
  auto it = argv.cbegin();
  auto const project_id = *it++;
  std::vector<std::string> extra_args(it, argv.cend());
  google::cloud::CompletionQueue cq;
  std::thread th([&cq] { cq.Run(); });
  google::cloud::bigtable::examples::AutoShutdownCQ shutdown(cq, std::move(th));

  google::cloud::bigtable::InstanceAdmin admin(
      google::cloud::bigtable::CreateDefaultInstanceAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()));

  AsyncTestIamPermissions(admin, cq, std::move(extra_args));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = google::cloud::bigtable;

  if (!argv.empty()) throw Usage{"auto"};
  if (!examples::RunAdminIntegrationTests()) return;
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const service_account =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_SERVICE_ACCOUNT")
          .value();
  auto const zone_a =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A")
          .value();
  auto const zone_b =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B")
          .value();

  cbt::InstanceAdmin admin(
      cbt::CreateDefaultInstanceAdminClient(project_id, cbt::ClientOptions{}));

  examples::CleanupOldInstances("exin-", admin);

  google::cloud::CompletionQueue cq;
  std::thread th([&cq] { cq.Run(); });
  examples::AutoShutdownCQ shutdown(cq, std::move(th));

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const instance_id = examples::RandomInstanceId("exin-", generator);

  std::cout << "\nRunning AsyncCreateInstance() example" << std::endl;
  AsyncCreateInstance(admin, cq, {instance_id, zone_a});

  std::cout << "\nRunning AsyncUpdateInstance() example" << std::endl;
  AsyncUpdateInstance(admin, cq, {instance_id});

  std::cout << "\nRunning AsyncGetInstance() example" << std::endl;
  AsyncGetInstance(admin, cq, {instance_id});

  std::cout << "\nRunning AsyncListInstances() example" << std::endl;
  AsyncListInstances(admin, cq, {});

  std::cout << "\nRunning AsyncListClusters() example" << std::endl;
  AsyncListClusters(admin, cq, {instance_id});

  std::cout << "\nRunning AsyncListAllClusters() example" << std::endl;
  AsyncListAllClusters(admin, cq, {});

  std::cout << "\nRunning AsyncListAppProfiles() example" << std::endl;
  AsyncListAppProfiles(admin, cq, {instance_id});

  std::cout << "\nRunning AsyncCreateCluster() example" << std::endl;
  AsyncCreateCluster(admin, cq, {instance_id, instance_id + "-c2", zone_b});

  std::cout << "\nRunning AsyncUpdateCluster() example" << std::endl;
  AsyncUpdateCluster(admin, cq, {instance_id, instance_id + "-c2"});

  std::cout << "\nRunning AsyncGetCluster() example" << std::endl;
  AsyncGetCluster(admin, cq, {instance_id, instance_id + "-c2"});

  std::cout << "\nRunning AsyncDeleteCluster() example" << std::endl;
  AsyncDeleteCluster(admin, cq, {instance_id, instance_id + "-c2"});

  std::cout << "\nRunning AsyncCreateAppProfile() example" << std::endl;
  AsyncCreateAppProfile(admin, cq, {instance_id, "my-app-profile"});

  std::cout << "\nRunning AsyncGetAppProfile() example" << std::endl;
  AsyncGetAppProfile(admin, cq, {instance_id, "my-app-profile"});

  std::cout << "\nRunning AsyncUpdateAppProfile() example" << std::endl;
  AsyncUpdateAppProfile(admin, cq, {instance_id, "my-app-profile"});

  std::cout << "\nRunning AsyncDeleteAppProfile() example" << std::endl;
  AsyncDeleteAppProfile(admin, cq, {instance_id, "my-app-profile"});

  std::cout << "\nRunning AsyncGetIamPolicy() example" << std::endl;
  AsyncGetIamPolicy(admin, cq, {instance_id});

  std::cout << "\nRunning AsyncSetIamPolicy() example" << std::endl;
  AsyncSetIamPolicy(admin, cq,
                    {instance_id, "roles/bigtable.user",
                     "serviceAccount:" + service_account});

  std::cout << "\nRunning AsyncGetNativeIamPolicy() example" << std::endl;
  AsyncGetNativeIamPolicy(admin, cq, {instance_id});

  std::cout << "\nRunning AsyncSetNativeIamPolicy() example" << std::endl;
  AsyncSetNativeIamPolicy(admin, cq,
                          {instance_id, "roles/bigtable.user",
                           "serviceAccount:" + service_account});

  std::cout << "\nRunning AsyncTestIamPermissions() example [1]" << std::endl;
  AsyncTestIamPermissionsCommand(
      {project_id, instance_id, "bigtable.instances.delete"});

  std::cout << "\nRunning AsyncTestIamPermissions() example [2]" << std::endl;
  AsyncTestIamPermissions(admin, cq,
                          {instance_id, "bigtable.instances.delete"});

  std::cout << "\nRunning AsyncDeleteInstance() example" << std::endl;
  AsyncDeleteInstance(admin, cq, {instance_id});
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InstallCrashHandler(argv[0]);

  namespace examples = google::cloud::bigtable::examples;
  examples::Example example({
      examples::MakeCommandEntry("async-create-instance",
                                 {"<instance-id>", "<zone>"},
                                 AsyncCreateInstance),
      examples::MakeCommandEntry("async-create-cluster",
                                 {"<instance-id>", "<cluster-id>", "<zone>"},
                                 AsyncCreateCluster),
      examples::MakeCommandEntry("async-create-app-profile",
                                 {"<instance-id>", "<profile-id>"},
                                 AsyncCreateAppProfile),
      examples::MakeCommandEntry("async-get-instance", {"<instance-id>"},
                                 AsyncGetInstance),
      examples::MakeCommandEntry("async-list-instances", {},
                                 AsyncListInstances),
      examples::MakeCommandEntry("async-get-cluster",
                                 {"<instance-id>", "<cluster-id>"},
                                 AsyncGetCluster),
      examples::MakeCommandEntry("async-get-app-profile",
                                 {"<instance-id>", "<app-profile-id>"},
                                 AsyncGetAppProfile),
      examples::MakeCommandEntry("async-get-iam-policy", {"<instance-id>"},
                                 AsyncGetIamPolicy),
      examples::MakeCommandEntry("async-get-native-iam-policy",
                                 {"<instance-id>"}, AsyncGetNativeIamPolicy),
      examples::MakeCommandEntry("async-list-clusters", {}, AsyncListClusters),
      examples::MakeCommandEntry("async-list-all-clusters", {},
                                 AsyncListAllClusters),
      examples::MakeCommandEntry("async-list-app-profiles", {"<instance-id>"},
                                 AsyncListAppProfiles),
      examples::MakeCommandEntry("async-update-instance", {"<instance-id>"},
                                 AsyncUpdateInstance),
      examples::MakeCommandEntry("async-update-cluster",
                                 {"<instance-id>", "<cluster-id>"},
                                 AsyncUpdateCluster),
      examples::MakeCommandEntry("async-update-app-profile",
                                 {"<instance-id>", "<profile-id>"},
                                 AsyncUpdateAppProfile),
      examples::MakeCommandEntry("async-delete-instance", {"<instance-id>"},
                                 AsyncDeleteInstance),
      examples::MakeCommandEntry("async-delete-cluster",
                                 {"<instance-id>", "<cluster-id>"},
                                 AsyncDeleteCluster),
      examples::MakeCommandEntry("async-delete-app-profile",
                                 {"<instance-id>", "<app-profile-id>"},
                                 AsyncDeleteAppProfile),
      examples::MakeCommandEntry("async-set-iam-policy",
                                 {"<instance-id>", "<role>", "<member>"},
                                 AsyncSetIamPolicy),
      examples::MakeCommandEntry("async-set-native-iam-policy",
                                 {"<instance-id>", "<role>", "<member>"},
                                 AsyncSetNativeIamPolicy),
      {"async-test-iam-permissions", AsyncTestIamPermissionsCommand},
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
