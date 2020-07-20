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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_ADMIN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_ADMIN_H

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/column_family.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/iam_policy.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/polling_policy.h"
#include "google/cloud/bigtable/table_config.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/iam_policy.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
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
 * [`StatusOr<T>` documentation](#google::cloud::v1::StatusOr) for more details.
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
 * @see #google::cloud::v1::StatusOr for a description of the error reporting
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
  // NOLINTNEXTLINE(performance-unnecessary-value-param) TODO(#4112)
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
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static TableView VIEW_UNSPECIFIED =
      google::bigtable::admin::v2::Table::VIEW_UNSPECIFIED;
  /// Populate only the name in the responses.
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static TableView NAME_ONLY =
      google::bigtable::admin::v2::Table::NAME_ONLY;
  /// Populate only the name and the fields related to the table schema.
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static TableView SCHEMA_VIEW =
      google::bigtable::admin::v2::Table::SCHEMA_VIEW;
  /// Populate only the name and the fields related to the table replication
  /// state.
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static TableView REPLICATION_VIEW =
      google::bigtable::admin::v2::Table::REPLICATION_VIEW;
  /// Populate all the fields in the response.
  // NOLINTNEXTLINE(readability-identifier-naming)
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
   * Parameters for `CreateBackup` and `AsyncCreateBackup`.
   *
   * @param cluster_id the name of the cluster relative to the instance managed
   *     by the `TableAdmin` object. The full cluster name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<cluster_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object.
   * @param backup_id the name of the backup relative to the cluster specified.
   *     The full backup name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<CLUSTER_ID>/backups/<backup_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient,
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object, and
   *     CLUSTER_ID is the cluster_id specified for this object.
   * @param table_id the id of the table within the instance to be backed up.
   *     The full name of the table is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object.
   * @param expire_time the date and time when the created backup will expire.
   */
  struct CreateBackupParams {
    CreateBackupParams() = default;
    CreateBackupParams(std::string cluster_id, std::string backup_id,
                       std::string table_id,
                       std::chrono::system_clock::time_point expire_time)
        : cluster_id(std::move(cluster_id)),
          backup_id(std::move(backup_id)),
          table_name(std::move(table_id)),
          expire_time(std::move(expire_time)) {}

    google::bigtable::admin::v2::CreateBackupRequest AsProto(
        std::string instance_name) const;

    std::string cluster_id;
    std::string backup_id;
    std::string table_name;
    std::chrono::system_clock::time_point expire_time;
  };

  /**
   * Create a new backup of a table in the instance.
   *
   * @param params instance of `CreateBackupParams`.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc create backup
   */
  StatusOr<google::bigtable::admin::v2::Backup> CreateBackup(
      CreateBackupParams const& params);

  /**
   * Sends an asynchronous request to create a new backup of a table in the
   * instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param params instance of `CreateBackupParams`.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second case, the future is satisfied
   *   with an exception.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_async_snippets.cc async create backup
   *
   */
  future<StatusOr<google::bigtable::admin::v2::Backup>> AsyncCreateBackup(
      CompletionQueue& cq, CreateBackupParams const& params);

  /**
   * Get information about a single backup.
   *
   * @param cluster_id the name of the cluster relative to the instance managed
   *     by the `TableAdmin` object. The full cluster name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<cluster_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object.
   * @param backup_id the name of the backup relative to the cluster specified.
   *     The full backup name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<CLUSTER_ID>/backups/<backup_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient,
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object, and
   *     CLUSTER_ID is the cluster_id previously specified.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc get backup
   */
  StatusOr<google::bigtable::admin::v2::Backup> GetBackup(
      std::string const& cluster_id, std::string const& backup_id);

  // clang-format off
  /**
   * Sends an asynchronous request to get information about a single backup.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param cluster_id the name of the cluster relative to the instance managed
   *     by the `TableAdmin` object. The full cluster name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<cluster_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object.
   * @param backup_id the name of the backup relative to the cluster specified.
   *     The full backup name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<CLUSTER_ID>/backups/<backup_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient,
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object, and
   *     CLUSTER_ID is the cluster_id previously specified.
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second case, the future is satisfied
   *   with an exception.

   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_async_snippets.cc async get backup
   */
  // clang-format on
  future<StatusOr<google::bigtable::admin::v2::Backup>> AsyncGetBackup(
      CompletionQueue& cq, std::string const& cluster_id,
      std::string const& backup_id);

  /**
   * Parameters for `UpdateBackup` and `AsyncUpdateBackup`.
   *
   * @param cluster_id the name of the cluster relative to the instance managed
   *     by the `TableAdmin` object. The full cluster name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<cluster_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object.
   * @param backup_id the name of the backup relative to the cluster specified.
   *     The full backup name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<CLUSTER_ID>/backups/<backup_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient,
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object, and
   *     CLUSTER_ID is the cluster_id specified for this object.
   * @param expire_time the date and time when the created backup will expire.
   */
  struct UpdateBackupParams {
    UpdateBackupParams() = default;
    UpdateBackupParams(std::string cluster_id, std::string backup_id,
                       std::chrono::system_clock::time_point expire_time)
        : cluster_id(std::move(cluster_id)),
          backup_name(std::move(backup_id)),
          expire_time(std::move(expire_time)) {}

    google::bigtable::admin::v2::UpdateBackupRequest AsProto(
        std::string const& instance_name) const;

    std::string cluster_id;
    std::string backup_name;
    std::chrono::system_clock::time_point expire_time;
  };

  /**
   * Updates a backup of a table in the instance.
   *
   * @param params instance of `UpdateBackupParams`.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc update backup
   */
  StatusOr<google::bigtable::admin::v2::Backup> UpdateBackup(
      UpdateBackupParams const& params);

  /**
   * Sends an asynchronous request to update a backup of a table in the
   * instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param params instance of `UpdateBackupParams`.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second case, the future is satisfied
   *   with an exception.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_async_snippets.cc async update backup
   *
   */
  future<StatusOr<google::bigtable::admin::v2::Backup>> AsyncUpdateBackup(
      CompletionQueue& cq, UpdateBackupParams const& params);

  /**
   * Delete a backup.
   *
   * @param cluster_id the name of the cluster relative to the instance managed
   *     by the `TableAdmin` object. The full cluster name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<cluster_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object.
   * @param backup_id the name of the backup relative to the cluster specified.
   *     The full backup name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<CLUSTER_ID>/backups/<backup_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient,
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object, and
   *     CLUSTER_ID is the cluster_id previously specified.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc delete backup
   */
  Status DeleteBackup(std::string const& cluster_id,
                      std::string const& backup_id);

  /**
   * Delete a backup.
   *
   * @param backup typically returned by a call to `GetBackup` or `ListBackups`.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc delete backup
   */
  Status DeleteBackup(google::bigtable::admin::v2::Backup const& backup);

  /**
   * Sends an asynchronous request to delete a backup.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param cluster_id the name of the cluster relative to the instance managed
   *     by the `TableAdmin` object. The full cluster name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<cluster_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object.
   * @param backup_id the name of the backup relative to the cluster specified.
   *     The full backup name is
   *    `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<CLUSTER_ID>/backups/<backup_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient,
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object, and
   *     CLUSTER_ID is the cluster_id previously specified.
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second case, the future is satisfied
   *   with an exception.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_async_snippets.cc async delete backup
   */
  future<Status> AsyncDeleteBackup(CompletionQueue& cq,
                                   std::string const& cluster_id,
                                   std::string const& backup_id);

  /**
   * Sends an asynchronous request to delete a backup.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param backup typically returned by a call to `GetBackup` or `ListBackups`.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second case, the future is satisfied
   *   with an exception.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_async_snippets.cc async delete backup
   */
  future<Status> AsyncDeleteBackup(
      CompletionQueue& cq, google::bigtable::admin::v2::Backup const& backup);

  /**
   * Parameters for `ListBackups` and `AsyncListBackups`.
   */
  struct ListBackupsParams {
    ListBackupsParams() = default;

    /**
     * Sets the cluster_id.
     *
     * @param cluster_id the name of the cluster relative to the instance
     *     managed by the `TableAdmin` object. If no cluster_id is specified,
     *     teh all backups in all clusters are listed. The full cluster name is
     *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<cluster_id>`
     *     where PROJECT_ID is obtained from the associated AdminClient and
     *     INSTANCE_ID is the instance_id() of the `TableAdmin` object.
     */
    ListBackupsParams& set_cluster(std::string const& cluster_id) {
      this->cluster_id = cluster_id;
      return *this;
    }

    /**
     * Sets the filtering expression.
     *
     * @param filter expression that filters backups listed in the response.
     *     The expression must specify the field name, a comparison operator,
     *     and the value that you want to use for filtering. The value must be a
     *     string, a number, or a boolean. The comparison operator must be
     *     <, >, <=, >=, !=, =, or :. Colon ‘:’ represents a HAS operator which
     *     is roughly synonymous with equality. Filter rules are case
     *     insensitive.
     *
     *     The fields eligible for filtering are:
     *       * `name`
     *       * `table`
     *       * `state`
     *       * `start_time` (and values are of the format YYYY-MM-DDTHH:MM:SSZ)
     *       * `end_time` (and values are of the format YYYY-MM-DDTHH:MM:SSZ)
     *       * `expire_time` (and values are of the format YYYY-MM-DDTHH:MM:SSZ)
     *       * `size_bytes`
     *
     *     To filter on multiple expressions, provide each separate expression
     *     within parentheses. By default, each expression is an AND expression.
     *     However, you can include AND, OR, and NOT expressions explicitly.
     *
     *     Some examples of using filters are:
     *       * `name:"exact"` --> The backup's name is the string "exact".
     *       * `name:howl` --> The backup's name contains the string "howl".
     *       * `table:prod` --> The table's name contains the string "prod".
     *       * `state:CREATING` --> The backup is pending creation.
     *       * `state:READY` --> The backup is fully created and ready for use.
     *       * `(name:howl) AND (start_time < \"2018-03-28T14:50:00Z\")`
     *          --> The backup name contains the string "howl" and start_time
     *              of the backup is before 2018-03-28T14:50:00Z.
     *       * `size_bytes > 10000000000` --> The backup's size is greater than
     *          10GB
     */
    ListBackupsParams& set_filter(std::string const& filter) {
      this->filter = filter;
      return *this;
    }

    /**
     * Sets the ordering expression.
     *
     * @param order_by expression for specifying the sort order of the results
     *     of the request. The string value should specify only one field in
     *     `google::bigtable::admin::v2::Backup`.
     *     The following field names are supported:
     *        * name
     *        * table
     *        * expire_time
     *        * start_time
     *        * end_time
     *        * size_bytes
     *        * state
     *
     *     For example, "start_time". The default sorting order is ascending.
     *     Append the " desc" suffix to the field name to sort descending, e.g.
     *     "start_time desc". Redundant space characters in the syntax are
     *     insignificant.
     *
     *     If order_by is empty, results will be sorted by `start_time` in
     *     descending order starting from the most recently created backup.
     */
    ListBackupsParams& set_order_by(std::string const& order_by) {
      this->order_by = order_by;
      return *this;
    }

    google::bigtable::admin::v2::ListBackupsRequest AsProto(
        std::string const& instance_name) const;

    absl::optional<std::string> cluster_id;
    absl::optional<std::string> filter;
    absl::optional<std::string> order_by;
  };

  /**
   * Retrieves a list of backups.
   *
   * @param params instance of `ListBackupsParams`.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc list backups
   */
  StatusOr<std::vector<google::bigtable::admin::v2::Backup>> ListBackups(
      ListBackupsParams const& params);

  /**
   * Sends an asynchronous request to retrieve a list of backups.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param params instance of `ListBackupsParams`.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second case, the future is satisfied
   *   with an exception.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_async_snippets.cc async list backups
   *
   */
  future<StatusOr<std::vector<google::bigtable::admin::v2::Backup>>>
  AsyncListBackups(CompletionQueue& cq, ListBackupsParams const& params);

  /**
   * Parameters for `RestoreTable` and `AsyncRestoreTable`.
   *
   * @param table_id the name of the table relative to the instance managed by
   *     this object. The full table name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of this object.
   * @param cluster_id the name of the cluster relative to the instance managed
   *     by the `TableAdmin` object. The full cluster name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<cluster_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object.
   * @param backup_id the name of the backup relative to the cluster specified.
   *     The full backup name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<CLUSTER_ID>/backups/<backup_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient,
   *     INSTANCE_ID is the instance_id() of the `TableAdmin` object, and
   *     CLUSTER_ID is the cluster_id previously specified.
   */
  struct RestoreTableParams {
    RestoreTableParams() = default;
    RestoreTableParams(std::string table_id, std::string cluster_id,
                       std::string backup_id)
        : table_id(std::move(table_id)),
          cluster_id(std::move(cluster_id)),
          backup_id(std::move(backup_id)) {}

    google::bigtable::admin::v2::RestoreTableRequest AsProto(
        std::string const& instance_name) const;

    std::string table_id;
    std::string cluster_id;
    std::string backup_id;
  };

  /**
   * Restore a backup into a new table in the instance.
   *
   * @param params instance of `RestoreTableParams`.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc restore table
   */
  StatusOr<google::bigtable::admin::v2::Table> RestoreTable(
      RestoreTableParams const& params);

  /**
   * Sends an asynchronous request to restore a backup into a new table in the
   * instance.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param params instance of `RestoreTableParams`.
   *
   * @return a future that will be satisfied when the request succeeds or the
   *   retry policy expires. In the first case, the future will contain the
   *   response from the service. In the second case, the future is satisfied
   *   with an exception.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_async_snippets.cc async restore table
   *
   */
  future<StatusOr<google::bigtable::admin::v2::Table>> AsyncRestoreTable(
      CompletionQueue& cq, RestoreTableParams const& params);

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

  /**
   * Gets the policy for @p table_id.
   *
   * @param table_id the table to query.
   * @return google::iam::v1::Policy the full IAM policy for the table.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_iam_policy_snippets.cc get iam policy
   */
  StatusOr<google::iam::v1::Policy> GetIamPolicy(std::string const& table_id);

  /**
   * Asynchronously gets the IAM policy for @p table_id.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param table_id the instance to query.
   * @return a future satisfied when either (a) the policy is fetched or (b)
   *     an unretriable error occurs or (c) retry policy has been exhausted.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_iam_policy_snippets.cc async get iam policy
   */
  future<StatusOr<google::iam::v1::Policy>> AsyncGetIamPolicy(
      CompletionQueue& cq, std::string const& table_id);

  /**
   * Sets the IAM policy for a table.
   *
   * This is the preferred way to the overload for `IamBindings`. This is more
   * closely coupled to the underlying protocol, enable more actions and is more
   * likely to tolerate future protocol changes.
   *
   * @param table_id which table to set the IAM policy for.
   * @param iam_policy google::iam::v1::Policy object containing role and
   * members.
   * @return google::iam::v1::Policy the current IAM policy for the table.
   *
   * @warning ETags are currently not used by Cloud Bigtable.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_iam_policy_snippets.cc set iam policy
   */
  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      std::string const& table_id, google::iam::v1::Policy const& iam_policy);

  /**
   * Asynchronously sets the IAM policy for a table.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param table_id which instance to set the IAM policy for.
   * @param iam_policy google::iam::v1::Policy object containing role and
   * members.
   * @return a future satisfied when either (a) the policy is created or (b)
   *     an unretriable error occurs or (c) retry policy has been
   *     exhausted.
   *
   * @warning ETags are currently not used by Cloud Bigtable.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet table_admin_iam_policy_snippets.cc async set iam policy
   */
  future<StatusOr<google::iam::v1::Policy>> AsyncSetIamPolicy(
      CompletionQueue& cq, std::string const& table_id,
      google::iam::v1::Policy const& iam_policy);

  /**
   * Returns a permission set that the caller has on the specified table.
   *
   * @param table_id the ID of the table to query.
   * @param permissions set of permissions to check for the resource.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Example
   * @snippet table_admin_iam_policy_snippets.cc test iam permissions
   *
   * @see https://cloud.google.com/bigtable/docs/access-control for a list of
   *     valid permissions on Google Cloud Bigtable.
   */
  StatusOr<std::vector<std::string>> TestIamPermissions(
      std::string const& table_id, std::vector<std::string> const& permissions);

  /**
   * Asynchronously obtains a permission set that the caller has on the
   * specified table.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param table_id the ID of the table to query.
   * @param permissions set of permissions to check for the resource.
   *
   * @par Example
   * @snippet table_admin_iam_policy_snippets.cc async test iam permissions
   *
   * @see https://cloud.google.com/bigtable/docs/access-control for a list of
   *     valid permissions on Google Cloud Bigtable.
   */
  future<StatusOr<std::vector<std::string>>> AsyncTestIamPermissions(
      CompletionQueue& cq, std::string const& table_id,
      std::vector<std::string> const& permissions);

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

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_ADMIN_H
