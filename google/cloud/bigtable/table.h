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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_H_

#include "google/cloud/bigtable/internal/grpc_error_delegate.h"
#include "google/cloud/bigtable/internal/table.h"
#include "google/cloud/bigtable/row_set.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
class MutationBatcher;

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
      : impl_(std::move(client), table_id) {}

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
  Table(std::shared_ptr<DataClient> client,
        bigtable::AppProfileId app_profile_id, std::string const& table_id)
      : impl_(std::move(client), std::move(app_profile_id), table_id) {}

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
  template <typename... Policies>
  Table(std::shared_ptr<DataClient> client, std::string const& table_id,
        Policies&&... policies)
      : impl_(std::move(client), table_id,
              std::forward<Policies>(policies)...) {}

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
  template <typename... Policies>
  Table(std::shared_ptr<DataClient> client,
        bigtable::AppProfileId app_profile_id, std::string const& table_id,
        Policies&&... policies)
      : impl_(std::move(client), std::move(app_profile_id), table_id,
              std::forward<Policies>(policies)...) {}

  std::string const& table_name() const { return impl_.table_name(); }
  std::string const& app_profile_id() const { return impl_.app_profile_id(); }
  std::string const& project_id() const { return impl_.client_->project_id(); }
  std::string const& instance_id() const {
    return impl_.client_->instance_id();
  }
  std::string const& table_id() const { return impl_.table_id(); }

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
  StatusOr<bool> CheckAndMutateRow(std::string row_key, Filter filter,
                                   std::vector<Mutation> true_mutations,
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
  future<StatusOr<google::bigtable::v2::CheckAndMutateRowResponse>>
  AsyncCheckAndMutateRow(std::string row_key, Filter filter,
                         std::vector<Mutation> true_mutations,
                         std::vector<Mutation> false_mutations,
                         CompletionQueue& cq);

  /**
   * Sample of the row keys in the table, including approximate data sizes.
   *
   * @tparam Collection the type of collection where the samples are returned.
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
   *
   * In addition, application developers can specify other collection types, for
   * example `std::list<>` or `std::deque<>`:
   * @snippet data_snippets.cc sample row keys collections
   */
  template <template <typename...> class Collection = std::vector>
  StatusOr<Collection<bigtable::RowKeySample>> SampleRows() {
    grpc::Status status;
    Collection<bigtable::RowKeySample> result;

    SampleRowsImpl(
        [&result](bigtable::RowKeySample rs) {
          result.emplace_back(std::move(rs));
        },
        [&result]() { result.clear(); }, status);

    if (!status.ok()) {
      return bigtable::internal::MakeStatusFromRpcError(status);
    }

    return result;
  }

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

 private:
  /**
   * Send request ReadModifyWriteRowRequest to modify the row and get it back
   */
  StatusOr<Row> ReadModifyWriteRowImpl(
      ::google::bigtable::v2::ReadModifyWriteRowRequest request);

  future<StatusOr<Row>> AsyncReadModifyWriteRowImpl(
      CompletionQueue& cq,
      ::google::bigtable::v2::ReadModifyWriteRowRequest request);

  /**
   * Refactor implementation to `.cc` file.
   *
   * Provides a compilation barrier so that the application is not
   * exposed to all the implementation details.
   *
   * @param inserter Function to insert the object to result.
   * @param clearer Function to clear the result object if RPC fails.
   */
  void SampleRowsImpl(
      std::function<void(bigtable::RowKeySample)> const& inserter,
      std::function<void()> const& clearer, grpc::Status& status);

  void AddRules(google::bigtable::v2::ReadModifyWriteRowRequest& request) {
    // no-op for empty list
  }

  template <typename... Args>
  void AddRules(google::bigtable::v2::ReadModifyWriteRowRequest& request,
                bigtable::ReadModifyWriteRule rule, Args&&... args) {
    *request.add_rules() = std::move(rule).as_proto();
    AddRules(request, std::forward<Args>(args)...);
  }

  std::unique_ptr<RPCRetryPolicy> clone_rpc_retry_policy() {
    return impl_.rpc_retry_policy_->clone();
  }

  std::unique_ptr<RPCBackoffPolicy> clone_rpc_backoff_policy() {
    return impl_.rpc_backoff_policy_->clone();
  }

  MetadataUpdatePolicy clone_metadata_update_policy() {
    return impl_.metadata_update_policy_;
  }

  std::unique_ptr<IdempotentMutationPolicy> clone_idempotent_mutation_policy() {
    return impl_.idempotent_mutation_policy_->clone();
  }

  friend class MutationBatcher;
  noex::Table impl_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_H_
