// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_H

#include "google/cloud/bigtable/admin/bigtable_instance_admin_connection.h"
#include "google/cloud/bigtable/admin/bigtable_instance_admin_options.h"
#include "google/cloud/bigtable/app_profile_config.h"
#include "google/cloud/bigtable/cluster_config.h"
#include "google/cloud/bigtable/cluster_list_responses.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/iam_policy.h"
#include "google/cloud/bigtable/instance_admin_client.h"
#include "google/cloud/bigtable/instance_config.h"
#include "google/cloud/bigtable/instance_list_responses.h"
#include "google/cloud/bigtable/instance_update_config.h"
#include "google/cloud/bigtable/internal/convert_policies.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future.h"
#include "google/cloud/project.h"
#include "google/cloud/status_or.h"
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class InstanceAdminTester;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Implements the APIs to administer Cloud Bigtable instances.
 *
 * @par Thread-safety
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating concurrently on the same
 * instance of this class is not guaranteed to work.
 *
 * @par Cost
 * Creating a new object of type `InstanceAdmin` is comparable to creating a few
 * objects of type `std::string` or a few objects of type
 * `std::shared_ptr<int>`. The class represents a shallow handle to a remote
 * object.
 *
 * @par Error Handling
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Operations that do not return a value simply
 * return a `google::cloud::Status` indicating success or the details of the
 * error Please consult the [`StatusOr<T>`
 * documentation](#google::cloud::StatusOr) for more details.
 *
 * @code
 * namespace cbt = google::cloud::bigtable;
 * namespace btadmin = google::bigtable::admin::v2;
 * cbt::InstanceAdmin admin = ...;
 * google::cloud::StatusOr<btadmin::Instance> instance = admin.GetInstance(...);
 *
 * if (!instance) {
 *   std::cerr << "Error fetching instance\n";
 *   return;
 * }
 *
 * // Use `instance` as a smart pointer here, e.g.:
 * std::cout << "The full instance name is " << instance->name() << "\n";
 * @endcode
 *
 * In addition, the @ref index "main page" contains examples using `StatusOr<T>`
 * to handle errors.
 *
 * @par Retry, Backoff, and Idempotency Policies
 * The library automatically retries requests that fail with transient errors,
 * and uses [truncated exponential backoff][backoff-link] to backoff between
 * retries. The default policies are to continue retrying for up to 10 minutes.
 * On each transient failure the backoff period is doubled, starting with an
 * initial backoff of 100 milliseconds. The backoff period growth is truncated
 * at 60 seconds. The default idempotency policy is to only retry idempotent
 * operations. Note that most operations that change state are **not**
 * idempotent.
 *
 * The application can override these policies when constructing objects of this
 * class. The documentation for the constructors show examples of this in
 * action.
 *
 * [backoff-link]: https://cloud.google.com/storage/docs/exponential-backoff
 *
 * @see https://cloud.google.com/bigtable/ for an overview of Cloud Bigtable.
 *
 * @see https://cloud.google.com/bigtable/docs/overview for an overview of the
 *     Cloud Bigtable data model.
 *
 * @see https://cloud.google.com/bigtable/docs/instances-clusters-nodes for an
 *     introduction of the main APIs into Cloud Bigtable.
 *
 * @see https://cloud.google.com/bigtable/docs/reference/service-apis-overview
 *     for an overview of the underlying Cloud Bigtable API.
 *
 * @see #google::cloud::StatusOr for a description of the error reporting class
 *     used by this library.
 *
 * @see `LimitedTimeRetryPolicy` and `LimitedErrorCountRetryPolicy` for
 *     alternative retry policies.
 *
 * @see `ExponentialBackoffPolicy` to configure different parameters for the
 *     exponential backoff policy.
 *
 * @see `SafeIdempotentMutationPolicy` and `AlwaysRetryMutationPolicy` for
 *     alternative idempotency policies.
 */
class InstanceAdmin {
 public:
  explicit InstanceAdmin(
      std::shared_ptr<bigtable_admin::BigtableInstanceAdminConnection>
          connection,
      std::string project)
      : connection_(std::move(connection)),
        project_id_(std::move(project)),
        project_name_(Project(project_id_).FullName()),
        retry_prototype_(
            DefaultRPCRetryPolicy(internal::kBigtableInstanceAdminLimits)),
        backoff_prototype_(
            DefaultRPCBackoffPolicy(internal::kBigtableInstanceAdminLimits)),
        polling_prototype_(
            DefaultPollingPolicy(internal::kBigtableInstanceAdminLimits)),
        options_(google::cloud::internal::MergeOptions(
            bigtable_internal::MakeInstanceAdminOptions(
                retry_prototype_, backoff_prototype_, polling_prototype_),
            connection_->options())) {}

  /**
   * @param client the interface to create grpc stubs, report errors, etc.
   */
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  explicit InstanceAdmin(std::shared_ptr<InstanceAdminClient> client)
      : InstanceAdmin(client->connection_, client->project()) {}

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
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
  explicit InstanceAdmin(std::shared_ptr<InstanceAdminClient> client,
                         Policies&&... policies)
      : connection_(client->connection_),
        project_id_(client->project()),
        project_name_(Project(project_id_).FullName()),
        retry_prototype_(
            DefaultRPCRetryPolicy(internal::kBigtableInstanceAdminLimits)),
        backoff_prototype_(
            DefaultRPCBackoffPolicy(internal::kBigtableInstanceAdminLimits)),
        polling_prototype_(
            DefaultPollingPolicy(internal::kBigtableInstanceAdminLimits)) {
    ChangePolicies(std::forward<Policies>(policies)...);
    options_ = google::cloud::internal::MergeOptions(
        bigtable_internal::MakeInstanceAdminOptions(
            retry_prototype_, backoff_prototype_, polling_prototype_),
        connection_->options());
  }

  /// The full name (`projects/<project_id>`) of the project.
  std::string const& project_name() const { return project_name_; }
  /// The project id, i.e., `project_name()` without the `projects/` prefix.
  std::string const& project_id() const { return project_id_; }

  /**
   * Returns an InstanceAdmin that reuses the connection and configuration of
   * this InstanceAdmin, but with a different resource name.
   */
  InstanceAdmin WithNewTarget(std::string project_id) const {
    auto admin = *this;
    admin.project_id_ = std::move(project_id);
    admin.project_name_ = Project(admin.project_id_).FullName();
    return admin;
  }

  /// Return the fully qualified name of the given instance_id.
  std::string InstanceName(std::string const& instance_id) const {
    return google::cloud::bigtable::InstanceName(project_id_, instance_id);
  }

  /// Return the fully qualified name of the given cluster_id in give
  /// instance_id.
  std::string ClusterName(std::string const& instance_id,
                          std::string const& cluster_id) const {
    return google::cloud::bigtable::ClusterName(project_id_, instance_id,
                                                cluster_id);
  }

  std::string AppProfileName(std::string const& instance_id,
                             std::string const& profile_id) const {
    return google::cloud::bigtable::AppProfileName(project_id_, instance_id,
                                                   profile_id);
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
   *   contains an `google::cloud::Status` with the details of the failure, or
   *   (c) the state of the operation is unknown after the time allocated by the
   *   retry policies has expired, in which case the future contains the last
   *   error status.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc create instance
   */
  future<StatusOr<google::bigtable::admin::v2::Instance>> CreateInstance(
      InstanceConfig instance_config);

  /**
   * Create a new Cluster of Cloud Bigtable.
   *
   * @param cluster_config a description of the new cluster to be created.
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   created. It must be between 6 and 30 characters.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc create cluster
   */
  future<StatusOr<google::bigtable::admin::v2::Cluster>> CreateCluster(
      ClusterConfig cluster_config, std::string const& instance_id,
      std::string const& cluster_id);

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
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc update instance
   */
  future<StatusOr<google::bigtable::admin::v2::Instance>> UpdateInstance(
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
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list instances
   */
  StatusOr<InstanceList> ListInstances();

  /**
   * Return the details of @p instance_id.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc get instance
   */
  StatusOr<google::bigtable::admin::v2::Instance> GetInstance(
      std::string const& instance_id);

  /**
   * Deletes the instances in the project.
   *
   * @param instance_id the id of the instance in the project that needs to be
   * deleted
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
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
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
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
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc list clusters
   */
  StatusOr<ClusterList> ListClusters(std::string const& instance_id);

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
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc update cluster
   */
  future<StatusOr<google::bigtable::admin::v2::Cluster>> UpdateCluster(
      ClusterConfig cluster_config);

  /**
   * Deletes the specified cluster of an instance in the project.
   *
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   deleted
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc delete cluster
   */
  Status DeleteCluster(std::string const& instance_id,
                       std::string const& cluster_id);

  /**
   * Gets the specified cluster of an instance in the project.
   *
   * @param instance_id the id of the instance in the project
   * @param cluster_id the id of the cluster in the project that needs to be
   *   deleted
   * @return a Cluster for given instance_id and cluster_id.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc get cluster
   */
  StatusOr<google::bigtable::admin::v2::Cluster> GetCluster(
      std::string const& instance_id, std::string const& cluster_id);

  /**
   * Create a new application profile.
   *
   * @param instance_id the instance for the new application profile.
   * @param config the configuration for the new application profile.
   * @return The proto describing the new application profile.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Multi-cluster Routing Example
   * @snippet bigtable_instance_admin_snippets.cc create app profile
   *
   * @par Single Cluster Routing Example
   * @snippet bigtable_instance_admin_snippets.cc create app profile cluster
   */
  StatusOr<google::bigtable::admin::v2::AppProfile> CreateAppProfile(
      std::string const& instance_id, AppProfileConfig config);

  /**
   * Fetch the detailed information about an existing application profile.
   *
   * @param instance_id the instance to look the profile in.
   * @param profile_id the id of the profile within that instance.
   * @return The proto describing the application profile.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc get app profile
   */
  StatusOr<google::bigtable::admin::v2::AppProfile> GetAppProfile(
      std::string const& instance_id, std::string const& profile_id);

  /**
   * Updates an existing application profile.
   *
   * @param instance_id the instance for the new application profile.
   * @param profile_id the id (not the full name) of the profile to update.
   * @param config the configuration for the new application profile.
   * @return The proto describing the new application profile.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Change Description Example
   * @snippet bigtable_instance_admin_snippets.cc update app profile description
   *
   * @par Change Routing to Any Cluster Example
   * @snippet bigtable_instance_admin_snippets.cc update app profile routing any
   *
   * @par Change Routing to a Specific Cluster Example
   * @snippet bigtable_instance_admin_snippets.cc update app profile routing
   */
  future<StatusOr<google::bigtable::admin::v2::AppProfile>> UpdateAppProfile(
      std::string const& instance_id, std::string const& profile_id,
      AppProfileUpdateConfig config);

  /**
   * List the application profiles in an instance.
   *
   * @param instance_id the instance to list the profiles for.
   * @return a std::vector with the protos describing any profiles.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
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
   *     application profile. This value is to to `true` by default. Passing
   *     `false` causes this function to fail even when no operations are
   *     pending.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc delete app profile
   */
  Status DeleteAppProfile(std::string const& instance_id,
                          std::string const& profile_id,
                          bool ignore_warnings = true);

  /**
   * Gets the native policy for @p instance_id.
   *
   * @param instance_id the instance to query.
   * @return google::iam::v1::Policy the full IAM policy for the instance.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc get native iam policy
   */
  StatusOr<google::iam::v1::Policy> GetNativeIamPolicy(
      std::string const& instance_id);

  /**
   * Sets the IAM policy for an instance.
   *
   * @param instance_id which instance to set the IAM policy for.
   * @param iam_policy google::iam::v1::Policy object containing role and
   * members.
   * @return google::iam::v1::Policy the current IAM policy for the instance.
   *
   * @warning ETags are currently not used by Cloud Bigtable.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_instance_admin_snippets.cc set native iam policy
   */
  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      std::string const& instance_id,
      google::iam::v1::Policy const& iam_policy);

  /**
   * Returns a permission set that the caller has on the specified instance.
   *
   * @param instance_id the ID of the instance to query.
   * @param permissions set of permissions to check for the resource.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
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
  friend class bigtable_internal::InstanceAdminTester;

  ///@{
  /// @name Helper functions to implement constructors with changed policies.
  void ChangePolicy(RPCRetryPolicy const& policy) {
    retry_prototype_ = policy.clone();
  }

  void ChangePolicy(RPCBackoffPolicy const& policy) {
    backoff_prototype_ = policy.clone();
  }

  void ChangePolicy(PollingPolicy const& policy) {
    polling_prototype_ = policy.clone();
  }

  template <typename Policy, typename... Policies>
  void ChangePolicies(Policy&& policy, Policies&&... policies) {
    ChangePolicy(policy);
    ChangePolicies(std::forward<Policies>(policies)...);
  }
  void ChangePolicies() {}
  ///@}

  std::shared_ptr<bigtable_admin::BigtableInstanceAdminConnection> connection_;
  std::string project_id_;
  std::string project_name_;
  ///@{
  /// These prototypes are only used as temporary storage during construction of
  /// the class, where they are consolidated as common policies in `options_`.
  std::shared_ptr<RPCRetryPolicy> retry_prototype_;
  std::shared_ptr<RPCBackoffPolicy> backoff_prototype_;
  std::shared_ptr<PollingPolicy> polling_prototype_;
  ///}
  Options options_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INSTANCE_ADMIN_H
