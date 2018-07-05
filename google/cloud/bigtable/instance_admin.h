// Copyright 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_H_

#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/bigtable/instance_config.h"
#include "google/cloud/bigtable/instance_update_config.h"
#include "google/cloud/bigtable/internal/instance_admin.h"
#include <future>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Implements a minimal API to administer Cloud Bigtable instances.
 */
class InstanceAdmin {
 public:
  /**
   * @param client the interface to create grpc stubs, report errors, etc.
   */
  InstanceAdmin(std::shared_ptr<InstanceAdminClient> client)
      : impl_(std::move(client)) {}

  /**
   * Create a new InstanceAdmin using explicit policies to handle RPC errors.
   *
   * @tparam RPCRetryPolicy control which operations to retry and for how long.
   * @tparam RPCBackoffPolicy control how does the client backs off after an RPC
   *     error.
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param retry_policy the policy to handle RPC errors.
   * @param backoff_policy the policy to control backoff after an error.
   */
  template <typename RPCRetryPolicy, typename RPCBackoffPolicy>
  InstanceAdmin(std::shared_ptr<InstanceAdminClient> client,
                RPCRetryPolicy retry_policy, RPCBackoffPolicy backoff_policy)
      : impl_(std::move(client), std::move(retry_policy),
              std::move(backoff_policy)) {}

  /// The full name (`projects/<project_id>`) of the project.
  std::string const& project_name() const { return impl_.project_name(); }
  /// The project id, i.e., `project_name()` without the `projects/` prefix.
  std::string const& project_id() const { return impl_.project_id(); }

  /// Return the fully qualified name of the given instance_id.
  std::string InstanceName(std::string const& instance_id) const {
    return impl_.InstanceName(instance_id);
  }

  /**
   * Create a new instance of Cloud Bigtable.
   *
   * @warning Note that this is operation can take seconds or minutes to
   * complete. The application may prefer to perform other work while waiting
   * for this operation.
   *
   * @param instance_config a description of the new instance to be created.
   * @return a future that becomes satisfied when (a) the operation has
   *   completed successfully, in which case it returns a proto with the
   *   Instance details, (b) the operation has failed, in which case the future
   *   contains an exception (typically `bigtable::GrpcError`) with the details
   *   of the failure, or (c) the state of the operation is unknown after the
   *   time allocated by the retry policies has expired, in which case the
   *   future contains an exception of type `bigtable::PollTimeout`.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc create instance
   */
  std::future<google::bigtable::admin::v2::Instance> CreateInstance(
      InstanceConfig instance_config);

  /**
   * Create a new Cluster of Cloud Bigtable.
   *
   * @param cluster_config a description of the new cluster to be created.
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   created
   *
   *  @par Example
   *  @snippet bigtable_samples_instance_admin.cc create cluster
   */
  std::future<google::bigtable::admin::v2::Cluster> CreateCluster(
      ClusterConfig cluster_config, bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id);

  /**
   * Update an existing instance of Cloud Bigtable.
   *
   * @warning Note that this is operation can take seconds or minutes to
   * complete. The application may prefer to perform other work while waiting
   * for this operation.
   *
   * @param instance_update_config config with modified instance.
   * @return a future that becomes satisfied when (a) the operation has
   *   completed successfully, in which case it returns a proto with the
   *   Instance details, (b) the operation has failed, in which case the future
   *   contains an exception (typically `bigtable::GrpcError`) with the details
   *   of the failure, or (c) the state of the operation is unknown after the
   *   time allocated by the retry policies has expired, in which case the
   *   future contains an exception of type `bigtable::PollTimeout`.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc update instance
   */
  std::future<google::bigtable::admin::v2::Instance> UpdateInstance(
      InstanceUpdateConfig instance_update_config);

  /**
   * Return the list of instances in the project.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc list instances
   */
  std::vector<google::bigtable::admin::v2::Instance> ListInstances();

  /**
   * Return the details of @p instance_id.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc get instance
   */
  google::bigtable::admin::v2::Instance GetInstance(
      std::string const& instance_id);

  /**
   * Deletes the instances in the project.
   * @param instance_id the id of the instance in the project that needs to be
   * deleted
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc delete instance
   */
  void DeleteInstance(std::string const& instance_id);

