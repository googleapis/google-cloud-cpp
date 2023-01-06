// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/admin/bigtable_instance_admin_client.h"
#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/iam_binding.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/absl_str_join_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/project.h"
#include <iterator>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void CreateInstance(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [create instance] [START bigtable_create_prod_instance]
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::Project;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& zone) {
    std::string project_name = Project(project_id).FullName();
    std::string cluster_id = instance_id + "-c1";

    google::bigtable::admin::v2::Instance in;
    in.set_type(google::bigtable::admin::v2::Instance::PRODUCTION);
    in.set_display_name("Put description here");

    google::bigtable::admin::v2::Cluster cluster;
    cluster.set_location(project_name + "/locations/" + zone);
    cluster.set_serve_nodes(3);
    cluster.set_default_storage_type(google::bigtable::admin::v2::HDD);

    std::map<std::string, google::bigtable::admin::v2::Cluster> cluster_map = {
        {cluster_id, std::move(cluster)}};

    future<StatusOr<google::bigtable::admin::v2::Instance>> instance_future =
        instance_admin.CreateInstance(project_name, instance_id, std::move(in),
                                      std::move(cluster_map));
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
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void CreateDevInstance(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [create dev instance] [START bigtable_create_dev_instance]
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::Project;
  using ::google::cloud::StatusOr;

  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& zone) {
    std::string project_name = Project(project_id).FullName();
    std::string cluster_id = instance_id + "-c1";

    google::bigtable::admin::v2::Instance in;
    in.set_type(google::bigtable::admin::v2::Instance::DEVELOPMENT);
    in.set_display_name("Put description here");

    google::bigtable::admin::v2::Cluster cluster;
    cluster.set_location(project_name + "/locations/" + zone);
    cluster.set_serve_nodes(0);
    cluster.set_default_storage_type(google::bigtable::admin::v2::HDD);

    std::map<std::string, google::bigtable::admin::v2::Cluster> cluster_map = {
        {cluster_id, std::move(cluster)}};

    future<StatusOr<google::bigtable::admin::v2::Instance>> instance_future =
        instance_admin.CreateInstance(project_name, instance_id, std::move(in),
                                      std::move(cluster_map));
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
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void CreateReplicatedInstance(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_create_replicated_cluster]
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::Project;
  using ::google::cloud::StatusOr;

  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& zone_a, std::string const& zone_b) {
    std::string project_name = Project(project_id).FullName();
    std::string c1 = instance_id + "-c1";
    std::string c2 = instance_id + "-c2";

    google::bigtable::admin::v2::Instance in;
    in.set_type(google::bigtable::admin::v2::Instance::PRODUCTION);
    in.set_display_name("Put description here");

    google::bigtable::admin::v2::Cluster cluster1;
    cluster1.set_location(project_name + "/locations/" + zone_a);
    cluster1.set_serve_nodes(3);
    cluster1.set_default_storage_type(google::bigtable::admin::v2::HDD);

    google::bigtable::admin::v2::Cluster cluster2;
    cluster2.set_location(project_name + "/locations/" + zone_b);
    cluster2.set_serve_nodes(3);
    cluster2.set_default_storage_type(google::bigtable::admin::v2::HDD);

    std::map<std::string, google::bigtable::admin::v2::Cluster> cluster_map = {
        {c1, std::move(cluster1)}, {c2, std::move(cluster2)}};

    future<StatusOr<google::bigtable::admin::v2::Instance>> instance_future =
        instance_admin.CreateInstance(project_name, instance_id, std::move(in),
                                      std::move(cluster_map));
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
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void UpdateInstance(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [update instance]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    StatusOr<google::bigtable::admin::v2::Instance> instance =
        instance_admin.GetInstance(instance_name);
    if (!instance) throw std::runtime_error(instance.status().message());
    // Modify the instance and prepare the mask with the modified field
    instance->set_display_name("Modified Display Name");
    google::protobuf::FieldMask mask;
    mask.add_paths("display_name");

    future<StatusOr<google::bigtable::admin::v2::Instance>> instance_future =
        instance_admin.PartialUpdateInstance(std::move(*instance),
                                             std::move(mask));
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
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void ListInstances(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [list instances] [START bigtable_list_instances]
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::Project;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id) {
    std::string project_name = Project(project_id).FullName();
    StatusOr<google::bigtable::admin::v2::ListInstancesResponse> instances =
        instance_admin.ListInstances(project_name);
    if (!instances) throw std::runtime_error(instances.status().message());
    for (auto const& instance : instances->instances()) {
      std::cout << instance.name() << "\n";
    }
    if (!instances->failed_locations().empty()) {
      std::cout << "The Cloud Bigtable service reports that the following "
                   "locations are temporarily unavailable and no information "
                   "about instances in these locations can be obtained:\n";
      for (auto const& failed_location : instances->failed_locations()) {
        std::cout << failed_location << "\n";
      }
    }
  }
  //! [list instances] [END bigtable_list_instances]
  (std::move(instance_admin), argv.at(0));
}

void CheckInstanceExists(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  // [START bigtable_check_instance_exists]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    auto instance = instance_admin.GetInstance(instance_name);
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
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void GetInstance(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [get instance] [START bigtable_get_instance]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    StatusOr<google::bigtable::admin::v2::Instance> instance =
        instance_admin.GetInstance(instance_name);
    if (!instance) throw std::runtime_error(instance.status().message());
    std::cout << "GetInstance details : " << instance->DebugString() << "\n";
  }
  //! [get instance] [END bigtable_get_instance]
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void DeleteInstance(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [delete instance] [START bigtable_delete_instance]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::Status;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    Status status = instance_admin.DeleteInstance(instance_name);
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Successfully deleted the instance " << instance_id << "\n";
  }
  //! [delete instance] [END bigtable_delete_instance]
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void CreateCluster(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [create cluster] [START bigtable_create_cluster]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::Project;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& cluster_id, std::string const& zone) {
    std::string project_name = Project(project_id).FullName();
    std::string instance_name = cbt::InstanceName(project_id, instance_id);

    google::bigtable::admin::v2::Cluster c;
    c.set_location(project_name + "/locations/" + zone);
    c.set_serve_nodes(3);
    c.set_default_storage_type(google::bigtable::admin::v2::HDD);

    future<StatusOr<google::bigtable::admin::v2::Cluster>> cluster_future =
        instance_admin.CreateCluster(instance_name, cluster_id, std::move(c));

    // Applications can wait asynchronously, in this example we just block.
    auto cluster = cluster_future.get();
    if (!cluster) throw std::runtime_error(cluster.status().message());
    std::cout << "Successfully created cluster " << cluster->name() << "\n";
  }
  //! [create cluster] [END bigtable_create_cluster]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void ListClusters(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [list clusters] [START bigtable_get_clusters]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    StatusOr<google::bigtable::admin::v2::ListClustersResponse> clusters =
        instance_admin.ListClusters(instance_name);
    if (!clusters) throw std::runtime_error(clusters.status().message());
    std::cout << "Cluster Name List\n";
    for (auto const& cluster : clusters->clusters()) {
      std::cout << "Cluster Name:" << cluster.name() << "\n";
    }
    if (!clusters->failed_locations().empty()) {
      std::cout << "The Cloud Bigtable service reports that the following "
                   "locations are temporarily unavailable and no information "
                   "about clusters in these locations can be obtained:\n";
      for (auto const& failed_location : clusters->failed_locations()) {
        std::cout << failed_location << "\n";
      }
    }
  }
  //! [list clusters] [END bigtable_get_clusters]
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void ListAllClusters(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [list all clusters] [START bigtable_get_clusters]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id) {
    std::string instance_name = cbt::InstanceName(project_id, "-");
    StatusOr<google::bigtable::admin::v2::ListClustersResponse> clusters =
        instance_admin.ListClusters(instance_name);
    if (!clusters) throw std::runtime_error(clusters.status().message());
    std::cout << "Cluster Name List\n";
    for (auto const& cluster : clusters->clusters()) {
      std::cout << "Cluster Name:" << cluster.name() << "\n";
    }
    if (!clusters->failed_locations().empty()) {
      std::cout << "The Cloud Bigtable service reports that the following "
                   "locations are temporarily unavailable and no information "
                   "about clusters in these locations can be obtained:\n";
      for (auto const& failed_location : clusters->failed_locations()) {
        std::cout << failed_location << "\n";
      }
    }
  }
  //! [list all clusters] [END bigtable_get_clusters]
  (std::move(instance_admin), argv.at(0));
}

void UpdateCluster(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [update cluster]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& cluster_id) {
    std::string cluster_name =
        cbt::ClusterName(project_id, instance_id, cluster_id);
    // GetCluster first and then modify it.
    StatusOr<google::bigtable::admin::v2::Cluster> cluster =
        instance_admin.GetCluster(cluster_name);
    if (!cluster) throw std::runtime_error(cluster.status().message());

    // The state cannot be sent on updates, so clear it first.
    cluster->clear_state();
    // Set the desired cluster configuration.
    cluster->set_serve_nodes(4);

    auto modified_cluster =
        instance_admin.UpdateCluster(std::move(*cluster)).get();
    if (!modified_cluster) {
      throw std::runtime_error(modified_cluster.status().message());
    }
    std::cout << "cluster details : " << cluster->DebugString() << "\n";
  }
  //! [update cluster]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void GetCluster(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [get cluster] [START bigtable_get_cluster]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& cluster_id) {
    std::string cluster_name =
        cbt::ClusterName(project_id, instance_id, cluster_id);
    StatusOr<google::bigtable::admin::v2::Cluster> cluster =
        instance_admin.GetCluster(cluster_name);
    if (!cluster) throw std::runtime_error(cluster.status().message());
    std::cout << "GetCluster details : " << cluster->DebugString() << "\n";
  }
  //! [get cluster] [END bigtable_get_cluster]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void DeleteCluster(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [delete cluster] [START bigtable_delete_cluster]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::Status;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& cluster_id) {
    std::string cluster_name =
        cbt::ClusterName(project_id, instance_id, cluster_id);
    Status status = instance_admin.DeleteCluster(cluster_name);
    if (!status.ok()) throw std::runtime_error(status.message());
  }
  //! [delete cluster] [END bigtable_delete_cluster]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void CreateAppProfile(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [create app profile] [START bigtable_create_app_profile]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& profile_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);

    google::bigtable::admin::v2::AppProfile ap;
    ap.mutable_multi_cluster_routing_use_any()->Clear();

    StatusOr<google::bigtable::admin::v2::AppProfile> profile =
        instance_admin.CreateAppProfile(instance_name, profile_id,
                                        std::move(ap));
    if (!profile) throw std::runtime_error(profile.status().message());
    std::cout << "New profile created with name=" << profile->name() << "\n";
  }
  //! [create app profile] [END bigtable_create_app_profile]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void CreateAppProfileCluster(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [create app profile cluster]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& profile_id, std::string const& cluster_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);

    google::bigtable::admin::v2::AppProfile ap;
    ap.mutable_single_cluster_routing()->set_cluster_id(cluster_id);

    StatusOr<google::bigtable::admin::v2::AppProfile> profile =
        instance_admin.CreateAppProfile(instance_name, profile_id,
                                        std::move(ap));
    if (!profile) throw std::runtime_error(profile.status().message());
    std::cout << "New profile created with name=" << profile->name() << "\n";
  }
  //! [create app profile cluster]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void GetAppProfile(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [get app profile] [START bigtable_get_app_profile]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& profile_id) {
    std::string profile_name =
        cbt::AppProfileName(project_id, instance_id, profile_id);
    StatusOr<google::bigtable::admin::v2::AppProfile> profile =
        instance_admin.GetAppProfile(profile_name);
    if (!profile) throw std::runtime_error(profile.status().message());
    std::cout << "Application Profile details=" << profile->DebugString()
              << "\n";
  }
  //! [get app profile] [END bigtable_get_app_profile]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void UpdateAppProfileDescription(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [update app profile description] [START bigtable_update_app_profile]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& profile_id, std::string const& description) {
    std::string profile_name =
        cbt::AppProfileName(project_id, instance_id, profile_id);

    google::bigtable::admin::v2::AppProfile profile;
    profile.set_name(profile_name);
    profile.set_description(description);

    google::protobuf::FieldMask mask;
    mask.add_paths("description");

    future<StatusOr<google::bigtable::admin::v2::AppProfile>> profile_future =
        instance_admin.UpdateAppProfile(std::move(profile), std::move(mask));
    auto modified_profile = profile_future.get();
    if (!modified_profile)
      throw std::runtime_error(modified_profile.status().message());
    std::cout << "Updated AppProfile: " << modified_profile->DebugString()
              << "\n";
  }
  //! [update app profile description] [END bigtable_update_app_profile]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void UpdateAppProfileRoutingAny(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [update app profile routing any] [START bigtable_update_app_profile]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& profile_id) {
    std::string profile_name =
        cbt::AppProfileName(project_id, instance_id, profile_id);

    google::bigtable::admin::v2::UpdateAppProfileRequest req;
    req.mutable_app_profile()->set_name(profile_name);
    req.mutable_app_profile()->mutable_multi_cluster_routing_use_any()->Clear();
    req.mutable_update_mask()->add_paths("multi_cluster_routing_use_any");
    req.set_ignore_warnings(true);

    future<StatusOr<google::bigtable::admin::v2::AppProfile>> profile_future =
        instance_admin.UpdateAppProfile(std::move(req));
    auto modified_profile = profile_future.get();
    if (!modified_profile)
      throw std::runtime_error(modified_profile.status().message());
    std::cout << "Updated AppProfile: " << modified_profile->DebugString()
              << "\n";
  }
  //! [update app profile routing any] [END bigtable_update_app_profile]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2));
}

