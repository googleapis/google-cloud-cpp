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

#include <memory>
#include "bigtable/client/admin_client.h"
#include "bigtable/client/column_family.h"
#include "bigtable/client/internal/unary_rpc_utils.h"
#include "bigtable/client/table_config.h"

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
      : client_(std::move(client)),
        instance_id_(std::move(instance_id)),
        instance_name_(InstanceName()),
        rpc_retry_policy_(DefaultRPCRetryPolicy()),
        rpc_backoff_policy_(DefaultRPCBackoffPolicy()),
        rpc_metadata_holder_(DefaultRPCMetadataHolder(
            instance_name(), RPCRequestParamType::kParent)) {}

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
      : client_(std::move(client)),
        instance_id_(std::move(instance_id)),
        instance_name_(InstanceName()),
        rpc_retry_policy_(retry_policy.clone()),
        rpc_backoff_policy_(backoff_policy.clone()),
        rpc_metadata_holder_(DefaultRPCMetadataHolder(
            instance_name(), RPCRequestParamType::kParent)) {}

  std::string const& project() const { return client_->project(); }
  std::string const& instance_id() const { return instance_id_; }
  std::string const& instance_name() const { return instance_name_; }

  /**
   * Create a new table in the instance.
   *
   * This function creates a new table and sets its initial schema, for example:
   *
   * @code
   * bigtable::TableAdmin admin = ...;
   * admin.CreateTable(
   *     "my-table", bigtable::TableConfig(
   *         {{"family", bigtable::GcRule::MaxNumVersions(1)}}, {}));
   * @endcode
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
   */
  ::google::bigtable::admin::v2::Table CreateTable(std::string table_id,
                                                   TableConfig config);

  /**
   * Return all the tables in the instance.
   *
   * @param view define what information about the tables is retrieved.
   *   - VIEW_UNSPECIFIED: equivalent to VIEW_SCHEMA.
   *   - NAME: return only the name of the table.
   *   - VIEW_SCHEMA: return the name and the schema.
   *   - FULL: return all the information about the table.
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
   */
  ::google::bigtable::admin::v2::Table GetTable(
      std::string table_id,
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
   */
  void DeleteTable(std::string table_id);

  /**
   * Modify the schema for an existing table.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @param modifications the list of modifications to the schema.
   * @return the resulting table schema.
   * @throws std::exception if the operation cannot be completed.
   */
  ::google::bigtable::admin::v2::Table ModifyColumnFamilies(
      std::string table_id,
      std::vector<ColumnFamilyModification> modifications);

  /**
   * Delete all the rows that start with a given prefix.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @param row_key_prefix drop any rows that start with this prefix.
   * @throws std::exception if the operation cannot be completed.
   */
  void DropRowsByPrefix(std::string table_id, std::string row_key_prefix);

  /**
   * Delete all the rows in a table.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @throws std::exception if the operation cannot be completed.
   */
  void DropAllRows(std::string table_id);

 private:
  /// Compute the fully qualified instance name.
  std::string InstanceName() const;

  /// Return the fully qualified name of a table in this object's instance.
  std::string TableName(std::string const& table_id) const {
    return instance_name() + "/tables/" + table_id;
  }

  /// Shortcuts to avoid typing long names over and over.
  using RpcUtils = bigtable::internal::UnaryRpcUtils<AdminClient>;
  using StubType = RpcUtils::StubType;

 private:
  std::shared_ptr<AdminClient> client_;
  std::string instance_id_;
  std::string instance_name_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  std::unique_ptr<RPCMetadataHolder> rpc_metadata_holder_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_TABLE_ADMIN_H_
