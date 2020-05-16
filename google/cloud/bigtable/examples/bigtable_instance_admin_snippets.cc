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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/crash_handler.h"

namespace {

using google::cloud::bigtable::examples::Usage;

void CreateInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                    std::vector<std::string> const& argv) {
  //! [create instance] [START bigtable_create_prod_instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& zone) {
    std::string display_name("Put description here");
    std::string cluster_id = instance_id + "-c1";
    auto cluster_config = cbt::ClusterConfig(zone, 3, cbt::ClusterConfig::HDD);
    cbt::InstanceConfig config(instance_id, display_name,
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
    if (!instance) throw std::runtime_error(instance.status().message());
    std::cout << "DONE, details=" << instance->DebugString() << "\n";
  }
  //! [create instance] [END bigtable_create_prod_instance]
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void CreateDevInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                       std::vector<std::string> const& argv) {
  //! [create dev instance] [START bigtable_create_dev_instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& zone) {
    std::string display_name("Put description here");
    std::string cluster_id = instance_id + "-c1";
    auto cluster_config = cbt::ClusterConfig(zone, 0, cbt::ClusterConfig::HDD);
    cbt::InstanceConfig config(instance_id, display_name,
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
    if (!instance) throw std::runtime_error(instance.status().message());
    std::cout << "DONE, details=" << instance->DebugString() << "\n";
  }
  //! [create dev instance] [END bigtable_create_dev_instance]
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void CreateReplicatedInstance(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_create_replicated_cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& zone_a, std::string const& zone_b) {
    std::string display_name("Put description here");
    auto c1 = instance_id + "-c1";
    auto c2 = instance_id + "-c2";
    cbt::InstanceConfig config(
        instance_id, display_name,
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
    if (!instance) throw std::runtime_error(instance.status().message());
    std::cout << "DONE, details=" << instance->DebugString() << "\n";
  }
  // [END bigtable_create_replicated_cluster]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void UpdateInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                    std::vector<std::string> const& argv) {
  //! [update instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id) {
    auto instance = instance_admin.GetInstance(instance_id);
    if (!instance) throw std::runtime_error(instance.status().message());
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
  (std::move(instance_admin), argv.at(0));
}

void ListInstances(google::cloud::bigtable::InstanceAdmin instance_admin,
                   std::vector<std::string> const&) {
  //! [list instances] [START bigtable_list_instances]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin) {
    StatusOr<cbt::InstanceList> instances = instance_admin.ListInstances();
    if (!instances) throw std::runtime_error(instances.status().message());
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
                         std::vector<std::string> const& argv) {
  // [START bigtable_check_instance_exists]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id) {
    StatusOr<google::bigtable::admin::v2::Instance> instance =
        instance_admin.GetInstance(instance_id);
    if (!instance) {
      if (instance.status().code() == google::cloud::StatusCode::kNotFound) {
        std::cout << "Instance " + instance_id + " does not exist\n";
        return;
      }
      throw std::runtime_error(instance.status().message());
    }
    std::cout << "Instance " << instance->name() << " was found\n";
  }
  // [END bigtable_check_instance_exists]
  (std::move(instance_admin), argv.at(0));
}

void GetInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                 std::vector<std::string> const& argv) {
  //! [get instance] [START bigtable_get_instance]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id) {
    StatusOr<google::bigtable::admin::v2::Instance> instance =
        instance_admin.GetInstance(instance_id);
    if (!instance) throw std::runtime_error(instance.status().message());
    std::cout << "GetInstance details : " << instance->DebugString() << "\n";
  }
  //! [get instance] [END bigtable_get_instance]
  (std::move(instance_admin), argv.at(0));
}

void DeleteInstance(google::cloud::bigtable::InstanceAdmin instance_admin,
                    std::vector<std::string> const& argv) {
  //! [delete instance] [START bigtable_delete_instance]
  namespace cbt = google::cloud::bigtable;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id) {
    google::cloud::Status status = instance_admin.DeleteInstance(instance_id);
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Successfully deleted the instance " << instance_id << "\n";
  }
  //! [delete instance] [END bigtable_delete_instance]
  (std::move(instance_admin), argv.at(0));
}

void CreateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   std::vector<std::string> const& argv) {
  //! [create cluster] [START bigtable_create_cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& cluster_id, std::string const& zone) {
    auto cluster_config = cbt::ClusterConfig(zone, 3, cbt::ClusterConfig::HDD);
    future<StatusOr<google::bigtable::admin::v2::Cluster>> cluster_future =
        instance_admin.CreateCluster(cluster_config, instance_id, cluster_id);

    // Applications can wait asynchronously, in this example we just block.
    auto cluster = cluster_future.get();
    if (!cluster) throw std::runtime_error(cluster.status().message());
    std::cout << "Successfully created cluster " << cluster->name() << "\n";
  }
  //! [create cluster] [END bigtable_create_cluster]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void ListClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                  std::vector<std::string> const& argv) {
  //! [list clusters] [START bigtable_get_clusters]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id) {
    StatusOr<cbt::ClusterList> clusters =
        instance_admin.ListClusters(instance_id);
    if (!clusters) throw std::runtime_error(clusters.status().message());
    std::cout << "Cluster Name List\n";
    for (auto const& cluster : clusters->clusters) {
      std::cout << "Cluster Name:" << cluster.name() << "\n";
    }
    if (!clusters->failed_locations.empty()) {
      std::cout << "The Cloud Bigtable service reports that the following "
                   "locations are temporarily unavailable and no information "
                   "about clusters in these locations can be obtained:\n";
      for (auto const& failed_location : clusters->failed_locations) {
        std::cout << failed_location << "\n";
      }
    }
  }
  //! [list clusters] [END bigtable_get_clusters]
  (std::move(instance_admin), argv.at(0));
}

void ListAllClusters(google::cloud::bigtable::InstanceAdmin instance_admin,
                     std::vector<std::string> const&) {
  //! [list all clusters] [START bigtable_get_clusters]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin) {
    StatusOr<cbt::ClusterList> clusters = instance_admin.ListClusters();
    if (!clusters) throw std::runtime_error(clusters.status().message());
    std::cout << "Cluster Name List\n";
    for (auto const& cluster : clusters->clusters) {
      std::cout << "Cluster Name:" << cluster.name() << "\n";
    }
    if (!clusters->failed_locations.empty()) {
      std::cout << "The Cloud Bigtable service reports that the following "
                   "locations are temporarily unavailable and no information "
                   "about clusters in these locations can be obtained:\n";
      for (auto const& failed_location : clusters->failed_locations) {
        std::cout << failed_location << "\n";
      }
    }
  }
  //! [list all clusters] [END bigtable_get_clusters]
  (std::move(instance_admin));
}

void UpdateCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   std::vector<std::string> const& argv) {
  //! [update cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& cluster_id) {
    // GetCluster first and then modify it.
    StatusOr<google::bigtable::admin::v2::Cluster> cluster =
        instance_admin.GetCluster(instance_id, cluster_id);
    if (!cluster) throw std::runtime_error(cluster.status().message());

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
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void GetCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                std::vector<std::string> const& argv) {
  //! [get cluster] [START bigtable_get_cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& cluster_id) {
    StatusOr<google::bigtable::admin::v2::Cluster> cluster =
        instance_admin.GetCluster(instance_id, cluster_id);
    if (!cluster) throw std::runtime_error(cluster.status().message());
    std::cout << "GetCluster details : " << cluster->DebugString() << "\n";
  }
  //! [get cluster] [END bigtable_get_cluster]
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void DeleteCluster(google::cloud::bigtable::InstanceAdmin instance_admin,
                   std::vector<std::string> const& argv) {
  //! [delete cluster] [START bigtable_delete_cluster]
  namespace cbt = google::cloud::bigtable;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& cluster_id) {
    google::cloud::Status status =
        instance_admin.DeleteCluster(instance_id, cluster_id);
    if (!status.ok()) throw std::runtime_error(status.message());
  }
  //! [delete cluster] [END bigtable_delete_cluster]
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void RunInstanceOperations(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    std::vector<std::string> const& argv) {
  //! [run instance operations]
  namespace cbt = google::cloud::bigtable;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& cluster_id, std::string const& zone) {
    std::string display_name("Put description here");
    auto cluster_config = cbt::ClusterConfig(zone, 3, cbt::ClusterConfig::HDD);
    cbt::InstanceConfig config(instance_id, display_name,
                               {{cluster_id, cluster_config}});
    config.set_type(cbt::InstanceConfig::PRODUCTION);

    std::cout << "\nCreating a PRODUCTION Instance: ";
    auto future = instance_admin.CreateInstance(config).get();
    std::cout << " Done\n";

    std::cout << "\nListing Instances:\n";
    auto instances = instance_admin.ListInstances();
    if (!instances) throw std::runtime_error(instances.status().message());
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
    if (!instance) throw std::runtime_error(instance.status().message());
    std::cout << "GetInstance details :\n" << instance->DebugString();

    std::cout << "\nListing Clusters:\n";
    auto clusters = instance_admin.ListClusters(instance_id);
    if (!clusters) throw std::runtime_error(clusters.status().message());
    std::cout << "Cluster Name List:\n";
    for (auto const& cluster : clusters->clusters) {
      std::cout << "Cluster Name: " << cluster.name() << "\n";
    }
    if (!clusters->failed_locations.empty()) {
      std::cout << "The Cloud Bigtable service reports that the following "
                   "locations are temporarily unavailable and no information "
                   "about clusters in these locations can be obtained:\n";
      for (auto const& failed_location : clusters->failed_locations) {
        std::cout << failed_location << "\n";
      }
    }

    std::cout << "\nDeleting Instance: ";
    google::cloud::Status status = instance_admin.DeleteInstance(instance_id);
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << " Done\n";
  }
  //! [run instance operations]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void CreateAppProfile(google::cloud::bigtable::InstanceAdmin instance_admin,
                      std::vector<std::string> const& argv) {
  //! [create app profile] [START bigtable_create_app_profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& profile_id) {
    auto config = cbt::AppProfileConfig::MultiClusterUseAny(profile_id);
    StatusOr<google::bigtable::admin::v2::AppProfile> profile =
        instance_admin.CreateAppProfile(instance_id, config);
    if (!profile) throw std::runtime_error(profile.status().message());
    std::cout << "New profile created with name=" << profile->name() << "\n";
  }
  //! [create app profile] [END bigtable_create_app_profile]
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void CreateAppProfileCluster(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    std::vector<std::string> const& argv) {
  //! [create app profile cluster]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& profile_id, std::string const& cluster_id) {
    auto config =
        cbt::AppProfileConfig::SingleClusterRouting(profile_id, cluster_id);
    StatusOr<google::bigtable::admin::v2::AppProfile> profile =
        instance_admin.CreateAppProfile(instance_id, config);
    if (!profile) throw std::runtime_error(profile.status().message());
    std::cout << "New profile created with name=" << profile->name() << "\n";
  }
  //! [create app profile cluster]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void GetAppProfile(google::cloud::bigtable::InstanceAdmin instance_admin,
                   std::vector<std::string> const& argv) {
  //! [get app profile] [START bigtable_get_app_profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& profile_id) {
    StatusOr<google::bigtable::admin::v2::AppProfile> profile =
        instance_admin.GetAppProfile(instance_id, profile_id);
    if (!profile) throw std::runtime_error(profile.status().message());
    std::cout << "Application Profile details=" << profile->DebugString()
              << "\n";
  }
  //! [get app profile] [END bigtable_get_app_profile]
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void UpdateAppProfileDescription(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    std::vector<std::string> const& argv) {
  //! [update app profile description] [START bigtable_update_app_profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& profile_id, std::string const& description) {
    future<StatusOr<google::bigtable::admin::v2::AppProfile>> profile_future =
        instance_admin.UpdateAppProfile(
            instance_id, profile_id,
            cbt::AppProfileUpdateConfig().set_description(description));
    auto profile = profile_future.get();
    if (!profile) throw std::runtime_error(profile.status().message());
    std::cout << "Updated AppProfile: " << profile->DebugString() << "\n";
  }
  //! [update app profile description] [END bigtable_update_app_profile]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void UpdateAppProfileRoutingAny(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    std::vector<std::string> const& argv) {
  //! [update app profile routing any] [START bigtable_update_app_profile]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& profile_id) {
    future<StatusOr<google::bigtable::admin::v2::AppProfile>> profile_future =
        instance_admin.UpdateAppProfile(instance_id, profile_id,
                                        cbt::AppProfileUpdateConfig()
                                            .set_multi_cluster_use_any()
                                            .set_ignore_warnings(true));
    auto profile = profile_future.get();
    if (!profile) throw std::runtime_error(profile.status().message());
    std::cout << "Updated AppProfile: " << profile->DebugString() << "\n";
  }
  //! [update app profile routing any] [END bigtable_update_app_profile]
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void UpdateAppProfileRoutingSingleCluster(
    google::cloud::bigtable::InstanceAdmin instance_admin,
    std::vector<std::string> const& argv) {
  //! [update app profile routing]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& profile_id, std::string const& cluster_id) {
    future<StatusOr<google::bigtable::admin::v2::AppProfile>> profile_future =
        instance_admin.UpdateAppProfile(
            instance_id, profile_id,
            cbt::AppProfileUpdateConfig()
                .set_single_cluster_routing(cluster_id)
                .set_ignore_warnings(true));
    auto profile = profile_future.get();
    if (!profile) throw std::runtime_error(profile.status().message());
    std::cout << "Updated AppProfile: " << profile->DebugString() << "\n";
  }
  //! [update app profile routing]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void ListAppProfiles(google::cloud::bigtable::InstanceAdmin instance_admin,
                     std::vector<std::string> const& argv) {
  //! [list app profiles]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id) {
    StatusOr<std::vector<google::bigtable::admin::v2::AppProfile>> profiles =
        instance_admin.ListAppProfiles(instance_id);
    if (!profiles) throw std::runtime_error(profiles.status().message());
    std::cout << "The " << instance_id << " instance has " << profiles->size()
              << " application profiles\n";
    for (auto const& profile : *profiles) {
      std::cout << profile.DebugString() << "\n";
    }
  }
  //! [list app profiles]
  (std::move(instance_admin), argv.at(0));
}