void UpdateAppProfileRoutingSingleCluster(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [update app profile routing]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::future;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& profile_id, std::string const& cluster_id) {
    std::string profile_name =
        cbt::AppProfileName(project_id, instance_id, profile_id);

    google::bigtable::admin::v2::UpdateAppProfileRequest req;
    req.mutable_app_profile()->set_name(profile_name);
    req.mutable_app_profile()->mutable_single_cluster_routing()->set_cluster_id(
        cluster_id);
    req.mutable_update_mask()->add_paths("single_cluster_routing");
    req.set_ignore_warnings(true);

    future<StatusOr<google::bigtable::admin::v2::AppProfile>> profile_future =
        instance_admin.UpdateAppProfile(std::move(req));
    auto modified_profile = profile_future.get();
    if (!modified_profile)
      throw std::runtime_error(modified_profile.status().message());
    std::cout << "Updated AppProfile: " << modified_profile->DebugString()
              << "\n";
  }
  //! [update app profile routing]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void ListAppProfiles(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [list app profiles]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StreamRange;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    StreamRange<google::bigtable::admin::v2::AppProfile> profiles =
        instance_admin.ListAppProfiles(instance_name);
    std::cout << "The " << instance_id << " instance has "
              << std::distance(profiles.begin(), profiles.end())
              << " application profiles\n";
    for (auto const& profile : profiles) {
      if (!profile) throw std::runtime_error(profile.status().message());
      std::cout << profile->DebugString() << "\n";
    }
  }
  //! [list app profiles]
  (std::move(instance_admin), argv.at(0), argv.at(1));
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
    std::string const& arg = argv[3];
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

  auto instance_admin =
      google::cloud::bigtable_admin::BigtableInstanceAdminClient(
          google::cloud::bigtable_admin::MakeBigtableInstanceAdminConnection());

  //! [delete app profile] [START bigtable_delete_app_profile]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::Status;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& profile_id, bool ignore_warnings) {
    std::string profile_name =
        cbt::AppProfileName(project_id, instance_id, profile_id);

    google::bigtable::admin::v2::DeleteAppProfileRequest req;
    req.set_name(profile_name);
    req.set_ignore_warnings(ignore_warnings);

    Status status = instance_admin.DeleteAppProfile(std::move(req));
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Application Profile deleted\n";
  }
  //! [delete app profile] [END bigtable_delete_app_profile]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2),
   ignore_warnings);
}