  /**
   * Return the list of clusters in an instance.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc list clusters
   */
  std::vector<google::bigtable::admin::v2::Cluster> ListClusters();

  /**
   * Return the list of clusters in an instance.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc list clusters
   */
  std::vector<google::bigtable::admin::v2::Cluster> ListClusters(
      std::string const& instance_id);

  /**
   * Update an existing cluster of Cloud Bigtable.
   *
   * @warning Note that this is operation can take seconds or minutes to
   * complete. The application may prefer to perform other work while waiting
   * for this operation.
   *
   * @param cluster_config cluster with updated values.
   * @return a future that becomes satisfied when (a) the operation has
   *   completed successfully, in which case it returns a proto with the
   *   Instance details, (b) the operation has failed, in which case the future
   *   contains an exception (typically `bigtable::GrpcError`) with the details
   *   of the failure, or (c) the state of the operation is unknown after the
   *   time allocated by the retry policies has expired, in which case the
   *   future contains an exception of type `bigtable::PollTimeout`.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc update cluster
   */
  std::future<google::bigtable::admin::v2::Cluster> UpdateCluster(
      ClusterConfig cluster_config);

  /**
   * Deletes the specified cluster of an instance in the project.
   *
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   deleted
   *
   *  @par Example
   *  @snippet bigtable_samples_instance_admin.cc delete cluster
   */
  void DeleteCluster(bigtable::InstanceId const& instance_id,
                     bigtable::ClusterId const& cluster_id);

  /**
   * Gets the specified cluster of an instance in the project.
   *
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   deleted
   * @return a Cluster for given instance_id and cluster_id.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc get cluster
   */
  google::bigtable::admin::v2::Cluster GetCluster(
      bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id);

  /**
   * Create a new application profile.
   *
   * @param instance_id the instance for the new application profile.
   * @param config the configuration for the new application profile.
   * @return The proto describing the new application profile.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc create app profile
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc create app profile cluster
   */
  google::bigtable::admin::v2::AppProfile CreateAppProfile(
      bigtable::InstanceId const& instance_id, AppProfileConfig config);

  /**
   * Fetch the detailed information about an existing application profile.
   *
   * @param instance_id the instance to look the profile in.
   * @param profile_id the id of the profile within that instance.
   * @return The proto describing the application profile.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc get app profile
   */
  google::bigtable::admin::v2::AppProfile GetAppProfile(
      bigtable::InstanceId const& instance_id,
      bigtable::AppProfileId const& profile_id);

  /**
   * List the application profiles in an instance.
   *
   * @param instance_id the instance to list the profiles for.
   * @return a std::vector with the protos describing any profiles.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc list app profiles
   */
  std::vector<google::bigtable::admin::v2::AppProfile> ListAppProfiles(
      std::string const& instance_id);

  /**
   * Delete an existing application profile.
   *
   * @param instance_id the instance to look the profile in.
   * @param profile_id the id of the profile within that instance.
   * @param ignore_warnings if true, ignore safety checks when deleting the
   *     application profile.
   *
   * @par Example
   * @snippet bigtable_samples_instance_admin.cc delete app profile
   */
  void DeleteAppProfile(bigtable::InstanceId const& instance_id,
                        bigtable::AppProfileId const& profile_id,
                        bool ignore_warnings = false);

 private:
  /// Implement CreateInstance() with a separate thread.
  google::bigtable::admin::v2::Instance CreateInstanceImpl(
      InstanceConfig instance_config);

  /// Implement CreateCluster() with a separate thread.
  google::bigtable::admin::v2::Cluster CreateClusterImpl(
      ClusterConfig const& cluster_config,
      bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id);

  // Implement UpdateInstance() with a separate thread.
  google::bigtable::admin::v2::Instance UpdateInstanceImpl(
      InstanceUpdateConfig instance_update_config);

  // Implement UpdateCluster() with a separate thread.
  google::bigtable::admin::v2::Cluster UpdateClusterImpl(
      ClusterConfig cluster_config);

 private:
  noex::InstanceAdmin impl_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_H_
