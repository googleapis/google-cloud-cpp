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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_ADMIN_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_ADMIN_H_

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/column_family.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/table_config.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include "google/cloud/status_or.h"
#include <future>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/// The result of checking replication against a given token.
enum class Consistency {
  /// Some of the mutations created before the consistency token have not been
  /// received by all the table replicas.
  kInconsistent,
  /// All mutations created before the consistency token have been received by
  /// all the table replicas.
  kConsistent,
};

/**
 * Implements the API to administer tables in a Cloud Bigtable instance.
 *
 * @par Thread-safety
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Cost
 * Creating a new object of type `TableAdmin` is comparable to creating a few
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
 * error Please consult the
 * [`StatusOr<T>` documentation](#google::cloud::v0::StatusOr) for more details.
 *
 * @code
 * namespace cbt = google::cloud::bigtable;
 * namespace btadmin = google::bigtable::admin::v2;
 * cbt::TableAdmin admin = ...;
 * google::cloud::StatusOr<btadmin::Table> metadata = admin.GetTable(...);
 *
 * if (!metadata) {
 *   std::cerr << "Error fetching table metadata\n";
 *   return;
 * }
 *
 * // Use "metadata" as a smart pointer here, e.g.:
 * std::cout << "The full table name is " << table->name() << " the table has "
 *           << table->column_families_size() << " column families\n";
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
 * @see #google::cloud::v0::StatusOr for a description of the error reporting
 *     class used by this library.
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
        rpc_retry_policy_prototype_(
            DefaultRPCRetryPolicy(internal::kBigtableTableAdminLimits)),
        rpc_backoff_policy_prototype_(
            DefaultRPCBackoffPolicy(internal::kBigtableTableAdminLimits)),
        metadata_update_policy_(instance_name(), MetadataParamTypes::PARENT),
        polling_policy_prototype_(
            DefaultPollingPolicy(internal::kBigtableTableAdminLimits)) {}

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

  TableAdmin(TableAdmin const&) = default;
  TableAdmin& operator=(TableAdmin const&) = default;

  //@{
  /// @name Convenience shorthands for the schema views.
  using TableView = google::bigtable::admin::v2::Table::View;
  /// Use the default view as defined for each function.
  constexpr static TableView VIEW_UNSPECIFIED =
      google::bigtable::admin::v2::Table::VIEW_UNSPECIFIED;
  /// Populate only the name in the responses.
  constexpr static TableView NAME_ONLY =
      google::bigtable::admin::v2::Table::NAME_ONLY;
  /// Populate only the name and the fields related to the table schema.
  constexpr static TableView SCHEMA_VIEW =
      google::bigtable::admin::v2::Table::SCHEMA_VIEW;
  /// Populate only the name and the fields related to the table replication
  /// state.
  constexpr static TableView REPLICATION_VIEW =
      google::bigtable::admin::v2::Table::REPLICATION_VIEW;
  /// Populate all the fields in the response.
  constexpr static TableView FULL = google::bigtable::admin::v2::Table::FULL;
  //@}

  std::string const& project() const { return client_->project(); }
  std::string const& instance_id() const { return instance_id_; }
  std::string const& instance_name() const { return instance_name_; }

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
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_snippets.cc create table
   */
  StatusOr<::google::bigtable::admin::v2::Table> CreateTable(
      std::string table_id, TableConfig config);

  /**
   * Sends an asynchronous request to create a new table in the instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param table_id the name of the table relative to the instance managed by
   *     this object.  The full table name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of this object.
   * @param config the initial schema for the table.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires.  In the first case, the future will contain the
   *   response from the service. In the second the future is satisfied with
   *   an exception. Note that the service only fills out the `table_name` field
   *   for this request.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_async_snippets.cc async create table
   */
  future<StatusOr<google::bigtable::admin::v2::Table>> AsyncCreateTable(
      CompletionQueue& cq, std::string table_id, TableConfig config);

  /**
   * Return all the tables in the instance.
   *
   * @param view define what information about the tables is retrieved.
   *   - `VIEW_UNSPECIFIED`: equivalent to `VIEW_SCHEMA`.
   *   - `NAME`: return only the name of the table.
   *   - `VIEW_SCHEMA`: return the name and the schema.
   *   - `FULL`: return all the information about the table.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_snippets.cc list tables
   */
  StatusOr<std::vector<::google::bigtable::admin::v2::Table>> ListTables(
      ::google::bigtable::admin::v2::Table::View view);

  /**
   * Sends an asynchronous request to get all the tables in the instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param view describes how much information to get about the name.
   *   - VIEW_UNSPECIFIED: equivalent to VIEW_SCHEMA.
   *   - NAME: return only the name of the table.
   *   - VIEW_SCHEMA: return the name and the schema.
   *   - FULL: return all the information about the table.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second the future is satisfied with
   *   an exception.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_async_snippets.cc async list tables
   */
  future<StatusOr<std::vector<::google::bigtable::admin::v2::Table>>>
  AsyncListTables(CompletionQueue& cq,
                  google::bigtable::admin::v2::Table::View view);

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
   * @return the information about the table or status.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_snippets.cc get table
   */
  StatusOr<::google::bigtable::admin::v2::Table> GetTable(
      std::string const& table_id, TableView view = SCHEMA_VIEW);

  /**
   * Sends an asynchronous request to get information about an existing table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @param view describes how much information to get about the name.
   *   - VIEW_UNSPECIFIED: equivalent to VIEW_SCHEMA.
   *   - NAME: return only the name of the table.
   *   - VIEW_SCHEMA: return the name and the schema.
   *   - FULL: return all the information about the table.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second the future is satisfied with
   *   an exception.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_async_snippets.cc async get table
   */
  future<StatusOr<google::bigtable::admin::v2::Table>> AsyncGetTable(
      CompletionQueue& cq, std::string const& table_id,
      google::bigtable::admin::v2::Table::View view);

  /**
   * Delete a table.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   *
   * @return status of the operation.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_snippets.cc delete table
   */
  Status DeleteTable(std::string const& table_id);

  /**
   * Start a request to asynchronously delete a table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   *
   * @return status of the operation.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_async_snippets.cc async delete table
   */
  future<Status> AsyncDeleteTable(CompletionQueue& cq,
                                  std::string const& table_id);

  /**
   * Modify the schema for an existing table.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @param modifications the list of modifications to the schema.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_snippets.cc modify table
   */
  StatusOr<::google::bigtable::admin::v2::Table> ModifyColumnFamilies(
      std::string const& table_id,
      std::vector<ColumnFamilyModification> modifications);

  /**
   * Make an asynchronous request to modify the column families of a table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param table_id the name of the table relative to the instance managed by
   *     this object.  The full table name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of this object.
   * @param modifications the list of modifications to the schema.
   * @return the information about table or status.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_async_snippets.cc async modify table
   */
  future<StatusOr<::google::bigtable::admin::v2::Table>>
  AsyncModifyColumnFamilies(
      CompletionQueue& cq, std::string const& table_id,
      std::vector<ColumnFamilyModification> modifications);

  /**
   * Delete all the rows that start with a given prefix.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @param row_key_prefix drop any rows that start with this prefix.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_snippets.cc drop rows by prefix
   */
  Status DropRowsByPrefix(std::string const& table_id,
                          std::string row_key_prefix);

  /**
   * Make an asynchronous request to delete all the rows that start with a given
   * prefix.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @param row_key_prefix drop any rows that start with this prefix.
   * @return status of the operation.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_async_snippets.cc async drop rows by prefix
   */
  future<Status> AsyncDropRowsByPrefix(CompletionQueue& cq,
                                       std::string const& table_id,
                                       std::string row_key_prefix);

  /**
   * Generates consistency token for a table.
   *
   * @param table_id the id of the table for which we want to generate
   *     consistency token.
   * @return the consistency token for table.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_snippets.cc generate consistency token
   */
  StatusOr<std::string> GenerateConsistencyToken(std::string const& table_id);

  /**
   * Make an asynchronous request to generates consistency token for a table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   * @return consistency token or status of the operation.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_async_snippets.cc async generate consistency token
   */
  future<StatusOr<std::string>> AsyncGenerateConsistencyToken(
      CompletionQueue& cq, std::string const& table_id);

  /**
   * Checks consistency of a table.
   *
   * @param table_id  the id of the table for which we want to check
   *     consistency.
   * @param consistency_token the consistency token of the table.
   * @return the consistency status for the table.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_snippets.cc check consistency
   */
  StatusOr<Consistency> CheckConsistency(std::string const& table_id,
                                         std::string const& consistency_token);

  /**
   * Make an asynchronous request to check consistency of a table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param table_id  the id of the table for which we want to check
   *     consistency.
   * @param consistency_token the consistency token of the table.
   * @return consistency status or status of the operation.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_async_snippets.cc async check consistency
   */
  future<StatusOr<Consistency>> AsyncCheckConsistency(
      CompletionQueue& cq, std::string const& table_id,
      std::string const& consistency_token);

  /**
   * Checks consistency of a table with multiple calls using a separate thread
   *
   * @param table_id the id of the table for which we want to check
   *     consistency.
   * @param consistency_token the consistency token of the table.
   * @return the consistency status for the table.
   * @throws std::exception if the operation cannot be completed.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_snippets.cc wait for consistency check
   */
  google::cloud::future<StatusOr<Consistency>> WaitForConsistency(
      std::string const& table_id, std::string const& consistency_token);

  /**
   * Asynchronously wait until a table is consistent with the given @p token.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param table_id the id of the table for which we want to check
   *     consistency.
   * @param consistency_token the consistency token of the table.
   * @return the consistency status for the table.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_async_snippets.cc async wait for consistency
   */
  google::cloud::future<StatusOr<Consistency>> AsyncWaitForConsistency(
      CompletionQueue& cq, std::string const& table_id,
      std::string const& consistency_token);

  /**
   * Delete all the rows in a table.
   *
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_snippets.cc drop all rows
   */
  Status DropAllRows(std::string const& table_id);

  /**
   * Make an asynchronous request to delete all the rows in a table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param table_id the id of the table within the instance associated with
   *     this object. The full name of the table is
   *     `this->instance_name() + "/tables/" + table_id`
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_async_snippets.cc async drop all rows
   */
  future<Status> AsyncDropAllRows(CompletionQueue& cq,
                                  std::string const& table_id);

  /// Return the fully qualified name of a table in this object's instance.
  std::string TableName(std::string const& table_id) const {
    return instance_name() + "/tables/" + table_id;
  }

  /// Return the fully qualified name of a Cluster.
  std::string ClusterName(std::string const& cluster_id) {
    return instance_name() + "/clusters/" + cluster_id;
  }

 private:
  //@{
  /// @name Helper functions to implement constructors with changed policies.
  void ChangePolicy(RPCRetryPolicy const& policy) {
    rpc_retry_policy_prototype_ = policy.clone();
  }

  void ChangePolicy(RPCBackoffPolicy const& policy) {
    rpc_backoff_policy_prototype_ = policy.clone();
  }

  void ChangePolicy(PollingPolicy const& policy) {
    polling_policy_prototype_ = policy.clone();
  }

  template <typename Policy, typename... Policies>
  void ChangePolicies(Policy&& policy, Policies&&... policies) {
    ChangePolicy(policy);
    ChangePolicies(std::forward<Policies>(policies)...);
  }
  void ChangePolicies() {}
  //@}

  std::unique_ptr<RPCRetryPolicy> clone_rpc_retry_policy() {
    return rpc_retry_policy_prototype_->clone();
  }

  std::unique_ptr<RPCBackoffPolicy> clone_rpc_backoff_policy() {
    return rpc_backoff_policy_prototype_->clone();
  }

  MetadataUpdatePolicy clone_metadata_update_policy() {
    return metadata_update_policy_;
  }

  std::unique_ptr<PollingPolicy> clone_polling_policy() {
    return polling_policy_prototype_->clone();
  }

  /// Compute the fully qualified instance name.
  std::string InstanceName() const;

  std::shared_ptr<AdminClient> client_;
  std::string instance_id_;
  std::string instance_name_;
  std::shared_ptr<RPCRetryPolicy const> rpc_retry_policy_prototype_;
  std::shared_ptr<RPCBackoffPolicy const> rpc_backoff_policy_prototype_;
  bigtable::MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<PollingPolicy const> polling_policy_prototype_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_ADMIN_H_