void GetIamPolicy(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [get native iam policy]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    StatusOr<google::iam::v1::Policy> policy =
        instance_admin.GetIamPolicy(instance_name);
    if (!policy) throw std::runtime_error(policy.status().message());
    std::cout << "The IAM Policy for " << instance_id << " is\n"
              << policy->DebugString() << "\n";
  }
  //! [get native iam policy]
  (std::move(instance_admin), argv.at(0), argv.at(1));
}

void SetIamPolicy(
    google::cloud::bigtable_admin::BigtableInstanceAdminClient instance_admin,
    std::vector<std::string> const& argv) {
  //! [set native iam policy]
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& project_id, std::string const& instance_id,
     std::string const& role, std::string const& member) {
    std::string instance_name = cbt::InstanceName(project_id, instance_id);
    StatusOr<google::iam::v1::Policy> current =
        instance_admin.GetIamPolicy(instance_name);
    if (!current) throw std::runtime_error(current.status().message());
    // This example adds the member to all existing bindings for that role. If
    // there are no such bindings, it adds a new one. This might not be what the
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
        instance_admin.SetIamPolicy(instance_name, *current);
    if (!policy) throw std::runtime_error(policy.status().message());
    std::cout << "The IAM Policy for " << instance_id << " is\n"
              << policy->DebugString() << "\n";
  }
  //! [set native iam policy]
  (std::move(instance_admin), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

void TestIamPermissions(std::vector<std::string> const& argv) {
  std::string usage =
      "test-iam-permissions <resource-name> <permission> [<permission>...]";
  if (argv.size() < 2) {
    throw Usage{usage};
  }
  auto it = argv.cbegin();
  auto const resource_name = *it++;
  if (!absl::StartsWith(resource_name, "projects/")) {
    throw Usage{usage +
                "\nresource-name should be fully qualified. For example: "
                "'projects/my-project/instances/my-instance'"};
  }
  std::vector<std::string> const permissions(it, argv.cend());

  auto instance_admin =
      google::cloud::bigtable_admin::BigtableInstanceAdminClient(
          google::cloud::bigtable_admin::MakeBigtableInstanceAdminConnection());

  //! [test iam permissions]
  namespace cbta = ::google::cloud::bigtable_admin;
  using ::google::cloud::StatusOr;
  [](cbta::BigtableInstanceAdminClient instance_admin,
     std::string const& resource_name,
     std::vector<std::string> const& permissions) {
    auto result = instance_admin.TestIamPermissions(resource_name, permissions);
    if (!result) throw std::runtime_error(result.status().message());
    std::cout << "The current user has the following permissions [";
    std::cout << absl::StrJoin(result->permissions(), ", ");
    std::cout << "]\n";
  }
  //! [test iam permissions]
  (std::move(instance_admin), std::move(resource_name), std::move(permissions));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = ::google::cloud::bigtable;
  namespace cbta = ::google::cloud::bigtable_admin;

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

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto conn = cbta::MakeBigtableInstanceAdminConnection();
  cbt::testing::CleanupStaleInstances(conn, project_id);
  cbta::BigtableInstanceAdminClient admin(std::move(conn));

  // Create a different instance id to run the replicated instance example.
  {
    auto const id = cbt::testing::RandomInstanceId(generator);
    std::cout << "\nRunning CreateReplicatedInstance() example" << std::endl;
    CreateReplicatedInstance(admin, {project_id, id, zone_a, zone_b});
    std::cout << "\nRunning GetInstance() example" << std::endl;
    GetInstance(admin, {project_id, id});
    (void)admin.DeleteInstance(cbt::InstanceName(project_id, id));
  }

  // Create a different instance id to run the development instance example.
  {
    auto const id = cbt::testing::RandomInstanceId(generator);
    std::cout << "\nRunning CreateDevInstance() example" << std::endl;
    CreateDevInstance(admin, {project_id, id, zone_a});
    std::cout << "\nRunning UpdateInstance() example" << std::endl;
    UpdateInstance(admin, {project_id, id});
    (void)admin.DeleteInstance(cbt::InstanceName(project_id, id));
  }

  auto const instance_id = cbt::testing::RandomInstanceId(generator);

  std::cout << "\nRunning CheckInstanceExists() example [1]" << std::endl;
  CheckInstanceExists(admin, {project_id, instance_id});

  std::cout << "\nRunning CreateInstance() example" << std::endl;
  CreateInstance(admin, {project_id, instance_id, zone_a});

  std::cout << "\nRunning CheckInstanceExists() example [2]" << std::endl;
  CheckInstanceExists(admin, {project_id, instance_id});

  std::cout << "\nRunning ListInstances() example" << std::endl;
  ListInstances(admin, {project_id});

  std::cout << "\nRunning GetInstance() example" << std::endl;
  GetInstance(admin, {project_id, instance_id});

  std::cout << "\nRunning ListClusters() example" << std::endl;
  ListClusters(admin, {project_id, instance_id});

  std::cout << "\nRunning ListAllClusters() example" << std::endl;
  ListAllClusters(admin, {project_id});

  std::cout << "\nRunning CreateCluster() example" << std::endl;
  CreateCluster(admin, {project_id, instance_id, instance_id + "-c2", zone_b});

  std::cout << "\nRunning UpdateCluster() example" << std::endl;
  UpdateCluster(admin, {project_id, instance_id, instance_id + "-c2"});

  std::cout << "\nRunning GetCluster() example" << std::endl;
  GetCluster(admin, {project_id, instance_id, instance_id + "-c2"});

  std::cout << "\nRunning CreateAppProfile example" << std::endl;
  CreateAppProfile(admin, {project_id, instance_id, "profile-p1"});

  std::cout << "\nRunning DeleteAppProfile() example [1]" << std::endl;
  DeleteAppProfile({project_id, instance_id, "profile-p1", "true"});

  std::cout << "\nRunning CreateAppProfileCluster() example" << std::endl;
  CreateAppProfileCluster(
      admin, {project_id, instance_id, "profile-p2", instance_id + "-c2"});

  std::cout << "\nRunning ListAppProfiles() example" << std::endl;
  ListAppProfiles(admin, {project_id, instance_id});

  std::cout << "\nRunning GetAppProfile() example" << std::endl;
  GetAppProfile(admin, {project_id, instance_id, "profile-p2"});

  std::cout << "\nRunning UpdateAppProfileDescription() example" << std::endl;
  UpdateAppProfileDescription(
      admin, {project_id, instance_id, "profile-p2", "A profile for examples"});

  std::cout << "\nRunning UpdateProfileRoutingAny() example" << std::endl;
  UpdateAppProfileRoutingAny(admin, {project_id, instance_id, "profile-p2"});

  std::cout << "\nRunning UpdateProfileRouting() example" << std::endl;
  UpdateAppProfileRoutingSingleCluster(
      admin, {project_id, instance_id, "profile-p2", instance_id + "-c2"});

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
  DeleteCluster(admin, {project_id, instance_id, instance_id + "-c2"});

  std::cout << "\nRunning GetIamPolicy() example" << std::endl;
  GetIamPolicy(admin, {project_id, instance_id});

  std::cout << "\nRunning SetIamPolicy() example" << std::endl;
  SetIamPolicy(admin, {project_id, instance_id, "roles/bigtable.user",
                       "serviceAccount:" + service_account});

  std::cout << "\nRunning TestIamPermissions() example" << std::endl;
  TestIamPermissions({cbt::InstanceName(project_id, instance_id),
                      "bigtable.instances.delete"});

  std::cout << "\nRunning DeleteInstance() example" << std::endl;
  DeleteInstance(admin, {project_id, instance_id});
}

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  namespace examples = ::google::cloud::bigtable::examples;
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
      {"test-iam-permissions", TestIamPermissions},
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
} catch (std::exception const& ex) {
  std::cerr << ex.what() << "\n";
  ::google::cloud::LogSink::Instance().Flush();
  return 1;
}
