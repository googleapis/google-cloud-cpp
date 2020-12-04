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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CONNECTION_H

#include "google/cloud/spanner/internal/instance_admin_stub.h"
#include "google/cloud/spanner/polling_policy.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/backoff_policy.h"
#include "google/cloud/internal/pagination_range.h"
#include <google/spanner/admin/instance/v1/spanner_instance_admin.grpc.pb.h>
#include <map>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * An input range to stream all the instances in a Cloud project.
 *
 * This type models an [input range][cppref-input-range] of
 * `google::spanner::admin::v1::Instance` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListInstancesRange = google::cloud::internal::PaginationRange<
    google::spanner::admin::instance::v1::Instance>;

/**
 * An input range to stream all the instance configs in a Cloud project.
 *
 * This type models an [input range][cppref-input-range] of
 * `google::spanner::admin::v1::Instance` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListInstanceConfigsRange = google::cloud::internal::PaginationRange<
    google::spanner::admin::instance::v1::InstanceConfig>;

/**
 * A connection to the Cloud Spanner instance administration service.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `InstanceAdminClient`.  This allows users to inject custom
 * behavior (e.g., with a Google Mock object) in a `InstanceAdminClient` object
 * for use in their own tests.
 *
 * To create a concrete instance that connects you to a real Cloud Spanner
 * instance administration service, see `MakeInstanceAdminConnection()`.
 */
class InstanceAdminConnection {
 public:
  virtual ~InstanceAdminConnection() = 0;

  //@{
  /**
   * @name Define the arguments for each member function.
   *
   * Applications may define classes derived from `InstanceAdminConnection`,
   * for example, because they want to mock the class. To avoid breaking all
   * such derived classes when we change the number or type of the arguments
   * to the member functions we define light weight structures to pass the
   * arguments.
   */
  /// Wrap the arguments for `GetInstance()`.
  struct GetInstanceParams {
    /// The full name of the instance in
    /// `projects/<project-id>/instances/<instance-id>` format.
    std::string instance_name;
  };

  /// Wrap the arguments for `CreateInstance()`.
  struct CreateInstanceParams {
    google::spanner::admin::instance::v1::CreateInstanceRequest request;
  };

  /// Wrap the arguments for `UpdateInstance()`.
  struct UpdateInstanceParams {
    google::spanner::admin::instance::v1::UpdateInstanceRequest request;
  };

  /// Wrap the arguments for `DeleteInstance()`.
  struct DeleteInstanceParams {
    std::string instance_name;
  };

  /// Wrap the arguments for `GetInstanceConfig()`.
  struct GetInstanceConfigParams {
    std::string instance_config_name;
  };

  /// Wrap the arguments for `ListInstanceConfigs()`.
  struct ListInstanceConfigsParams {
    std::string project_id;
  };

  /**
   * Wrap the arguments for `ListInstances()`.
   */
  struct ListInstancesParams {
    /**
     * Query the instances in this project.
     *
     * This is a required value, it must be non-empty.
     */
    std::string project_id;

    /**
     * A filtering expression to restrict the set of instances included in the
     * response.
     *
     * @see The [RPC reference documentation][1] for the format of the filtering
     *     expression.
     *
     * [1]:
     * https://cloud.google.com/spanner/docs/reference/rpc/google.spanner.admin.instance.v1#google.spanner.admin.instance.v1.ListInstancesRequest
     */
    std::string filter;
  };

  /// Wrap the arguments for `GetIamPolicy()`.
  struct GetIamPolicyParams {
    std::string instance_name;
  };

  /// Wrap the arguments for `SetIamPolicy()`.
  struct SetIamPolicyParams {
    std::string instance_name;
    google::iam::v1::Policy policy;
  };

  /// Wrap the arguments for `TestIamPermissions()`.
  struct TestIamPermissionsParams {
    std::string instance_name;
    std::vector<std::string> permissions;
  };
  //@}

  /// Return the metadata for the given instance.
  virtual StatusOr<google::spanner::admin::instance::v1::Instance> GetInstance(
      GetInstanceParams) = 0;

  virtual future<StatusOr<google::spanner::admin::instance::v1::Instance>>
  CreateInstance(CreateInstanceParams p) = 0;

  virtual future<StatusOr<google::spanner::admin::instance::v1::Instance>>
  UpdateInstance(UpdateInstanceParams p) = 0;

  virtual Status DeleteInstance(DeleteInstanceParams p) = 0;

  /// Return the InstanceConfig with the given name.
  virtual StatusOr<google::spanner::admin::instance::v1::InstanceConfig>
      GetInstanceConfig(GetInstanceConfigParams) = 0;

  /**
   * Returns a one-pass input range with all the instance configs.
   */
  virtual ListInstanceConfigsRange ListInstanceConfigs(
      ListInstanceConfigsParams) = 0;

  /**
   * Returns a one-pass input range with all the instances meeting the
   * requirements in @p params
   */
  virtual ListInstancesRange ListInstances(ListInstancesParams params) = 0;

  /// Define the interface for a
  /// google.spanner.v1.DatabaseAdmin.GetIamPolicy RPC.
  virtual StatusOr<google::iam::v1::Policy> GetIamPolicy(
      GetIamPolicyParams) = 0;

  /// Define the interface for a
  /// google.spanner.v1.DatabaseAdmin.SetIamPolicy RPC.
  virtual StatusOr<google::iam::v1::Policy> SetIamPolicy(
      SetIamPolicyParams) = 0;

  /// Define the interface for a
  /// google.spanner.v1.DatabaseAdmin.TestIamPermissions RPC.
  virtual StatusOr<google::iam::v1::TestIamPermissionsResponse>
      TestIamPermissions(TestIamPermissionsParams) = 0;
};

/**
 * Returns an InstanceAdminConnection object that can be used for interacting
 * with Cloud Spanner's admin APIs.
 *
 * The returned connection object should not be used directly, rather it should
 * be given to a `InstanceAdminClient` instance.
 *
 * @see `InstanceAdminConnection`
 *
 * @param options (optional) configure the `InstanceAdminConnection` created by
 *     this function.
 */
std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    ConnectionOptions const& options = ConnectionOptions());

/**
 * @copydoc MakeInstanceAdminConnection
 *
 * @param retry_policy control for how long (or how many times) are retryable
 *     RPCs attempted
 * @param backoff_policy controls the backoff behavior between retry attempts,
 *     typically some form of exponential backoff with jitter
 * @param polling_policy controls for how often, and how quickly, are long
 *     running checked for completion
 *
 * @par Example
 * @snippet samples.cc custom-instance-admin-policies
 */
std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    ConnectionOptions const& options, std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::unique_ptr<PollingPolicy> polling_policy);

namespace internal {

std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    std::shared_ptr<internal::InstanceAdminStub> base_stub,
    ConnectionOptions const& options);

std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    std::shared_ptr<internal::InstanceAdminStub> base_stub,
    ConnectionOptions const& options, std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy,
    std::unique_ptr<PollingPolicy> polling_policy);

}  // namespace internal

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CONNECTION_H
