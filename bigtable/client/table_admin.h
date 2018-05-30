// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TABLE_ADMIN_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TABLE_ADMIN_H_

#include "bigtable/client/admin_client.h"
#include "bigtable/client/bigtable_strong_types.h"
#include "bigtable/client/column_family.h"
#include "bigtable/client/internal/table_admin.h"
#include "bigtable/client/internal/throw_delegate.h"
#include "bigtable/client/polling_policy.h"
#include "bigtable/client/table_config.h"
#include <future>
#include <memory>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Implements the API to administer tables instance a Cloud Bigtable instance.
 */
class TableAdmin {
 public:
  /**
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param instance_id the id of the instance, e.g., "my-instance", the full
   *   name (e.g. '/projects/my-project/instances/my-instance') is built using
   *   the project id in the @p client parameter.
   */
  TableAdmin(std::shared_ptr<AdminClient> client, std::string instance_id)
      : impl_(std::move(client), std::move(instance_id)) {}

  /**
   * Create a new TableAdmin using explicit policies to handle RPC errors.
   *
   * @tparam RPCRetryPolicy control which operations to retry and for how long.
   * @tparam RPCBackoffPolicy control how does the client backs off after an RPC
   *     error.
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param instance_id the id of the instance, e.g., "my-instance", the full
   *   name (e.g. '/projects/my-project/instances/my-instance') is built using
   *   the project id in the @p client parameter.
   * @param retry_policy the policy to handle RPC errors.
   * @param backoff_policy the policy to control backoff after an error.
   */
  template <typename RPCRetryPolicy, typename RPCBackoffPolicy>
  TableAdmin(std::shared_ptr<AdminClient> client, std::string instance_id,
             RPCRetryPolicy retry_policy, RPCBackoffPolicy backoff_policy)
      : impl_(std::move(client), std::move(instance_id),
              std::move(retry_policy), std::move(backoff_policy)) {}

  /**
   * Create a new TableAdmin using explicit policies to handle RPC errors.
   *
   * @tparam RPCRetryPolicy control which operations to retry and for how long.
   * @tparam RPCBackoffPolicy control how does the client backs off after an RPC
   *     error.
   * @tparam PollingPolicy provides parameters for asynchronous calls.
   * @param client the interface to create grpc stubs, report errors, etc.
   * @param instance_id the id of the instance, e.g., "my-instance", the full
   *   name (e.g. '/projects/my-project/instances/my-instance') is built using
   *   the project id in the @p client parameter.
   * @param retry_policy the policy to handle RPC errors.
   * @param backoff_policy the policy to control backoff after an error.
   * @param polling_policy the policy to control the asynchronous call
   * parameters
   */
  template <typename RPCRetryPolicy, typename RPCBackoffPolicy,
            typename PollingPolicy>
  TableAdmin(std::shared_ptr<AdminClient> client, std::string instance_id,
             RPCRetryPolicy retry_policy, RPCBackoffPolicy backoff_policy,
             PollingPolicy polling_policy)
      : impl_(std::move(client), std::move(instance_id),
              std::move(retry_policy), std::move(backoff_policy),
              std::move(polling_policy)) {}

  TableAdmin(TableAdmin const& table_admin) = default;
  TableAdmin& operator=(TableAdmin const& table_admin) = default;

  std::string const& project() const { return impl_.project(); }
  std::string const& instance_id() const { return impl_.instance_id(); }
  std::string const& instance_name() const { return impl_.instance_name(); }

  /**
   * Create a new table in the instance.
   *
   * @param table_id the name of the table relative to the instance managed by
   *     this object.  The full table name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of this object.
   * @param config the initial schema for the table.
   * @return the attributes of the newly created table.  Notice that the server
   *     only populates the table_name() field at this time.
   * @throws std::exception if the operation cannot be completed.
   *
   * @par Example
   * @snippet bigtable_samples.cc create table
   */
  ::google::bigtable::admin::v2::Table CreateTable(std::string table_id,
                                                   TableConfig config);

  /**
   * Return all the tables in the instance.
   *
   * @param view define what information about the tables is retrieved.
   *   - `VIEW_UNSPECIFIED`: equivalent to `VIEW_SCHEMA`.
   *   - `NAME`: return only the name of the table.
   *   - `VIEW_SCHEMA`: return the name and the schema.
   *   - `FULL`: return all the information about the table.
   *
   * @par Example
   * @snippet bigtable_samples.cc list tables
   */
  std::vector<::google::bigtable::admin::v2::Table> ListTables(
      ::google::bigtable::admin::v2::Table::View view);

  /**
   * Get information about a single table.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @param view describes how much information to get about the name.
   *   - VIEW_UNSPECIFIED: equivalent to VIEW_SCHEMA.
   *   - NAME: return only the name of the table.
   *   - VIEW_SCHEMA: return the name and the schema.
   *   - FULL: return all the information about the table.
   * @return the information about the table.
   * @throws std::exception if the information could not be obtained before the
   *     RPC policies in effect gave up.
   *
   * @par Example
   * @snippet bigtable_samples.cc get table
   */
  ::google::bigtable::admin::v2::Table GetTable(
      std::string const& table_id,
      ::google::bigtable::admin::v2::Table::View view =
          ::google::bigtable::admin::v2::Table::SCHEMA_VIEW);

