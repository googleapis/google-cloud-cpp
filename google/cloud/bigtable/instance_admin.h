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
#include "google/cloud/bigtable/cluster_list_responses.h"
#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/bigtable/instance_config.h"
#include "google/cloud/bigtable/instance_update_config.h"
#include "google/cloud/bigtable/internal/async_list_instances.h"
#include "google/cloud/bigtable/internal/instance_admin.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include <future>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Implements the APIs to administer Cloud Bigtable instances.
 *
 * @par Cost
 * Creating a new object of type `InstanceAdmin` is comparable to creating a few
 * objects of type `std::string` or a few objects of type
 * `std::shared_ptr<int>`. The class represents a shallow handle to a remote
 * object.
 */
class InstanceAdmin {
 public:
  /**
   * @param client the interface to create grpc stubs, report errors, etc.
   */
  explicit InstanceAdmin(std::shared_ptr<InstanceAdminClient> client)
      : impl_(std::move(client)) {}

  /**
   * Create a new InstanceAdmin using explicit policies to handle RPC errors.
   *
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param policies the set of policy overrides for this object.
   * @tparam Policies the types of the policies to override, the types must
   *     derive from one of the following types:
   *     - `RPCBackoffPolicy` how to backoff from a failed RPC. Currently only
   *       `ExponentialBackoffPolicy` is implemented. You can also create your
   *       own policies that backoff using a different algorithm.
   *     - `RPCRetryPolicy` for how long to retry failed RPCs. Use
   *       `LimitedErrorCountRetryPolicy` to limit the number of failures
   *       allowed. Use `LimitedTimeRetryPolicy` to bound the time for any
   *       request. You can also create your own policies that combine time and
   *       error counts.
   *     - `PollingPolicy` for how long will the class wait for
   *       `google.longrunning.Operation` to complete. This class combines both
   *       the backoff policy for checking long running operations and the
   *       retry policy.
   *
   * @see GenericPollingPolicy, ExponentialBackoffPolicy,
   *     LimitedErrorCountRetryPolicy, LimitedTimeRetryPolicy.
   */
  template <typename... Policies>
  explicit InstanceAdmin(std::shared_ptr<InstanceAdminClient> client,
                         Policies&&... policies)
      : impl_(std::move(client), std::forward<Policies>(policies)...) {}

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
   *   instance_id and a display_name parameters must be set in instance_config,
   *   - instance_id : must be between 6 and 33 characters.
   *   - display_name : must be between 4 and 30 characters.
   * @return a future that becomes satisfied when (a) the operation has
   *   completed successfully, in which case it returns a proto with the
   *   Instance details, (b) the operation has failed, in which case the future
   *   contains an exception (typically `bigtable::GrpcError`) with the details
   *   of the failure, or (c) the state of the operation is unknown after the
   *   time allocated by the retry policies has expired, in which case the
   *   future contains an exception of type `bigtable::PollTimeout`.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc create instance
   */
  std::future<StatusOr<google::bigtable::admin::v2::Instance>> CreateInstance(
      InstanceConfig instance_config);

