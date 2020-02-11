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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_H

#include "google/cloud/bigtable/async_row_reader.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/filters.h"
#include "google/cloud/bigtable/idempotent_mutation_policy.h"
#include "google/cloud/bigtable/mutations.h"
#include "google/cloud/bigtable/read_modify_write_rule.h"
#include "google/cloud/bigtable/row_key_sample.h"
#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/row_set.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/disjunction.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/// The branch taken by a Table::CheckAndMutateRow operation.
enum class MutationBranch {
  /// The predicate provided to CheckAndMutateRow did not match and the
  /// `false_mutations` (if any) were applied.
  kPredicateNotMatched,
  /// The predicate provided to CheckAndMutateRow matched and the
  /// `true_mutations` (if any) were applied.
  kPredicateMatched,
};

class MutationBatcher;

/**
 * Return the full table name.
 *
 * The full table name is:
 *
 * `projects/<PROJECT_ID>/instances/<INSTANCE_ID>/tables/<table_id>`
 *
 * Where the project id and instance id come from the @p client parameter.
 */
inline std::string TableName(std::shared_ptr<DataClient> client,
                             std::string const& table_id) {
  return InstanceName(std::move(client)) + "/tables/" + table_id;
}

/**
 * The main interface to interact with data in a Cloud Bigtable table.
 *
 * This class provides member functions to:
 * - read specific rows: `Table::ReadRow()`
 * - scan a ranges of rows: `Table::ReadRows()`
 * - update or create a single row: `Table::Apply()`
 * - update or modify multiple rows: `Table::BulkApply()`
 * - update a row based on previous values: `Table::CheckAndMutateRow()`
 * - to atomically append data and/or increment multiple values in a row:
 *   `Table::ReadModifyWriteRow()`
 * - to sample the row keys: `Table::SampleRows()`.
 *
 * The class deals with the most common transient failures, and retries the
 * underlying RPC calls subject to the policies configured by the application.
 * These policies are documented in `Table::Table()`.
 *
 * @par Thread-safety
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Cost
 * Creating a new object of type `Table` is comparable to creating a few objects
 * of type `std::string` or a few objects of type `std::shared_ptr<int>`. The
 * class represents a shallow handle to a remote object.
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
 * cbt::Table = ...;
 * google::cloud::StatusOr<std::pair<bool, cbt::Row>> row = table.ReadRow(...);
 *
 * if (!row) {
 *   std::cerr << "Error reading row\n";
 *   return;
 * }
 *
 * // Use "row" as a smart pointer here, e.g.:
 * if (!row->first) {
 *   std::cout << "Contacting the server was successful, but the row does not"
 *             << " exist\n";
 *   return;
 * }
 * std::cout << "The row has " << row->second.cells().size() << " cells\n";
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
class Table {
 private:
  // We need to eliminate some function overloads from resolution, and that
  // requires a bit of infrastructure in the private section.

  /// A meta function to check if @p P is a valid Policy type.
  template <typename P>
  struct valid_policy : google::cloud::internal::disjunction<
                            std::is_base_of<RPCBackoffPolicy, P>,
                            std::is_base_of<RPCRetryPolicy, P>,
                            std::is_base_of<IdempotentMutationPolicy, P>> {};

  /// A meta function to check if all the @p Policies are valid policy types.
  template <typename... Policies>
  struct valid_policies : internal::conjunction<valid_policy<Policies>...> {};

 public:
  /**
   * Constructor with default policies.
   *
   * @param client how to communicate with Cloud Bigtable, including
   *     credentials, the project id, and the instance id.
   * @param table_id the table id within the instance defined by client.  The
   *     full table name is `client->instance_name() + '/tables/' + table_id`.
   */
  Table(std::shared_ptr<DataClient> client, std::string const& table_id)
      : Table(std::move(client), std::string{}, table_id) {}

  /**
   * Constructor with default policies.
   *
   * @param client how to communicate with Cloud Bigtable, including
   *     credentials, the project id, and the instance id.
   * @param app_profile_id the app_profile_id needed for using the replication
   * API.
   * @param table_id the table id within the instance defined by client.  The
   *     full table name is `client->instance_name() + '/tables/' + table_id`.
   *
   * @par Example
   * @snippet bigtable_hello_app_profile.cc cbt namespace
   *
   * @par Example Using AppProfile
   * @snippet bigtable_hello_app_profile.cc read with app profile
   */
  Table(std::shared_ptr<DataClient> client, std::string app_profile_id,
        std::string const& table_id)
      : client_(std::move(client)),
        app_profile_id_(std::move(app_profile_id)),
        table_name_(TableName(client_, table_id)),
        table_id_(table_id),
        rpc_retry_policy_prototype_(
            bigtable::DefaultRPCRetryPolicy(internal::kBigtableLimits)),
        rpc_backoff_policy_prototype_(
            bigtable::DefaultRPCBackoffPolicy(internal::kBigtableLimits)),
        metadata_update_policy_(
            MetadataUpdatePolicy(table_name_, MetadataParamTypes::TABLE_NAME)),
        idempotent_mutation_policy_(
            bigtable::DefaultIdempotentMutationPolicy()) {}

  /**
   * Constructor with explicit policies.
   *
   * The policies are passed by value, because this makes it easy for
   * applications to create them.
   *
   * @par Example
   * @code
   * using namespace std::chrono_literals; // assuming C++14.
   * auto client = bigtable::CreateDefaultClient(...); // details ommitted
   * bigtable::Table table(client, "my-table",
   *                       // Allow up to 20 minutes to retry operations
   *                       bigtable::LimitedTimeRetryPolicy(20min),
   *                       // Start with 50 milliseconds backoff, grow
   *                       // exponentially to 5 minutes.
   *                       bigtable::ExponentialBackoffPolicy(50ms, 5min),
   *                       // Only retry idempotent mutations.
   *                       bigtable::SafeIdempotentMutationPolicy());
   * @endcode
   *
   * @param client how to communicate with Cloud Bigtable, including
   *     credentials, the project id, and the instance id.
   * @param table_id the table id within the instance defined by client.  The
   *     full table name is `client->instance_name() + "/tables/" + table_id`.
   * @param policies the set of policy overrides for this object.
   * @tparam Policies the types of the policies to override, the types must
   *     derive from one of the following types:
   *
   *     - `IdempotentMutationPolicy` which mutations are retried. Use
   *       `SafeIdempotentMutationPolicy` to only retry idempotent operations,
   *       use `AlwaysRetryMutationPolicy` to retry all operations. Read the
   *       caveats in the class definition to understand the downsides of the
   *       latter. You can also create your own policies that decide which
   *       mutations to retry.
   *     - `RPCBackoffPolicy` how to backoff from a failed RPC. Currently only
   *       `ExponentialBackoffPolicy` is implemented. You can also create your
   *       own policies that backoff using a different algorithm.
   *     - `RPCRetryPolicy` for how long to retry failed RPCs. Use
   *       `LimitedErrorCountRetryPolicy` to limit the number of failures
   *       allowed. Use `LimitedTimeRetryPolicy` to bound the time for any
   *       request. You can also create your own policies that combine time and
   *       error counts.
   *
   * @see SafeIdempotentMutationPolicy, AlwaysRetryMutationPolicy,
   *     ExponentialBackoffPolicy, LimitedErrorCountRetryPolicy,
   *     LimitedTimeRetryPolicy.
   *
   * @par Idempotency Policy Example
   * @snippet data_snippets.cc apply relaxed idempotency
   *
   * @par Modified Retry Policy Example
   * @snippet data_snippets.cc apply custom retry
   */
  template <typename... Policies,
            typename std::enable_if<valid_policies<Policies...>::value,
                                    int>::type = 0>
  Table(std::shared_ptr<DataClient> client, std::string const& table_id,
        Policies&&... policies)
      : Table(std::move(client), table_id) {
    ChangePolicies(std::forward<Policies>(policies)...);
  }

  /**
   * Constructor with explicit policies.
   *
   * The policies are passed by value, because this makes it easy for
   * applications to create them.
   *
   * @par Example
   * @code
   * using namespace std::chrono_literals; // assuming C++14.
   * auto client = bigtable::CreateDefaultClient(...); // details ommitted
   * bigtable::Table table(client, "app_id", "my-table",
   *                       // Allow up to 20 minutes to retry operations
   *                       bigtable::LimitedTimeRetryPolicy(20min),
   *                       // Start with 50 milliseconds backoff, grow
   *                       // exponentially to 5 minutes.
   *                       bigtable::ExponentialBackoffPolicy(50ms, 5min),
   *                       // Only retry idempotent mutations.
   *                       bigtable::SafeIdempotentMutationPolicy());
   * @endcode
   *
   * @param client how to communicate with Cloud Bigtable, including
   *     credentials, the project id, and the instance id.
   * @param app_profile_id the app_profile_id needed for using the replication
   * API.
   * @param table_id the table id within the instance defined by client.  The
   *     full table name is `client->instance_name() + "/tables/" + table_id`.
   * @param policies the set of policy overrides for this object.
   * @tparam Policies the types of the policies to override, the types must
   *     derive from one of the following types:
   *     - `IdempotentMutationPolicy` which mutations are retried. Use
   *       `SafeIdempotentMutationPolicy` to only retry idempotent operations,
   *       use `AlwaysRetryMutationPolicy` to retry all operations. Read the
   *       caveats in the class definition to understand the downsides of the
   *       latter. You can also create your own policies that decide which
   *       mutations to retry.
   *     - `RPCBackoffPolicy` how to backoff from a failed RPC. Currently only
   *       `ExponentialBackoffPolicy` is implemented. You can also create your
   *       own policies that backoff using a different algorithm.
   *     - `RPCRetryPolicy` for how long to retry failed RPCs. Use
   *       `LimitedErrorCountRetryPolicy` to limit the number of failures
   *       allowed. Use `LimitedTimeRetryPolicy` to bound the time for any
   *       request. You can also create your own policies that combine time and
   *       error counts.
   *
   * @see SafeIdempotentMutationPolicy, AlwaysRetryMutationPolicy,
   *     ExponentialBackoffPolicy, LimitedErrorCountRetryPolicy,
   *     LimitedTimeRetryPolicy.
   *
   * @par Idempotency Policy Example
   * @snippet data_snippets.cc apply relaxed idempotency
   *
   * @par Modified Retry Policy Example
   * @snippet data_snippets.cc apply custom retry
   */
  template <typename... Policies,
            typename std::enable_if<valid_policies<Policies...>::value,
                                    int>::type = 0>
  Table(std::shared_ptr<DataClient> client, std::string app_profile_id,
        std::string const& table_id, Policies&&... policies)
      : Table(std::move(client), std::move(app_profile_id), table_id) {
    ChangePolicies(std::forward<Policies>(policies)...);
  }

  std::string const& table_name() const { return table_name_; }
  std::string const& app_profile_id() const { return app_profile_id_; }
  std::string const& project_id() const { return client_->project_id(); }
  std::string const& instance_id() const { return client_->instance_id(); }
  std::string const& table_id() const { return table_id_; }

  /**
   * Attempts to apply the mutation to a row.
   *
   * @param mut the mutation. Note that this function takes ownership (and
   *     then discards) the data in the mutation.  In general, a
   *     `SingleRowMutation` can be used to modify and/or delete multiple cells,
   *     across different columns and column families.
   *
   * @return status of the operation.
   *
   * @par Idempotency
   * This operation is idempotent if the provided mutations are idempotent. Note
   * that `google::cloud::bigtable::SetCell()` without an explicit timestamp is
   * **not** an idempotent operation.
   *
   * @par Example
   * @snippet data_snippets.cc apply
   */
  Status Apply(SingleRowMutation mut);

  /**
   * Makes asynchronous attempts to apply the mutation to a row.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param mut the mutation. Note that this function takes ownership
   * (and then discards) the data in the mutation.  In general, a
   *     `SingleRowMutation` can be used to modify and/or delete
   * multiple cells, across different columns and column families.
   * @param cq the completion queue that will execute the asynchronous
   *    calls, the application must ensure that one or more threads are
   *    blocked on `cq.Run()`.
   *
   * @par Idempotency
   * This operation is idempotent if the provided mutations are idempotent. Note
   * that `google::cloud::bigtable::SetCell()` without an explicit timestamp is
   * **not** an idempotent operation.
   *
   * @par Example
   * @snippet data_async_snippets.cc async-apply
   */

  future<Status> AsyncApply(SingleRowMutation mut, CompletionQueue& cq);

  /**
   * Attempts to apply mutations to multiple rows.
   *
   * @param mut the mutations, note that this function takes
   *     ownership (and then discards) the data in the mutation. In general, a
   *     `BulkMutation` can modify multiple rows, and the modifications for each
   *     row can change (or create) multiple cells, across different columns and
   *     column families.
   *
   * @par Idempotency
   * This operation is idempotent if the provided mutations are idempotent. Note
   * that `google::cloud::bigtable::SetCell()` without an explicit timestamp is
   * **not** an idempotent operation.
   *
   * @par Example
   * @snippet data_snippets.cc bulk apply
   */
  std::vector<FailedMutation> BulkApply(BulkMutation mut);

  /**
   * Makes asynchronous attempts to apply mutations to multiple rows.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param mut the mutations, note that this function takes
   *     ownership (and then discards) the data in the mutation. In general, a
   *     `BulkMutation` can modify multiple rows, and the modifications for each
   *     row can change (or create) multiple cells, across different columns and
   *     column families.
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   *
   * @par Idempotency
   * This operation is idempotent if the provided mutations are idempotent. Note
   * that `google::cloud::bigtable::SetCell()` without an explicit timestamp is
   * **not** an idempotent operation.
   *
   * @par Example
   * @snippet data_async_snippets.cc bulk async-bulk-apply
   */
  future<std::vector<FailedMutation>> AsyncBulkApply(BulkMutation mut,
                                                     CompletionQueue& cq);

  /**
   * Reads a set of rows from the table.
   *
   * @param row_set the rows to read from.
   * @param filter is applied on the server-side to data in the rows.
   *
   * @par Idempotency
   * This is a read-only operation and therefore it is always idempotent.
   *
   * @par Example
   * @snippet data_snippets.cc read rows
   */
  RowReader ReadRows(RowSet row_set, Filter filter);

  /**
   * Reads a limited set of rows from the table.
   *
   * @param row_set the rows to read from.
   * @param rows_limit the maximum number of rows to read. Cannot be a negative
   *     number or zero. Use `ReadRows(RowSet, Filter)` to read all matching
   *     rows.
   * @param filter is applied on the server-side to data in the rows.
   *
   * @par Idempotency
   * This is a read-only operation and therefore it is always idempotent.
   *
   * @par Example
   * @snippet data_snippets.cc read rows with limit
   */
  RowReader ReadRows(RowSet row_set, std::int64_t rows_limit, Filter filter);

  /**
   * Read and return a single row from the table.
   *
   * @param row_key the row to read.
   * @param filter a filter expression, can be used to select a subset of the
   *     column families and columns in the row.
   * @returns a tuple, the first element is a boolean, with value `false` if the
   *     row does not exist.  If the first element is `true` the second element
   *     has the contents of the Row.  Note that the contents may be empty
   *     if the filter expression removes all column families and columns.
   *
   * @par Idempotency
   * This is a read-only operation and therefore it is always idempotent.
   *
   * @par Example
   * @snippet data_snippets.cc read row
   */
  StatusOr<std::pair<bool, Row>> ReadRow(std::string row_key, Filter filter);

  /**
   * Atomic test-and-set for a row using filter expressions.
   *
   * Atomically check the value of a row using a filter expression.  If the
   * expression passes (meaning at least one element is returned by it), one
   * set of mutations is applied.  If the filter does not pass, a different set
   * of mutations is applied.  The changes are atomically applied in the server.
   *
   * @param row_key the row to modify.
   * @param filter the filter expression.
   * @param true_mutations the mutations for the "filter passed" case.
   * @param false_mutations the mutations for the "filter did not pass" case.
   * @returns true if the filter passed.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Check for Value Example
   * @snippet data_snippets.cc check and mutate
   *
   * @par Check for Cell Presence Example
   * @snippet data_snippets.cc check and mutate not present
   */
  StatusOr<MutationBranch> CheckAndMutateRow(
      std::string row_key, Filter filter, std::vector<Mutation> true_mutations,
      std::vector<Mutation> false_mutations);

  /**
   * Make an asynchronous request to conditionally mutate a row.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param row_key the row key on which the conditional mutation will be
   *     performed
   * @param filter the condition, depending on which the mutation will be
   *     performed
   * @param true_mutations the mutations which will be performed if @p filter is
   *     true
   * @param false_mutations the mutations which will be performed if @p filter
   *     is false
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet data_async_snippets.cc async check and mutate
   */
  future<StatusOr<MutationBranch>> AsyncCheckAndMutateRow(
      std::string row_key, Filter filter, std::vector<Mutation> true_mutations,
      std::vector<Mutation> false_mutations, CompletionQueue& cq);

  /**
   * Sample of the row keys in the table, including approximate data sizes.
   *
   * @returns Note that the sample may only include one element for small
   *     tables.  In addition, the sample may include row keys that do not exist
   *     on the table, and may include the empty row key to indicate
   *     "end of table".
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Examples
   * @snippet data_snippets.cc sample row keys
   */
  StatusOr<std::vector<bigtable::RowKeySample>> SampleRows();

  /**
   * Atomically read and modify the row in the server, returning the
   * resulting row
   *
   * @tparam Args this is zero or more ReadModifyWriteRules to apply on a row
   * @param row_key the row to read
   * @param rule to modify the row. Two types of rules are applied here
   *     AppendValue which will read the existing value and append the
   *     text provided to the value.
   *     IncrementAmount which will read the existing uint64 big-endian-int
   *     and add the value provided.
   *     Both rules accept the family and column identifier to modify.
   * @param rules is the zero or more ReadModifyWriteRules to apply on a row.
   * @returns The new contents of all modified cells.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet data_snippets.cc read modify write
   */
  template <typename... Args>
  StatusOr<Row> ReadModifyWriteRow(std::string row_key,
                                   bigtable::ReadModifyWriteRule rule,
                                   Args&&... rules) {
    grpc::Status status;

    ::google::bigtable::v2::ReadModifyWriteRowRequest request;
    request.set_row_key(std::move(row_key));

    // Generate a better compile time error message than the default one
    // if the types do not match
    static_assert(
        bigtable::internal::conjunction<
            std::is_convertible<Args, bigtable::ReadModifyWriteRule>...>::value,
        "The arguments passed to ReadModifyWriteRow(row_key,...) must be "
        "convertible to bigtable::ReadModifyWriteRule");

    *request.add_rules() = std::move(rule).as_proto();
    AddRules(request, std::forward<Args>(rules)...);
    return ReadModifyWriteRowImpl(std::move(request));
  }

  /**
   * Make an asynchronous request to atomically read and modify a row.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param row_key the row key on which modification will be performed
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   *
   * @param rule to modify the row. Two types of rules are applied here
   *     AppendValue which will read the existing value and append the
   *     text provided to the value.
   *     IncrementAmount which will read the existing uint64 big-endian-int
   *     and add the value provided.
   *     Both rules accept the family and column identifier to modify.
   * @param rules is the zero or more ReadModifyWriteRules to apply on a row.
   * @returns A future, that becomes satisfied when the operation completes,
   *     at that point the future has the contents of all modified cells.
   *
   * @par Idempotency
   * This operation is always treated as non-idempotent.
   *
   * @par Example
   * @snippet data_async_snippets.cc async read modify write
   */
  template <typename... Args>
  future<StatusOr<Row>> AsyncReadModifyWriteRow(
      std::string row_key, CompletionQueue& cq,
      bigtable::ReadModifyWriteRule rule, Args&&... rules) {
    ::google::bigtable::v2::ReadModifyWriteRowRequest request;
    request.set_row_key(std::move(row_key));
    *request.add_rules() = std::move(rule).as_proto();
    AddRules(request, std::forward<Args>(rules)...);

    return AsyncReadModifyWriteRowImpl(cq, std::move(request));
  }

  /**
   * Asynchronously reads a set of rows from the table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param on_row the callback to be invoked on each successfully read row; it
   *     should be invocable with `Row` and return a future<bool>; the returned
   *     `future<bool>` should be satisfied with `true` when the user is ready
   *     to receive the next callback and with `false` when the user doesn't
   *     want any more rows; if `on_row` throws, the results are undefined
   * @param on_finish the callback to be invoked when the stream is closed; it
   *     should be invocable with `Status` and not return anything; it will
   *     always be called as the last callback; if `on_finish` throws, the
   *     results are undefined
   * @param row_set the rows to read from.
   * @param filter is applied on the server-side to data in the rows.
   *
   * @tparam RowFunctor the type of the @p on_row callback.
   * @tparam FinishFunctor the type of the @p on_finish callback.
   *
   * @par Example
   * @snippet data_async_snippets.cc async read rows
   */
  template <typename RowFunctor, typename FinishFunctor>
  void AsyncReadRows(CompletionQueue& cq, RowFunctor on_row,
                     FinishFunctor on_finish, RowSet row_set, Filter filter) {
    AsyncRowReader<RowFunctor, FinishFunctor>::Create(
        cq, client_, app_profile_id_, table_name_, std::move(on_row),
        std::move(on_finish), std::move(row_set),
        AsyncRowReader<RowFunctor, FinishFunctor>::NO_ROWS_LIMIT,
        std::move(filter), clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
        metadata_update_policy_,
        google::cloud::internal::make_unique<
            bigtable::internal::ReadRowsParserFactory>());
  }

  /**
   * Asynchronously reads a set of rows from the table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param on_row the callback to be invoked on each successfully read row; it
   *     should be invocable with `Row` and return a future<bool>; the returned
   *     `future<bool>` should be satisfied with `true` when the user is ready
   *     to receive the next callback and with `false` when the user doesn't
   *     want any more rows; if `on_row` throws, the results are undefined
   * @param on_finish the callback to be invoked when the stream is closed; it
   *     should be invocable with `Status` and not return anything; it will
   *     always be called as the last callback; if `on_finish` throws, the
   *     results are undefined
   * @param row_set the rows to read from.
   * @param rows_limit the maximum number of rows to read. Cannot be a negative
   *     number or zero. Use `AsyncReadRows(CompletionQueue, RowSet, Filter)` to
   *     read all matching rows.
   * @param filter is applied on the server-side to data in the rows.
   *
   * @tparam RowFunctor the type of the @p on_row callback.
   * @tparam FinishFunctor the type of the @p on_finish callback.
   *
   * @par Example
   * @snippet data_async_snippets.cc async read rows with limit
   */
  template <typename RowFunctor, typename FinishFunctor>
  void AsyncReadRows(CompletionQueue& cq, RowFunctor on_row,
                     FinishFunctor on_finish, RowSet row_set,
                     std::int64_t rows_limit, Filter filter) {
    AsyncRowReader<RowFunctor, FinishFunctor>::Create(
        cq, client_, app_profile_id_, table_name_, std::move(on_row),
        std::move(on_finish), std::move(row_set), rows_limit, std::move(filter),
        clone_rpc_retry_policy(), clone_rpc_backoff_policy(),
        metadata_update_policy_,
        google::cloud::internal::make_unique<
            bigtable::internal::ReadRowsParserFactory>());
  }

  /**
   * Asynchronously read and return a single row from the table.
   *
   * @warning This is an early version of the asynchronous APIs for Cloud
   *     Bigtable. These APIs might be changed in backward-incompatible ways. It
   *     is not subject to any SLA or deprecation policy.
   *
   * @param cq the completion queue that will execute the asynchronous calls,
   *     the application must ensure that one or more threads are blocked on
   *     `cq.Run()`.
   * @param row_key the row to read.
   * @param filter a filter expression, can be used to select a subset of the
   *     column families and columns in the row.
   * @returns a future satisfied when the operation completes, failes
   *     permanently or keeps failing transiently, but the retry policy has been
   *     exhausted. The future will return a tuple. The first element is a
   *     boolean, with value `false` if the row does not exist.  If the first
   *     element is `true` the second element has the contents of the Row.  Note
   *     that the contents may be empty if the filter expression removes all
   *     column families and columns.
   *
   * @par Idempotency
   * This is a read-only operation and therefore it is always idempotent.
   *
   * @par Example
   * @snippet data_async_snippets.cc async read row
   */
  future<StatusOr<std::pair<bool, Row>>> AsyncReadRow(CompletionQueue& cq,
                                                      std::string row_key,
                                                      Filter filter);

 private:
  /**
   * Send request ReadModifyWriteRowRequest to modify the row and get it back
   */
  StatusOr<Row> ReadModifyWriteRowImpl(
      ::google::bigtable::v2::ReadModifyWriteRowRequest request);

  future<StatusOr<Row>> AsyncReadModifyWriteRowImpl(
      CompletionQueue& cq,
      ::google::bigtable::v2::ReadModifyWriteRowRequest request);

  void AddRules(google::bigtable::v2::ReadModifyWriteRowRequest&) {
    // no-op for empty list
  }

  template <typename... Args>
  void AddRules(google::bigtable::v2::ReadModifyWriteRowRequest& request,
                bigtable::ReadModifyWriteRule rule, Args&&... args) {
    *request.add_rules() = std::move(rule).as_proto();
    AddRules(request, std::forward<Args>(args)...);
  }

  std::unique_ptr<RPCRetryPolicy> clone_rpc_retry_policy() {
    return rpc_retry_policy_prototype_->clone();
  }

  std::unique_ptr<RPCBackoffPolicy> clone_rpc_backoff_policy() {
    return rpc_backoff_policy_prototype_->clone();
  }

  MetadataUpdatePolicy clone_metadata_update_policy() {
    return metadata_update_policy_;
  }

  std::unique_ptr<IdempotentMutationPolicy> clone_idempotent_mutation_policy() {
    return idempotent_mutation_policy_->clone();
  }

  //@{
  /// @name Helper functions to implement constructors with changed policies.
  void ChangePolicy(RPCRetryPolicy const& policy) {
    rpc_retry_policy_prototype_ = policy.clone();
  }

  void ChangePolicy(RPCBackoffPolicy const& policy) {
    rpc_backoff_policy_prototype_ = policy.clone();
  }

  void ChangePolicy(IdempotentMutationPolicy const& policy) {
    idempotent_mutation_policy_ = policy.clone();
  }

  template <typename Policy, typename... Policies>
  void ChangePolicies(Policy&& policy, Policies&&... policies) {
    ChangePolicy(policy);
    ChangePolicies(std::forward<Policies>(policies)...);
  }
  void ChangePolicies() {}
  //@}

  friend class MutationBatcher;
  std::shared_ptr<DataClient> client_;
  std::string app_profile_id_;
  std::string table_name_;
  std::string table_id_;
  std::shared_ptr<RPCRetryPolicy const> rpc_retry_policy_prototype_;
  std::shared_ptr<RPCBackoffPolicy const> rpc_backoff_policy_prototype_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::shared_ptr<IdempotentMutationPolicy> idempotent_mutation_policy_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_H
