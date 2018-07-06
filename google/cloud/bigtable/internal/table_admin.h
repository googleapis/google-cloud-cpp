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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_ADMIN_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_ADMIN_H_

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/column_family.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/table_config.h"
#include <future>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace noex {

/**
 * Implements the API to administer tables instance a Cloud Bigtable instance.
 */
class TableAdmin {
 public:
  // Make the class bigtable::TableAdmin friend of this class, So private
  // functions of this class can be accessed
  friend class bigtable::TableAdmin;
  /**
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param instance_id the id of the instance, e.g., "my-instance", the full
   *   name (e.g. '/projects/my-project/instances/my-instance') is built using
   *   the project id in the @p client parameter.
   */
  TableAdmin(std::shared_ptr<AdminClient> client, std::string instance_id)
      : client_(std::move(client)),
        instance_id_(std::move(instance_id)),
        instance_name_(InstanceName()),
        rpc_retry_policy_(DefaultRPCRetryPolicy()),
        rpc_backoff_policy_(DefaultRPCBackoffPolicy()),
        metadata_update_policy_(instance_name(), MetadataParamTypes::PARENT),
        polling_policy_(DefaultPollingPolicy()) {}

  /**
   * Create a new TableAdmin using explicit policies to handle RPC errors.
   *
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param instance_id the id of the instance, e.g., "my-instance", the full
   *   name (e.g. '/projects/my-project/instances/my-instance') is built using
   *   the project id in the @p client parameter.
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
  TableAdmin(std::shared_ptr<AdminClient> client, std::string instance_id,
             Policies&&... policies)
      : TableAdmin(std::move(client), std::move(instance_id)) {
    ChangePolicies(std::forward<Policies>(policies)...);
  }

  std::string const& project() const { return client_->project(); }
  std::string const& instance_id() const { return instance_id_; }
  std::string const& instance_name() const { return instance_name_; }

  //@{
  /**
   * @name No exception versions of TableAdmin::*
   *
   * These functions provide the same functionality as their counterparts in the
   * `bigtable::TableAdmin` class, but do not raise exceptions on errors,
   * instead they return the error on the status parameter.
   */
  ::google::bigtable::admin::v2::Table CreateTable(std::string table_id,
                                                   TableConfig config,
                                                   grpc::Status& status);

  std::vector<::google::bigtable::admin::v2::Table> ListTables(
      ::google::bigtable::admin::v2::Table::View view, grpc::Status& status);

  ::google::bigtable::admin::v2::Table GetTable(
      std::string const& table_id, grpc::Status& status,
      ::google::bigtable::admin::v2::Table::View view =
          ::google::bigtable::admin::v2::Table::SCHEMA_VIEW);

  void DeleteTable(std::string const& table_id, grpc::Status& status);

  ::google::bigtable::admin::v2::Table ModifyColumnFamilies(
      std::string const& table_id,
      std::vector<ColumnFamilyModification> modifications,
      grpc::Status& status);

  void DropRowsByPrefix(std::string const& table_id, std::string row_key_prefix,
                        grpc::Status& status);

  void DropAllRows(std::string const& table_id, grpc::Status& status);

  ::google::bigtable::admin::v2::Snapshot GetSnapshot(
      bigtable::ClusterId const& cluster_id,
      bigtable::SnapshotId const& snapshot_id, grpc::Status& status);

  std::string GenerateConsistencyToken(std::string const& table_id,
                                       grpc::Status& status);

  bool CheckConsistency(bigtable::TableId const& table_id,
                        bigtable::ConsistencyToken const& consistency_token,
                        grpc::Status& status);

  void DeleteSnapshot(bigtable::ClusterId const& cluster_id,
                      bigtable::SnapshotId const& snapshot_id,
                      grpc::Status& status);

  template <template <typename...> class Collection = std::vector>
  Collection<::google::bigtable::admin::v2::Snapshot> ListSnapshots(
      grpc::Status& status,
      bigtable::ClusterId const& cluster_id = bigtable::ClusterId("-")) {
    Collection<::google::bigtable::admin::v2::Snapshot> result;
    ListSnapshotsImpl(
        cluster_id,
        [&result](::google::bigtable::admin::v2::Snapshot snapshot) {
          result.emplace_back(std::move(snapshot));
        },
        status);
    return result;
  }

  //@}

 private:
  //@{
  /// @name Helper functions to implement constructors with changed policies.
  void ChangePolicy(RPCRetryPolicy& policy) {
    rpc_retry_policy_ = policy.clone();
  }

  void ChangePolicy(RPCBackoffPolicy& policy) {
    rpc_backoff_policy_ = policy.clone();
  }

  void ChangePolicy(PollingPolicy& policy) { polling_policy_ = policy.clone(); }

  template <typename Policy, typename... Policies>
  void ChangePolicies(Policy&& policy, Policies&&... policies) {
    ChangePolicy(policy);
    ChangePolicies(std::forward<Policies>(policies)...);
  }
  void ChangePolicies() {}
  //@}

  /// Compute the fully qualified instance name.
  std::string InstanceName() const;

  /// Return the fully qualified name of a table in this object's instance.
  std::string TableName(std::string const& table_id) const {
    return instance_name() + "/tables/" + table_id;
  }

  /// Return the fully qualified name of a snapshot.
  std::string SnapshotName(bigtable::ClusterId const& cluster_id,
                           bigtable::SnapshotId const& snapshot_id) {
    return instance_name() + "/clusters/" + cluster_id.get() + "/snapshots/" +
           snapshot_id.get();
  }

  /// Return the fully qualified name of a Cluster.
  std::string ClusterName(bigtable::ClusterId const& cluster_id) {
    return instance_name() + "/clusters/" + cluster_id.get();
  }

  /**
   * Refactor implementation to `.cc` file.
   *
   * Provides a compilation barrier so that the application is not
   * exposed to all the implementation details.
   *
   * @param cluster_id cluster_id which contains the snapshots.
   * @param inserter Function to insert the object to result.
   * @param clearer Function to clear the result object if RPC fails.
   * @param status Status which contains the information whether the operation
   * succeeded successfully or not.
   */
  void ListSnapshotsImpl(
      bigtable::ClusterId const& cluster_id,
      std::function<void(google::bigtable::admin::v2::Snapshot)> const&
          inserter,
      grpc::Status& status);

  bool WaitForConsistencyCheckHelper(
      bigtable::TableId const& table_id,
      bigtable::ConsistencyToken const& consistency_token,
      grpc::Status& status);

 private:
  std::shared_ptr<AdminClient> client_;
  std::string instance_id_;
  std::string instance_name_;
  std::shared_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::shared_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  bigtable::MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<PollingPolicy> polling_policy_;
};

}  // namespace noex
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_TABLE_ADMIN_H_