  /**
   * Create a new Cluster of Cloud Bigtable.
   *
   * @param cluster_config a description of the new cluster to be created.
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   created. It must be between 6 and 30 characters.
   *
   *  @par Example
   *  @snippet bigtable_instance_admin_snippets.cc create cluster
   */
  std::future<StatusOr<google::bigtable::admin::v2::Cluster>> CreateCluster(
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
   * @snippet bigtable_instance_admin_snippets.cc update instance
   */
  std::future<StatusOr<google::bigtable::admin::v2::Instance>> UpdateInstance(
      InstanceUpdateConfig instance_update_config);

  /**
   * Obtain the list of instances in the project.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of instances, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list instances
   */
  StatusOr<InstanceList> ListInstances();

  /**
   * Query (asynchronously) the list of instances in the project.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of instances, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @return the list of instances. It is possible that some zones are not
   * currently available for querying. In that case this function returns the
   * list of failed locations in the `projects/<project>/locations/<zone_id>`
   * format.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async list instances
   */
  future<InstanceList> AsyncListInstances(CompletionQueue& cq);

  /**
   * Return the details of @p instance_id.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc get instance
   */
  StatusOr<google::bigtable::admin::v2::Instance> GetInstance(
      std::string const& instance_id);

  /**
   * Sends an asynchronous request to get information about an existing
   * instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the id of the instance in the project that to be
   *     retrieved.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second the future is satisfied with
   *   an exception.
   *
   * @throws std::exception if the operation cannot be started.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async get instance
   */
  future<google::bigtable::admin::v2::Instance> AsyncGetInstance(
      CompletionQueue& cq, std::string const& instance_id);

  /**
   * Deletes the instances in the project.
   * @param instance_id the id of the instance in the project that needs to be
   * deleted
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc delete instance
   */
  Status DeleteInstance(std::string const& instance_id);

  /**
   * Obtain the list of clusters in an instance.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of clusters, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list clusters
   */
  StatusOr<ClusterList> ListClusters();

  /**
   * Obtain the list of clusters in an instance.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of clusters, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list clusters
   */
  StatusOr<ClusterList> ListClusters(std::string const& instance_id);

  /**
   * Query (asynchronously) the list of clusters in a project.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of clusters, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @return the list of clusters. It is possible that some zones are not
   *     currently available for querying. In that case this function returns
   *     the list of failed locations in the
   *     `projects/<project>/locations/<zone_id>` format.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list clusters
   */
  future<ClusterList> AsyncListClusters(CompletionQueue& cq);

  /**
   * Query (asynchronously) the list of clusters in an instance.
   *
   * @note In some circumstances Cloud Bigtable may be unable to obtain the full
   *   list of clusters, typically because some transient failure has made
   *   specific zones unavailable. In this cases the service returns a separate
   *   list of `failed_locations` that represent the unavailable zones.
   *   Applications may want to retry the operation after the transient
   *   conditions have cleared.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param instance_id the instance in a project.
   * @return the list of clusters. It is possible that some zones are not
   *     currently available for querying. In that case this function returns
   *     the list of failed locations in the
   *     `projects/<project>/locations/<zone_id>` format.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list clusters
   */
  future<ClusterList> AsyncListClusters(CompletionQueue& cq,
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
   * @snippet bigtable_instance_admin_snippets.cc update cluster
   */
  std::future<StatusOr<google::bigtable::admin::v2::Cluster>> UpdateCluster(
      ClusterConfig cluster_config);

  /**
   * Deletes the specified cluster of an instance in the project.
   *
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   deleted
   *
   *  @par Example
   *  @snippet bigtable_instance_admin_snippets.cc delete cluster
   */
  Status DeleteCluster(bigtable::InstanceId const& instance_id,
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
   * @snippet bigtable_instance_admin_snippets.cc get cluster
   */
  StatusOr<google::bigtable::admin::v2::Cluster> GetCluster(
      bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id);

  /**
   * Sends an asynchronous request to get information about existing cluster of
   * an instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param instance_id the id of the instance in the project.
   * @param cluster_id the id of the cluster in the project that needs to be
   * retrieved.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second the future is satisfied with
   *   an exception.
   *
   * @throws std::exception if the operation cannot be started.
   *
   * @par Example
   * @snippet instance_admin_async_snippets.cc async get cluster
   */
  future<google::bigtable::admin::v2::Cluster> AsyncGetCluster(
      CompletionQueue& cq, bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id);

  /**
   * Create a new application profile.
   *
   * @param instance_id the instance for the new application profile.
   * @param config the configuration for the new application profile.
   * @return The proto describing the new application profile.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc create app profile
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc create app profile cluster
   */
  StatusOr<google::bigtable::admin::v2::AppProfile> CreateAppProfile(
      bigtable::InstanceId const& instance_id, AppProfileConfig config);

  /**
   * Fetch the detailed information about an existing application profile.
   *
   * @param instance_id the instance to look the profile in.
   * @param profile_id the id of the profile within that instance.
   * @return The proto describing the application profile.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc get app profile
   */
  StatusOr<google::bigtable::admin::v2::AppProfile> GetAppProfile(
      bigtable::InstanceId const& instance_id,
      bigtable::AppProfileId const& profile_id);

  /**
   * Create a new application profile.
   *
   * @param instance_id the instance for the new application profile.
   * @param profile_id the id (not the full name) of the profile to update.
   * @param config the configuration for the new application profile.
   * @return The proto describing the new application profile.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc update app profile description
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc update app profile routing any
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc update app profile routing
   */
  std::future<StatusOr<google::bigtable::admin::v2::AppProfile>>
  UpdateAppProfile(bigtable::InstanceId instance_id,
                   bigtable::AppProfileId profile_id,
                   AppProfileUpdateConfig config);

  /**
   * List the application profiles in an instance.
   *
   * @param instance_id the instance to list the profiles for.
   * @return a std::vector with the protos describing any profiles.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list app profiles
   */
  StatusOr<std::vector<google::bigtable::admin::v2::AppProfile>>
  ListAppProfiles(std::string const& instance_id);

  /**
   * Delete an existing application profile.
   *
   * @param instance_id the instance to look the profile in.
   * @param profile_id the id of the profile within that instance.
   * @param ignore_warnings if true, ignore safety checks when deleting the
   *     application profile.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc delete app profile
   */
  Status DeleteAppProfile(bigtable::InstanceId const& instance_id,
                          bigtable::AppProfileId const& profile_id,
                          bool ignore_warnings = false);

  /**
   * Gets the policy for @p instance_id.
   *
   * @param instance_id the instance to query.
   * @return Policy the full IAM policy for the instance.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc get iam policy
   */
  StatusOr<google::cloud::IamPolicy> GetIamPolicy(
      std::string const& instance_id);

  /**
   * Sets the IAM policy for an instance.
   *
   * Applications can provide the @p etag to implement optimistic concurrency
   * control. If @p etag is not empty, the server will reject calls where the
   * provided ETag does not match the ETag value stored in the server.
   *
   * @param instance_id which instance to set the IAM policy for.
   * @param iam_bindings IamBindings object containing role and members.
   * @param etag the expected ETag value for the current policy.
   * @return Policy the current IAM bindings for the instance.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc set iam policy
   */
  StatusOr<google::cloud::IamPolicy> SetIamPolicy(
      std::string const& instance_id,
      google::cloud::IamBindings const& iam_bindings,
      std::string const& etag = std::string{});

  /**
   * Returns a permission set that the caller has on the specified instance.
   *
   * @param instance_id the ID of the instance to query.
   * @param permissions set of permissions to check for the resource.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc test iam permissions
   *
   * @see https://cloud.google.com/bigtable/docs/access-control for a list of
   *     valid permissions on Google Cloud Bigtable.
   */
  StatusOr<std::vector<std::string>> TestIamPermissions(
      std::string const& instance_id,
      std::vector<std::string> const& permissions);

 private:
  /// Implement CreateInstance() with a separate thread.
  StatusOr<google::bigtable::admin::v2::Instance> CreateInstanceImpl(
      InstanceConfig instance_config);

  /// Implement CreateCluster() with a separate thread.
  StatusOr<google::bigtable::admin::v2::Cluster> CreateClusterImpl(
      ClusterConfig const& cluster_config,
      bigtable::InstanceId const& instance_id,
      bigtable::ClusterId const& cluster_id);

  // Implement UpdateInstance() with a separate thread.
  StatusOr<google::bigtable::admin::v2::Instance> UpdateInstanceImpl(
      InstanceUpdateConfig instance_update_config);

  // Implement UpdateCluster() with a separate thread.
  StatusOr<google::bigtable::admin::v2::Cluster> UpdateClusterImpl(
      ClusterConfig cluster_config);

  /// Poll the result of UpdateAppProfile in a separate thread.
  StatusOr<google::bigtable::admin::v2::AppProfile> UpdateAppProfileImpl(
      bigtable::InstanceId instance_id, bigtable::AppProfileId profile_id,
      AppProfileUpdateConfig config);

 private:
  noex::InstanceAdmin impl_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_H_