  /**
   * Delete a table.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @throws std::exception if the table could not be deleted before the RPC
   *     policies in effect gave up.
   *
   * @par Example
   * @snippet bigtable_samples.cc delete table
   */
  void DeleteTable(std::string const& table_id);

  /**
   * Modify the schema for an existing table.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @param modifications the list of modifications to the schema.
   * @return the resulting table schema.
   * @throws std::exception if the operation cannot be completed.
   *
   * @par Example
   * @snippet bigtable_samples.cc modify table
   */
  ::google::bigtable::admin::v2::Table ModifyColumnFamilies(
      std::string const& table_id,
      std::vector<ColumnFamilyModification> modifications);

  /**
   * Delete all the rows that start with a given prefix.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @param row_key_prefix drop any rows that start with this prefix.
   * @throws std::exception if the operation cannot be completed.
   *
   * @par Example
   * @snippet bigtable_samples.cc drop rows by prefix
   */
  void DropRowsByPrefix(std::string const& table_id,
                        std::string row_key_prefix);

  /**
   * Generates consistency token for a table.
   *
   * @param table_id the id of the table for which we want to generate
   *     consistency token.
   * @return the consistency token for table.
   * @throws std::exception if the operation cannot be completed.
   */
  std::string GenerateConsistencyToken(std::string const& table_id);

  /**
   * Checks consistency of a table.
   *
   * @param table_id  the id of the table for which we want to check
   *     consistency.
   * @param consistency_token the consistency token of the table.
   * @return the consistency status for the table.
   * @throws std::exception if the operation cannot be completed.
   */
  bool CheckConsistency(bigtable::TableId const& table_id,
                        bigtable::ConsistencyToken const& consistency_token);

  /**
   * Checks consistency of a table with multiple calls using a separate thread
   *
   * @param table_id the id of the table for which we want to check
   *     consistency.
   * @param consistency_token the consistency token of the table.
   * @return the consistency status for the table.
   * @throws std::exception if the operation cannot be completed.
   */
  std::future<bool> WaitForConsistencyCheck(
      bigtable::TableId const& table_id,
      bigtable::ConsistencyToken const& consistency_token) {
    return std::async(std::launch::async,
                      &TableAdmin::WaitForConsistencyCheckImpl, this, table_id,
                      consistency_token);
  }

  /**
   * Delete all the rows in a table.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @throws std::exception if the operation cannot be completed.
   *
   * @par Example
   * @snippet bigtable_samples.cc drop all rows
   */
  void DropAllRows(std::string const& table_id);

  //@{
  /**
   * @name Snapshot APIs.
   *
   * @warning This is a private alpha release of Cloud Bigtable snapshots. This
   * feature is not currently available to most Cloud Bigtable customers. This
   * feature might be changed in backward-incompatible ways and is not
   * recommended for production use. It is not subject to any SLA or deprecation
   * policy.
   */
  /**
   * Get information about a single snapshot.
   *
   * @warning This is a private alpha release of Cloud Bigtable snapshots. This
   * feature is not currently available to most Cloud Bigtable customers. This
   * feature might be changed in backward-incompatible ways and is not
   * recommended for production use. It is not subject to any SLA or deprecation
   * policy.
   *
   * @param cluster_id the cluster id to which snapshot is associated.
   * @param snapshot_id the id of the snapshot.
   * @return the information about the snapshot.
   * @throws std::exception if the information could not be obtained before the
   *     RPC policies in effect gave up.
   */
  google::bigtable::admin::v2::Snapshot GetSnapshot(
      bigtable::ClusterId const& cluster_id,
      bigtable::SnapshotId const& snapshot_id);

  /**
   * Delete a snapshot.
   *
   * @warning This is a private alpha release of Cloud Bigtable snapshots. This
   * feature is not currently available to most Cloud Bigtable customers. This
   * feature might be changed in backward-incompatible ways and is not
   * recommended for production use. It is not subject to any SLA or deprecation
   * policy.
   *
   * @param cluster_id the id of the cluster to which snapshot belongs.
   * @param snapshot_id the id of the snapshot which needs to be deleted.
   * @throws std::exception if the operation cannot be completed.
   */
  void DeleteSnapshot(bigtable::ClusterId const& cluster_id,
                      bigtable::SnapshotId const& snapshot_id);
  //@}

  /**
   * List snapshots in the given instance.
   * @param cluster_id the name of the cluster for which snapshots should be
   * listed.
   * @return collection containing the snapshots for the given cluster.
   * @throws std::exception if the operation cannot be completed.
   */
  template <template <typename...> class Collection = std::vector>
  Collection<::google::bigtable::admin::v2::Snapshot> ListSnapshots(
      bigtable::ClusterId cluster_id = bigtable::ClusterId("-")) {
    grpc::Status status;
    auto result = impl_.ListSnapshots<Collection>(status, cluster_id);
    if (not status.ok()) {
      bigtable::internal::RaiseRpcError(status, status.error_message());
    }
    return result;
  }

 private:
  /**
   * Implements the polling loop for `WaitForConsistencyCheck` on a
   * separate thread
   */
  bool WaitForConsistencyCheckImpl(
      bigtable::TableId const& table_id,
      bigtable::ConsistencyToken const& consistency_token);

 private:
  noex::TableAdmin impl_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TABLE_ADMIN_H_