void DeleteAppProfile(std::vector<std::string> const& argv) {
  std::string basic_usage =
      "delete-app-profile <project-id> <instance-id> <profile-id>"
      " [ignore-warnings (default: true)]";
  if (argv.size() != 3 && argv.size() != 4) {
    throw Usage{basic_usage};
  }

  bool ignore_warnings = true;
  if (argv.size() == 4) {
    std::string arg = argv[3];
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
  google::cloud::bigtable::InstanceAdmin instance(
      google::cloud::bigtable::CreateDefaultInstanceAdminClient(
          argv[0], google::cloud::bigtable::ClientOptions()));

  //! [delete app profile] [START bigtable_delete_app_profile]
  namespace cbt = google::cloud::bigtable;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& profile_id, bool ignore_warnings) {
    google::cloud::Status status = instance_admin.DeleteAppProfile(
        instance_id, profile_id, ignore_warnings);
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Application Profile deleted\n";
  }
  //! [delete app profile] [END bigtable_delete_app_profile]
  (std::move(instance), argv[1], argv[2], ignore_warnings);
}

void GetIamPolicy(google::cloud::bigtable::InstanceAdmin instance_admin,
                  std::vector<std::string> const& argv) {
  //! [get iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id) {
    StatusOr<google::cloud::IamPolicy> policy =
        instance_admin.GetIamPolicy(instance_id);
    if (!policy) throw std::runtime_error(policy.status().message());
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
  (std::move(instance_admin), argv.at(0));
}

