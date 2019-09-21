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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CLIENT_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CLIENT_H_

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

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CLIENT_H_
