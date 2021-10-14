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
#include "google/cloud/bigtable/resource_names.h"
#include "google/cloud/bigtable/table_config.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/iam_policy.h"
#include "google/cloud/internal/attributes.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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
 * threads is guaranteed to work. Two threads operating concurrently on the same
 * instance of this class is not guaranteed to work.
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
 * error Please consult the [`StatusOr<T>`
 * documentation](#google::cloud::StatusOr) for more details.
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
 * @see #google::cloud::StatusOr for a description of the error reporting class
 *     used by this library.
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
            DefaultPollingPolicy(internal::kBigtableTableAdminLimits)),
        background_threads_(client_->BackgroundThreadsFactory()()) {}

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
  using TableView = ::google::bigtable::admin::v2::Table::View;
  /// Only populate 'name' and fields related to the table's encryption state.
  static auto constexpr ENCRYPTION_VIEW =  // NOLINT(readability-identifier-naming)
      google::bigtable::admin::v2::Table::ENCRYPTION_VIEW;
  /// Populate all the fields in the response.
  static auto constexpr FULL =  // NOLINT(readability-identifier-naming)
      google::bigtable::admin::v2::Table::FULL;
  /// Populate only the name in the responses.
  static auto constexpr NAME_ONLY =  // NOLINT(readability-identifier-naming)
      google::bigtable::admin::v2::Table::NAME_ONLY;
  /// Populate only the name and the fields related to the table replication
  /// state.
  static auto constexpr REPLICATION_VIEW =  // NOLINT(readability-identifier-naming)
      google::bigtable::admin::v2::Table::REPLICATION_VIEW;
  /// Populate only the name and the fields related to the table schema.
  static auto constexpr SCHEMA_VIEW =  // NOLINT(readability-identifier-naming)
      google::bigtable::admin::v2::Table::SCHEMA_VIEW;
  /// Use the default view as defined for each function.
  static auto constexpr VIEW_UNSPECIFIED =  // NOLINT(readability-identifier-naming)
      google::bigtable::admin::v2::Table::VIEW_UNSPECIFIED;
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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_snippets.cc create table
   */
  StatusOr<::google::bigtable::admin::v2::Table> CreateTable(
      std::string table_id, TableConfig config);

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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_snippets.cc list tables
   */
  StatusOr<std::vector<::google::bigtable::admin::v2::Table>> ListTables(
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
   * @return the information about the table or status.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_snippets.cc get table
   */
  StatusOr<::google::bigtable::admin::v2::Table> GetTable(
      std::string const& table_id, TableView view = SCHEMA_VIEW);

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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_snippets.cc delete table
   */
  Status DeleteTable(std::string const& table_id);

  /**
   * Parameters for `CreateBackup`.
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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc create backup
   */
  StatusOr<google::bigtable::admin::v2::Backup> CreateBackup(
      CreateBackupParams const& params);

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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc get backup
   */
  StatusOr<google::bigtable::admin::v2::Backup> GetBackup(
      std::string const& cluster_id, std::string const& backup_id);

  /**
   * Parameters for `UpdateBackup`.
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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc update backup
   */
  StatusOr<google::bigtable::admin::v2::Backup> UpdateBackup(
      UpdateBackupParams const& params);

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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc delete backup
   */
  Status DeleteBackup(google::bigtable::admin::v2::Backup const& backup);

  /**
   * Parameters for `ListBackups`.
   */
  struct ListBackupsParams {
    ListBackupsParams() = default;

    /**
     * Sets the cluster_id.
     *
     * @param c the name of the cluster relative to the instance
     *     managed by the `TableAdmin` object. If no cluster_id is specified,
     *     the all backups in all clusters are listed. The full cluster name is
     *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/clusters/<cluster_id>`
     *     where PROJECT_ID is obtained from the associated AdminClient and
     *     INSTANCE_ID is the instance_id() of the `TableAdmin` object.
     */
    ListBackupsParams& set_cluster(std::string c) {
      this->cluster_id = std::move(c);
      return *this;
    }

    /**
     * Sets the filtering expression.
     *
     * @param f expression that filters backups listed in the response.
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
     *       * `start_time` (and values are of the format
     *              `YYYY-MM-DDTHH:MM:SSZ`)
     *       * `end_time` (and values are of the format `YYYY-MM-DDTHH:MM:SSZ`)
     *       * `expire_time` (and values are of the format
     *              `YYYY-MM-DDTHH:MM:SSZ`)
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
     *              of the backup is before `2018-03-28T14:50:00Z`.
     *       * `size_bytes > 10000000000` --> The backup's size is greater than
     *          10GB
     */
    ListBackupsParams& set_filter(std::string f) {
      this->filter = std::move(f);
      return *this;
    }

    /**
     * Sets the ordering expression.
     *
     * @param o expression for specifying the sort order of the results
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
    ListBackupsParams& set_order_by(std::string o) {
      this->order_by = std::move(o);
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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc list backups
   */
  StatusOr<std::vector<google::bigtable::admin::v2::Backup>> ListBackups(
      ListBackupsParams const& params);

  /**
   * Parameters for `RestoreTable`.
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

    /// @deprecated covert the parameters to a proto.
    google::bigtable::admin::v2::RestoreTableRequest AsProto(
        std::string const& instance_name) const;

    std::string table_id;
    std::string cluster_id;
    std::string backup_id;
  };

  /**
   * Parameters for `RestoreTable`.
   *
   * @param table_id the name of the table relative to the instance managed by
   *     this object. The full table name is
   *     `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
   *     where PROJECT_ID is obtained from the associated AdminClient and
   *     INSTANCE_ID is the instance_id() of this object.
   * @param backup_name the full name of the backup used to restore @p table_id.
   */
  struct RestoreTableFromInstanceParams {
    std::string table_id;
    std::string backup_name;
  };

  /**
   * Restore a backup into a new table in the instance.
   *
   * @param params instance of `RestoreTableParams`.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc restore table
   */
  StatusOr<google::bigtable::admin::v2::Table> RestoreTable(
      RestoreTableParams const& params);

  /**
   * Restore a backup into a new table in the instance.
   *
   * @param params instance of `RestoreTableFromInstanceParams`.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc restore2
   */
  StatusOr<google::bigtable::admin::v2::Table> RestoreTable(
      RestoreTableFromInstanceParams params);

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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_snippets.cc modify table
   */
  StatusOr<::google::bigtable::admin::v2::Table> ModifyColumnFamilies(
      std::string const& table_id,
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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_snippets.cc drop rows by prefix
   */
  Status DropRowsByPrefix(std::string const& table_id,
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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_snippets.cc generate consistency token
   */
  StatusOr<std::string> GenerateConsistencyToken(std::string const& table_id);

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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_snippets.cc check consistency
   */
  StatusOr<Consistency> CheckConsistency(std::string const& table_id,
                                         std::string const& consistency_token);

  /**
   * Checks consistency of a table with multiple calls using a separate thread
   *
   * @param table_id the id of the table for which we want to check
   *     consistency.
   * @param consistency_token the consistency token of the table.
   * @return the consistency status for the table.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_snippets.cc wait for consistency check
   */
  google::cloud::future<StatusOr<Consistency>> WaitForConsistency(
      std::string const& table_id, std::string const& consistency_token);

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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_snippets.cc drop all rows
   */
  Status DropAllRows(std::string const& table_id);

  /**
   * Gets the policy for @p table_id.
   *
   * @param table_id the table to query.
   * @return google::iam::v1::Policy the full IAM policy for the table.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_iam_policy_snippets.cc get iam policy
   */
  StatusOr<google::iam::v1::Policy> GetIamPolicy(std::string const& table_id);

  /**
   * Gets the policy for @p backup_id.
   *
   * @param cluster_id the associated cluster that contains backup.
   *
   * @param backup_id the backup to query.
   * @return google::iam::v1::Policy the full IAM policy for the backup.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc get backup iam policy
   */
  StatusOr<google::iam::v1::Policy> GetIamPolicy(std::string const& cluster_id,
                                                 std::string const& backup_id);

  /**
   * Sets the IAM policy for a table.
   *
   * This is the preferred way to overload `IamBindings`. This is more closely
   * coupled to the underlying protocol, enable more actions and is more likely
   * to tolerate future protocol changes.
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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_iam_policy_snippets.cc set iam policy
   */
  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      std::string const& table_id, google::iam::v1::Policy const& iam_policy);

  /**
   * Sets the IAM policy for a backup.
   *
   * This is the preferred way to overload `IamBindings`. This is more closely
   * coupled to the underlying protocol, enable more actions and is more likely
   * to tolerate future protocol changes.
   *
   * @param cluster_id which is the cluster containing the backup.
   * @param backup_id which backup to set the IAM policy for.
   * @param iam_policy google::iam::v1::Policy object containing role and
   * members.
   * @return google::iam::v1::Policy the current IAM policy for the table.
   *
   * @warning ETags are currently not used by Cloud Bigtable.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet bigtable_table_admin_backup_snippets.cc set backup iam policy
   */
  StatusOr<google::iam::v1::Policy> SetIamPolicy(
      std::string const& cluster_id, std::string const& backup_id,
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
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
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
   * Returns a permission set that the caller has on the specified backup.
   *
   * @param cluster_id the ID of the cluster that contains the backup.
   * @param backup_id the ID of the backup to query.
   * @param permissions set of permissions to check for the resource.
   *
   * @par Idempotency
   * This operation is read-only and therefore it is always idempotent.
   *
   * @par Thread-safety
   * Two threads concurrently calling this member function on the same instance
   * of this class are **not** guaranteed to work. Consider copying the object
   * and using different copies in each thread.
   *
   * @par Example
   * @snippet table_admin_iam_policy_snippets.cc test iam permissions
   *
   * @see https://cloud.google.com/bigtable/docs/access-control for a list of
   *     valid permissions on Google Cloud Bigtable.
   */
  StatusOr<std::vector<std::string>> TestIamPermissions(
      std::string const& cluster_id, std::string const& backup_id,
      std::vector<std::string> const& permissions);

  /// Return the fully qualified name of a table in this object's instance.
  std::string TableName(std::string const& table_id) const {
    return google::cloud::bigtable::TableName(project(), instance_id(),
                                              table_id);
  }

  /// Return the fully qualified name of a Cluster.
  std::string ClusterName(std::string const& cluster_id) const {
    return google::cloud::bigtable::ClusterName(project(), instance_id(),
                                                cluster_id);
  }

  /// Return the fully qualified name of a Backup.
  std::string BackupName(std::string const& cluster_id,
                         std::string const& backup_id) const {
    return google::cloud::bigtable::BackupName(project(), instance_id(),
                                               cluster_id, backup_id);
  }

 private:
  friend class TableAdminTester;

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

  future<StatusOr<google::bigtable::admin::v2::Backup>> AsyncCreateBackupImpl(
      CompletionQueue& cq, CreateBackupParams const& params);

  future<StatusOr<google::bigtable::admin::v2::Table>> AsyncRestoreTableImpl(
      CompletionQueue& cq, RestoreTableParams const& params);

  future<StatusOr<google::bigtable::admin::v2::Table>> AsyncRestoreTableImpl(
      CompletionQueue& cq, RestoreTableFromInstanceParams params);

  future<StatusOr<Consistency>> AsyncCheckConsistency(
      CompletionQueue& cq, std::string const& table_id,
      std::string const& consistency_token);

  future<StatusOr<Consistency>> AsyncWaitForConsistencyImpl(
      CompletionQueue& cq, std::string const& table_id,
      std::string const& consistency_token);

  std::shared_ptr<AdminClient> client_;
  std::string instance_id_;
  std::string instance_name_;
  std::shared_ptr<RPCRetryPolicy const> rpc_retry_policy_prototype_;
  std::shared_ptr<RPCBackoffPolicy const> rpc_backoff_policy_prototype_;
  bigtable::MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<PollingPolicy const> polling_policy_prototype_;
  std::shared_ptr<BackgroundThreads> background_threads_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_ADMIN_H