void SetIamPolicy(google::cloud::bigtable::InstanceAdmin instance_admin,
                  std::vector<std::string> const& argv) {
  //! [set iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& role, std::string const& member) {
    StatusOr<google::cloud::IamPolicy> current =
        instance_admin.GetIamPolicy(instance_id);
    if (!current) throw std::runtime_error(current.status().message());
    auto bindings = current->bindings;
    bindings.AddMember(role, member);
    StatusOr<google::cloud::IamPolicy> policy =
        instance_admin.SetIamPolicy(instance_id, bindings, current->etag);
    if (!policy) throw std::runtime_error(policy.status().message());
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
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void GetNativeIamPolicy(google::cloud::bigtable::InstanceAdmin instance_admin,
                        std::vector<std::string> const& argv) {
  //! [get native iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id) {
    StatusOr<google::iam::v1::Policy> policy =
        instance_admin.GetNativeIamPolicy(instance_id);
    if (!policy) throw std::runtime_error(policy.status().message());
    std::cout << "The IAM Policy for " << instance_id << " is\n"
              << policy->DebugString() << "\n";
  }
  //! [get native iam policy]
  (std::move(instance_admin), argv.at(0));
}

void SetNativeIamPolicy(google::cloud::bigtable::InstanceAdmin instance_admin,
                        std::vector<std::string> const& argv) {
  //! [set native iam policy]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& instance_id,
     std::string const& role, std::string const& member) {
    StatusOr<google::iam::v1::Policy> current =
        instance_admin.GetNativeIamPolicy(instance_id);
    if (!current) throw std::runtime_error(current.status().message());
    // This example adds the member to all existing bindings for that role. If
    // there are no such bindgs, it adds a new one. This might not be what the
    // user wants, e.g. in case of conditional bindings.
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
    StatusOr<google::iam::v1::Policy> policy =
        instance_admin.SetIamPolicy(instance_id, *current);
    if (!policy) throw std::runtime_error(policy.status().message());
    std::cout << "The IAM Policy for " << instance_id << " is\n"
              << policy->DebugString() << "\n";
  }
  //! [set native iam policy]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void TestIamPermissions(std::vector<std::string> const& argv) {
  if (argv.size() < 3) {
    throw Usage{
        "test-iam-permissions <project-id> <resource-id>"
        " <permission> [permission ...]"};
  }
  auto it = argv.cbegin();
  auto const project_id = *it++;
  auto const resource = *it++;
  std::vector<std::string> const permissions(it, argv.cend());

  google::cloud::bigtable::InstanceAdmin instance(
      google::cloud::bigtable::CreateDefaultInstanceAdminClient(
          project_id, google::cloud::bigtable::ClientOptions()));

  //! [test iam permissions]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::InstanceAdmin instance_admin, std::string const& resource,
     std::vector<std::string> const& permissions) {
    StatusOr<std::vector<std::string>> result =
        instance_admin.TestIamPermissions(resource, permissions);
    if (!result) throw std::runtime_error(result.status().message());
    std::cout << "The current user has the following permissions [";
    char const* sep = "";
    for (auto const& p : *result) {
      std::cout << sep << p;
      sep = ", ";
    }
    std::cout << "]\n";
  }
  //! [test iam permissions]
  (std::move(instance), std::move(resource), std::move(permissions));
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

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  examples::CleanupOldInstances("exin-", admin);

  // Create a different instance id to run the replicated instance example.
  {
    auto const id = examples::RandomInstanceId("exin-", generator);
    std::cout << "\nRunning CreateReplicatedInstance() example" << std::endl;
    CreateReplicatedInstance(admin, {id, zone_a, zone_b});
    std::cout << "\nRunning GetInstance() example" << std::endl;
    GetInstance(admin, {id});
    (void)admin.DeleteInstance(id);
  }

  // Create a different instance id to run the development instance example.
  {
    auto const id = examples::RandomInstanceId("exin-", generator);
    std::cout << "\nRunning CreateDevInstance() example" << std::endl;
    CreateDevInstance(admin, {id, zone_a});
    std::cout << "\nRunning UpdateInstance() example" << std::endl;
    UpdateInstance(admin, {id});
    (void)admin.DeleteInstance(id);
  }

  // Run the legacy "run instance operations" example using a different instance
  // id
  {
    auto const id = examples::RandomInstanceId("exin-", generator);
    auto const cluster_id = examples::RandomClusterId("exin-c1-", generator);
    std::cout << "\nRunning RunInstanceOperations() example" << std::endl;
    RunInstanceOperations(admin, {id, cluster_id, zone_a});
  }

  auto const instance_id = examples::RandomInstanceId("exin-", generator);

  std::cout << "\nRunning CheckInstanceExists() example [1]" << std::endl;
  CheckInstanceExists(admin, {instance_id});

  std::cout << "\nRunning CreateInstance() example" << std::endl;
  CreateInstance(admin, {instance_id, zone_a});

  std::cout << "\nRunning CheckInstanceExists() example [2]" << std::endl;
  CheckInstanceExists(admin, {instance_id});

  std::cout << "\nRunning ListInstances() example" << std::endl;
  ListInstances(admin, {});

  std::cout << "\nRunning GetInstance() example" << std::endl;
  GetInstance(admin, {instance_id});

  std::cout << "\nRunning ListClusters() example" << std::endl;
  ListClusters(admin, {instance_id});

  std::cout << "\nRunning ListAllClusters() example" << std::endl;
  ListAllClusters(admin, {});

  std::cout << "\nRunning CreateCluster() example" << std::endl;
  CreateCluster(admin, {instance_id, instance_id + "-c2", zone_b});

  std::cout << "\nRunning UpdateCluster() example" << std::endl;
  UpdateCluster(admin, {instance_id, instance_id + "-c2"});

  std::cout << "\nRunning GetCluster() example" << std::endl;
  GetCluster(admin, {instance_id, instance_id + "-c2"});

  std::cout << "\nRunning CreateAppProfile example" << std::endl;
  CreateAppProfile(admin, {instance_id, "profile-p1"});

  std::cout << "\nRunning DeleteAppProfile() example [1]" << std::endl;
  DeleteAppProfile({project_id, instance_id, "profile-p1", "true"});

  std::cout << "\nRunning CreateAppProfileCluster() example" << std::endl;
  CreateAppProfileCluster(admin,
                          {instance_id, "profile-p2", instance_id + "-c2"});

  std::cout << "\nRunning ListAppProfiles() example" << std::endl;
  ListAppProfiles(admin, {instance_id});

  std::cout << "\nRunning GetAppProfile() example" << std::endl;
  GetAppProfile(admin, {instance_id, "profile-p2"});

  std::cout << "\nRunning UpdateAppProfileDescription() example" << std::endl;
  UpdateAppProfileDescription(
      admin, {instance_id, "profile-p2", "A profile for examples"});

  std::cout << "\nRunning UpdateProfileRoutingAny() example" << std::endl;
  UpdateAppProfileRoutingAny(admin, {instance_id, "profile-p2"});

  std::cout << "\nRunning UpdateProfileRouting() example" << std::endl;
  UpdateAppProfileRoutingSingleCluster(
      admin, {instance_id, "profile-p2", instance_id + "-c2"});

  std::cout << "\nRunning DeleteAppProfile() example [2]" << std::endl;
  try {
    DeleteAppProfile({project_id, instance_id, "profile-p2", "invalid"});
  } catch (Usage const&) {
  }

  try {
    // Running with ignore_warnings==false almost always fails, I am not even
    // sure why we have that lever. In any case, we need to test both branches
    // of the code, so do that here.
    std::cout << "\nRunning DeleteAppProfile() example [3]" << std::endl;
    DeleteAppProfile({project_id, instance_id, "profile-p2", "false"});
  } catch (std::exception const&) {
    std::cout << "\nRunning DeleteAppProfile() example [4]" << std::endl;
    DeleteAppProfile({project_id, instance_id, "profile-p2"});
  }

  std::cout << "\nRunning DeleteCluster() example" << std::endl;
  DeleteCluster(admin, {instance_id, instance_id + "-c2"});

  std::cout << "\nRunning GetIamPolicy() example" << std::endl;
  GetIamPolicy(admin, {instance_id});

  std::cout << "\nRunning SetIamPolicy() example" << std::endl;
  SetIamPolicy(admin, {instance_id, "roles/bigtable.user",
                       "serviceAccount:" + service_account});

  std::cout << "\nRunning GetNativeIamPolicy() example" << std::endl;
  GetNativeIamPolicy(admin, {instance_id});

  std::cout << "\nRunning SetNativeIamPolicy() example" << std::endl;
  SetNativeIamPolicy(admin, {instance_id, "roles/bigtable.user",
                             "serviceAccount:" + service_account});

  std::cout << "\nRunning TestIamPermissions() example" << std::endl;
  TestIamPermissions({project_id, instance_id, "bigtable.instances.delete"});

  std::cout << "\nRunning DeleteInstance() example" << std::endl;
  DeleteInstance(admin, {instance_id});
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InstallCrashHandler(argv[0]);

  namespace examples = google::cloud::bigtable::examples;
  examples::Example example({
      examples::MakeCommandEntry("create-instance", {"<instance-id>", "<zone>"},
                                 CreateInstance),
      examples::MakeCommandEntry("create-dev-instance",
                                 {"<instance-id>", "<zone>"},
                                 CreateDevInstance),
      examples::MakeCommandEntry("create-replicated-instance",
                                 {"<instance-id>", "<zone-a>", "<zone-b>"},
                                 CreateReplicatedInstance),
      examples::MakeCommandEntry("update-instance", {"<instance-id>"},
                                 UpdateInstance),
      examples::MakeCommandEntry("list-instances", {}, ListInstances),
      examples::MakeCommandEntry("check-instance-exists", {"<instance-id>"},
                                 CheckInstanceExists),
      examples::MakeCommandEntry("get-instance", {"<instance-id>"},
                                 GetInstance),
      examples::MakeCommandEntry("delete-instance", {"<instance-id>"},
                                 DeleteInstance),
      examples::MakeCommandEntry("create-cluster",
                                 {"<instance-id>", "<cluster-id>", "<zone>"},
                                 CreateCluster),
      examples::MakeCommandEntry("list-clusters", {"<instance-id>"},
                                 ListClusters),
      examples::MakeCommandEntry("list-all-clusters", {}, ListAllClusters),
      examples::MakeCommandEntry(
          "update-cluster", {"<instance-id>", "<cluster-id>"}, UpdateCluster),
      examples::MakeCommandEntry("get-cluster",
                                 {"<instance-id>", "<cluster-id>"}, GetCluster),
      examples::MakeCommandEntry(
          "delete-cluster", {"<instance-id>", "<cluster-id>"}, DeleteCluster),
      examples::MakeCommandEntry("run",
                                 {"<instance-id>", "<cluster-id>", "<zone>"},
                                 RunInstanceOperations),
      examples::MakeCommandEntry("create-app-profile",
                                 {"<instance-id>", "<profile-id>"},
                                 CreateAppProfile),
      examples::MakeCommandEntry(
          "create-app-profile-cluster",
          {"<instance-id>", "<profile-id>", "<cluster-id>"},
          CreateAppProfileCluster),
      examples::MakeCommandEntry(
          "get-app-profile", {"<instance-id>", "<profile-id>"}, GetAppProfile),
      examples::MakeCommandEntry(
          "update-app-profile-description",
          {"<instance-id>", "<profile-id>", "<new-description>"},
          UpdateAppProfileDescription),
      examples::MakeCommandEntry("update-app-profile-routing-any",
                                 {"<instance-id>", "<profile-id>"},
                                 UpdateAppProfileRoutingAny),
      examples::MakeCommandEntry(
          "update-app-profile-routing",
          {"<instance-id>", "<profile-id>", "<cluster-id>"},
          UpdateAppProfileRoutingSingleCluster),
      examples::MakeCommandEntry("list-app-profiles", {"<instance-id>"},
                                 ListAppProfiles),
      {"delete-app-profile", DeleteAppProfile},
      examples::MakeCommandEntry("get-iam-policy", {"<instance-id>"},
                                 GetIamPolicy),
      examples::MakeCommandEntry("set-iam-policy",
                                 {"<instance-id>", "<role>", "<member>"},
                                 SetIamPolicy),
      examples::MakeCommandEntry("get-native-iam-policy", {"<instance-id>"},
                                 GetNativeIamPolicy),
      examples::MakeCommandEntry("set-native-iam-policy",
                                 {"<instance-id>", "<role>", "<member>"},
                                 SetNativeIamPolicy),
      {"test-iam-permissions", TestIamPermissions},
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}
