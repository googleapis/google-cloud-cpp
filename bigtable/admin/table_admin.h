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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_ADMIN_TABLE_ADMIN_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_ADMIN_TABLE_ADMIN_H_

#include "bigtable/admin/admin_client.h"

#include <memory>
#include <thread>

#include <absl/strings/string_view.h>

#include "bigtable/admin/column_family.h"
#include "bigtable/admin/table_config.h"
#include "bigtable/client/rpc_backoff_policy.h"
#include "bigtable/client/rpc_retry_policy.h"

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
      : client_(client),
        instance_id_(std::move(instance_id)),
        instance_name_(InstanceName()),
        rpc_retry_policy_(DefaultRPCRetryPolicy()),
        rpc_backoff_policy_(DefaultRPCBackoffPolicy()) {}

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
      : client_(client),
        instance_id_(std::move(instance_id)),
        instance_name_(InstanceName()),
        rpc_retry_policy_(retry_policy.clone()),
        rpc_backoff_policy_(backoff_policy.clone()) {}

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

  /// A shortcut for the grpc stub this class wraps.
  using StubType =
      ::google::bigtable::admin::v2::BigtableTableAdmin::StubInterface;

  /**
   * Determine if @p T is a pointer to member function with the expected
   * signature for CallWithRetry().
   *
   * This is the generic case, where the type does not match the expected
   * signature.  The class derives from std::false_type, so
   * CheckSignature<T>::value is `false`.
   *
   * @tparam T the type to check against the expected signature.
   */
  template <typename T>
  struct CheckSignature : std::false_type {
    /// Must define ResponseType because it is used in std::enable_if<>.
    using ResponseType = int;
  };

  /**
   * Determine if a type is a pointer to member function with the correct
   * signature for CallWithRetry()
   *
   * This is the case where the type actually matches the expected signature.
   * This class derives from std::true_type, so CheckSignature<T>::value is
   * `true` in this case.  The class also extracts the request and response
   * types used in the implementation of CallWithRetry().
   *
   * @tparam Request the RPC request type.
   * @tparam Response the RPC response type.
   */
  template <typename Request, typename Response>
  struct CheckSignature<grpc::Status (StubType::*)(grpc::ClientContext*,
                                                   Request const&, Response*)>
      : public std::true_type {
    using RequestType = Request;
    using ResponseType = Response;
    using MemberFunctionType = grpc::Status (StubType::*)(grpc::ClientContext*,
                                                          Request const&,
                                                          Response*);
  };

  /**
   * Call a simple unary RPC with retries.
   *
   * Given a pointer to member function in the grpc StubInterface class this
   * generic function calls it with retries until success or until the RPC
   * policies determine that this is an error.
   *
   * We use std::enable_if<> to stop signature errors at compile-time.  The
   * `CheckSignature` meta function returns `false` if given a type that is not
   * a pointer to member with the right signature, that disables this function
   * altogether, and the developer gets a nice-ish error message.
   *
   * @tparam MemberFunction the signature of the member function.
   * @param function the pointer to the member function to call.
   * @param request an initialized request parameter for the RPC.
   * @return the return parameter from the RPC.
   * @throw std::exception with a description of the RPC error.
   */
  template <typename MemberFunction>
  // Disable the function if the provided member function does not match the
  // expected signature.  Compilers also emit nice error messages in this case.
  typename std::enable_if<
      CheckSignature<MemberFunction>::value,
      typename CheckSignature<MemberFunction>::ResponseType>::type
  CallWithRetry(
      MemberFunction function,
      typename CheckSignature<MemberFunction>::RequestType const& request,
      absl::string_view error_message) {
    // Copy the policies in effect for the operation.
    auto rpc_policy = rpc_retry_policy_->clone();
    auto backoff_policy = rpc_backoff_policy_->clone();

    typename CheckSignature<MemberFunction>::ResponseType response;
    while (true) {
      grpc::ClientContext client_context;
      rpc_policy->setup(client_context);
      backoff_policy->setup(client_context);
      // Call the pointer to member function.
      grpc::Status status = ((*client_->Stub()).*function)(
          &client_context, request, &response);
      client_->on_completion(status);
      if (status.ok()) {
        break;
      }
      if (not rpc_policy->on_failure(status)) {
        RaiseError(status, error_message);
      }
      auto delay = backoff_policy->on_completion(status);
      std::this_thread::sleep_for(delay);
    }
    return response;
  }

  /// Raise an exception representing the given status.
  [[noreturn]] void RaiseError(grpc::Status const& status,
                               absl::string_view error_message) const;

 private:
  std::shared_ptr<AdminClient> client_;
  std::string instance_id_;
  std::string instance_name_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_ADMIN_TABLE_ADMIN_H_
