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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CONNECTION_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CONNECTION_H_

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/internal/instance_admin_stub.h"
#include "google/cloud/spanner/internal/range_from_pagination.h"
#include "google/cloud/spanner/retry_policy.h"
#include <google/spanner/admin/instance/v1/spanner_instance_admin.grpc.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
/**
 * An input range to stream all the databases in a Cloud Spanner instance.
 *
 * This type models an [input range][cppref-input-range] of
 * `google::spanner::admin::v1::Database` objects. Applications can make a
 * single pass through the results.
 *
 * [cppref-input-range]: https://en.cppreference.com/w/cpp/ranges/input_range
 */
using ListInstancesRange = internal::PaginationRange<
    google::spanner::admin::instance::v1::Instance,
    google::spanner::admin::instance::v1::ListInstancesRequest,
    google::spanner::admin::instance::v1::ListInstancesResponse>;

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

  struct GetInstanceParams {
    std::string instance_name;
  };
  virtual StatusOr<google::spanner::admin::instance::v1::Instance> GetInstance(
      GetInstanceParams) = 0;

  /**
   * The parameters for a `ListInstances()` request.
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

  /**
   * Returns a one-pass input range with all the instances meeting the
   * requirements in @p params
   */
  virtual ListInstancesRange ListInstances(ListInstancesParams params) = 0;
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

namespace internal {
/**
 * Create an InstanceAdminConnection using an existing base Stub.
 *
 * Returns a new InstanceAdminConnection with all the normal decorators applied
 * to @p base_stub.
 */
std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    std::shared_ptr<internal::InstanceAdminStub> base_stub,
    ConnectionOptions const& options);

std::shared_ptr<InstanceAdminConnection> MakeInstanceAdminConnection(
    std::shared_ptr<internal::InstanceAdminStub> base_stub,
    ConnectionOptions const& options, std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy);
}  // namespace internal

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_INSTANCE_ADMIN_CONNECTION_H_
