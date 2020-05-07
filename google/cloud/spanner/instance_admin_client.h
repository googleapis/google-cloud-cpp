// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CLIENT_H

#include "google/cloud/spanner/iam_updater.h"
#include "google/cloud/spanner/instance.h"
#include "google/cloud/spanner/instance_admin_connection.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Performs instance administration operations on Cloud Spanner.
 *
 * Applications use this class to perform operations on
 * [Spanner Databases][spanner-doc-link].
 *
 * @par Performance
 *
 * `InstanceAdminClient` objects are cheap to create, copy, and move. However,
 * each `InstanceAdminClient` object must be created with a
 * `std::shared_ptr<InstanceAdminConnection>`, which itself is relatively
 * expensive to create. Therefore, connection instances should be shared when
 * possible. See the `MakeInstanceAdminConnection()` method and the
 * `InstanceAdminConnection` interface for more details.
 *
 * @par Thread Safety
 *
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Error Handling
 *
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Please consult the
 * [`StatusOr<T>` documentation](#google::cloud::v0::StatusOr) for more details.
 *
 * [spanner-doc-link]:
 * https://cloud.google.com/spanner/docs/api-libraries-overview
 */
class InstanceAdminClient {
 public:
  explicit InstanceAdminClient(std::shared_ptr<InstanceAdminConnection> conn)
      : conn_(std::move(conn)) {}

  /// No default construction.
  /// Use `InstanceAdminClient(std::shared_ptr<InstanceAdminConnection>)`
  InstanceAdminClient() = delete;

  //@{
  // @name Copy and move support
  InstanceAdminClient(InstanceAdminClient const&) = default;
  InstanceAdminClient& operator=(InstanceAdminClient const&) = default;
  InstanceAdminClient(InstanceAdminClient&&) = default;
  InstanceAdminClient& operator=(InstanceAdminClient&&) = default;
  //@}

  //@{
  // @name Equality
  friend bool operator==(InstanceAdminClient const& a,
                         InstanceAdminClient const& b) {
    return a.conn_ == b.conn_;
  }
  friend bool operator!=(InstanceAdminClient const& a,
                         InstanceAdminClient const& b) {
    return !(a == b);
  }
  //@}

  /**
   * Retrieve metadata information about a Cloud Spanner Instance.
   *
   * @par Idempotency
   * This is a read-only operation and therefore it is always treated as
   * idempotent.
   *
   * @par Example
   * @snippet samples.cc get-instance
   */
  StatusOr<google::spanner::admin::instance::v1::Instance> GetInstance(
      Instance const& in);

  /**
   * Creates a new Cloud Spanner instance in the given project.
   *
   * Use CreateInstanceRequestBuilder to build the
   * `google::spanner::admin::instance::v1::CreateInstanceRequest` object.
   *
   * Note that the instance id must be between 2 and 64 characters long, it must
   * start with a lowercase letter (`[a-z]`), it must end with a lowercase
   * letter or a number (`[a-z0-9]`) and any characters between the beginning
   * and ending characters must be lower case letters, numbers, or dashes (`-`),
   * that is, they must belong to the `[-a-z0-9]` character set.
   *
   * @par Example
   * @snippet samples.cc create-instance
   *
   */
  future<StatusOr<google::spanner::admin::instance::v1::Instance>>
  CreateInstance(
      google::spanner::admin::instance::v1::CreateInstanceRequest const&);

  /**
   * Updates a Cloud Spanner instance.
   *
   * Use `google::cloud::spanner::UpdateInstanceRequestBuilder` to build the
   * `google::spanner::admin::instance::v1::UpdateInstanceRequest` object.
   *
   * @par Idempotency
   * This operation is idempotent as its result does not depend on the previous
   * state of the instance. Note that, as is the case with all operations, it is
   * subject to race conditions if multiple tasks are attempting to change the
   * same fields in the same instance.
   *
   * @par Example
   * @snippet samples.cc update-instance
   *
   */
  future<StatusOr<google::spanner::admin::instance::v1::Instance>>
  UpdateInstance(
      google::spanner::admin::instance::v1::UpdateInstanceRequest const&);

  /**
   * Deletes an existing Cloud Spanner instance.
   *
   * @warning Deleting an instance deletes all the databases in the
   * instance. This is an unrecoverable operation.
   *
   * @par Example
   * @snippet samples.cc delete-instance
   */
  Status DeleteInstance(Instance const& in);

  /**
   * Retrieve information about a Cloud Spanner Instance Config.
   *
   * @par Idempotency
   * This is a read-only operation and therefore it is always treated as
   * idempotent.
   *
   * @par Example
   * @snippet samples.cc get-instance-config
   */
  StatusOr<google::spanner::admin::instance::v1::InstanceConfig>
  GetInstanceConfig(std::string const& name);

  /**
   * Retrieve a list of instance configs for a given project.
   *
   * @par Idempotency
   * This is a read-only operation and therefore it is always treated as
   * idempotent.
   *
   * @par Example
   * @snippet samples.cc list-instance-configs
   */
  ListInstanceConfigsRange ListInstanceConfigs(std::string project_id);

  /**
   * Retrieve a list of instances for a given project.
   *
   * @par Idempotency
   * This is a read-only operation and therefore it is always treated as
   * idempotent.
   *
   * @par Example
   * @snippet samples.cc list-instances
   */
  ListInstancesRange ListInstances(std::string project_id, std::string filter);

  /**
   * Get the IAM policy in effect for the given instance.
   *
   * This function retrieves the IAM policy configured in the given instance,
   * that is, which roles are enabled in the instance, and what entities are
   * members of each role.
   *
   * @par Idempotency
   * This is a read-only operation and therefore it is always treated as
   * idempotent.
   *
   * @par Example
   * @snippet samples.cc instance-get-iam-policy
   *
   * @see The [Cloud Spanner
   *     documentation](https://cloud.google.com/spanner/docs/iam) for a
   *     description of the roles and permissions supported by Cloud Spanner.
   * @see [IAM Overview](https://cloud.google.com/iam/docs/overview#permissions)
   *     for an introduction to Identity and Access Management in Google Cloud
   *     Platform.
   */
  StatusOr<google::iam::v1::Policy> GetIamPolicy(Instance const& in);

  /**
   * Set the IAM policy for the given instance.
   *
   * This function changes the IAM policy configured in the given instance to
   * the value of @p policy.
   *
   * @par Idempotency
   * This function is only idempotent if the `etag` field in @p policy is set.
   * Therefore, the underlying RPCs are only retried if the field is set, and
   * the function returns the first RPC error in any other case.
   *
   * @par Example
   * @snippet samples.cc add-database-reader
   *
   * @see The [Cloud Spanner
   *     documentation](https://cloud.google.com/spanner/docs/iam) for a
   *     description of the roles and permissions supported by Cloud Spanner.
   * @see [IAM Overview](https://cloud.google.com/iam/docs/overview#permissions)
   *     for an introduction to Identity and Access Management in Google Cloud
   *     Platform.
   */
  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      Instance const& in, google::iam::v1::Policy policy);

  /**
   * Updates the IAM policy for an instance using an optimistic concurrency
   * control loop.
   *
   * This function repeatedly reads the current IAM policy in @p in, and then
   * calls the @p updater with the this policy. The @p updater returns an empty
   * optional if no changes are required, or it returns the new desired value
   * for the IAM policy. This function then updates the policy.
   *
   * Updating an IAM policy can fail with retryable errors or can be aborted
   * because there were simultaneous changes the to IAM policy. In these cases
   * this function reruns the loop until it succeeds.
   *
   * The function returns the final IAM policy, or an error if the rerun policy
   * for the underlying connection has expired.
   *
   * @par Idempotency
   * This function always sets the `etag` field on the policy, so the underlying
   * RPCs are retried automatically.
   *
   * @param in the identifier for the instance where you want to change the IAM
   *     policy.
   * @param updater a callback to modify the policy.  Return an unset optional
   *     to indicate that no changes to the policy are needed.
   */
  StatusOr<google::iam::v1::Policy> SetIamPolicy(Instance const& in,
                                                 IamUpdater const& updater);

  /**
   * @copydoc SetIamPolicy(Instance const&,IamUpdater const&)
   *
   * @param rerun_policy controls for how long (or how many times) the updater
   *     will be rerun after the IAM policy update aborts.
   * @param backoff_policy controls how long `SetIamPolicy` waits between
   *     reruns.
   */
  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      Instance const& in, IamUpdater const& updater,
      std::unique_ptr<TransactionRerunPolicy> rerun_policy,
      std::unique_ptr<BackoffPolicy> backoff_policy);

  /**
   * Get the subset of the permissions the caller has on the given instance.
   *
   * This function compares the given list of permissions against those
   * permissions granted to the caller, and returns the subset of the list that
   * the caller actually holds.
   *
   * @note Permission wildcards, such as `spanner.*` are not allowed.
   *
   * @par Example
   * @snippet samples.cc instance-test-iam-permissions
   *
   * @see The [Cloud Spanner
   * documentation](https://cloud.google.com/spanner/docs/iam) for a description
   * of the roles and permissions supported by Cloud Spanner.
   * @see [IAM Overview](https://cloud.google.com/iam/docs/overview#permissions)
   *     for an introduction to Identity and Access Management in Google Cloud
   *     Platform.
   */
  StatusOr<google::iam::v1::TestIamPermissionsResponse> TestIamPermissions(
      Instance const& in, std::vector<std::string> permissions);

 private:
  std::shared_ptr<InstanceAdminConnection> conn_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CLIENT_H
