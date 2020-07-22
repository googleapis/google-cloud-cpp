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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CLIENT_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CLIENT_H

#include "google/cloud/spanner/backoff_policy.h"
#include "google/cloud/spanner/batch_dml_result.h"
#include "google/cloud/spanner/client_options.h"
#include "google/cloud/spanner/commit_result.h"
#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/spanner/database.h"
#include "google/cloud/spanner/keys.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/partition_options.h"
#include "google/cloud/spanner/query_options.h"
#include "google/cloud/spanner/query_partition.h"
#include "google/cloud/spanner/read_options.h"
#include "google/cloud/spanner/read_partition.h"
#include "google/cloud/spanner/results.h"
#include "google/cloud/spanner/retry_policy.h"
#include "google/cloud/spanner/session_pool_options.h"
#include "google/cloud/spanner/sql_statement.h"
#include "google/cloud/spanner/transaction.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/optional.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <google/spanner/v1/spanner.pb.h>
#include <grpcpp/grpcpp.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * Performs database client operations on Spanner.
 *
 * Applications use this class to perform operations on
 * [Spanner Databases][spanner-doc-link].
 *
 * @par Performance
 *
 * `Client` objects are relatively cheap to create, copy, and move. However,
 * each `Client` object must be created with a `std::shared_ptr<Connection>`,
 * which itself is relatively expensive to create. Therefore, connection
 * instances should be shared when possible. See the `MakeConnection()` method
 * and the `Connection` interface for more details.
 *
 * @par Thread Safety
 *
 * Instances of this class created via copy-construction or copy-assignment
 * share the underlying pool of connections. Access to these copies via multiple
 * threads is guaranteed to work. Two threads operating on the same instance of
 * this class is not guaranteed to work.
 *
 * @par Error Handling
 *
 * This class uses `StatusOr<T>` to report errors. When an operation fails to
 * perform its work the returned `StatusOr<T>` contains the error details. If
 * the `ok()` member function in the `StatusOr<T>` returns `true` then it
 * contains the expected result. Please consult the
 * [`StatusOr<T>` documentation](#google::cloud::v0::StatusOr) for more details.
 *
 * @code
 * namespace spanner = ::google::cloud::spanner;
 *
 * auto db = spanner::Database("my_project", "my_instance", "my_db_id"));
 * auto conn = spanner::MakeConnection(db);
 * auto client = spanner::Client(conn);
 *
 * auto rows = client.Read(...);
 * using RowType = std::tuple<std::int64_t, std::string>;
 * for (auto const& row : spanner::StreamOf<RowType>(rows)) {
 *   // ...
 * }
 * @endcode
 *
 * @par Query Options
 *
 * Most operations that take an `SqlStatement` may also be modified with
 * `QueryOptions`. These options can be set at various levels, with more
 * specific levels taking precedence over broader ones. For example,
 * `QueryOptions` that are passed directly to `Client::ExecuteQuery()` will
 * take precedence over the `Client`-level defaults (if any), which will
 * themselves take precedence over any environment variables. The following
 * table shows the environment variables that may optionally be set and the
 * `QueryOptions` setting that they affect.
 *
 * Environment Variable         | QueryOptions setting
 * ---------------------------- | --------------------
 * `SPANNER_OPTIMIZER_VERSION`  | `QueryOptions::optimizer_version()`
 *
 * @see https://cloud.google.com/spanner/docs/reference/rest/v1/QueryOptions
 *
 * [spanner-doc-link]:
 * https://cloud.google.com/spanner/docs/api-libraries-overview
 */
class Client {
 public:
  /**
   * Constructs a `Client` object using the specified @p conn and @p opts.
   *
   * See `MakeConnection()` for how to create a connection to Spanner. To help
   * with unit testing, callers may create fake/mock `Connection` objects that
   * are injected into the `Client`.
   */
  explicit Client(std::shared_ptr<Connection> conn, ClientOptions opts = {})
      : conn_(std::move(conn)), opts_(std::move(opts)) {}

  /// No default construction. Use `Client(std::shared_ptr<Connection>)`
  Client() = delete;

  //@{
  // @name Copy and move support
  Client(Client const&) = default;
  Client& operator=(Client const&) = default;
  Client(Client&&) = default;
  Client& operator=(Client&&) = default;
  //@}

  //@{
  // @name Equality
  friend bool operator==(Client const& a, Client const& b) {
    return a.conn_ == b.conn_;
  }
  friend bool operator!=(Client const& a, Client const& b) { return !(a == b); }
  //@}

  //@{
  /**
   * Reads rows from the database using key lookups and scans, as a simple
   * key/value style alternative to `ExecuteQuery()`.
   *
   * Callers can optionally supply a `Transaction` or
   * `Transaction::SingleUseOptions` used to create a single-use transaction -
   * or neither, in which case a single-use transaction with default options
   * is used.
   *
   * @param table The name of the table in the database to be read.
   * @param keys Identifies the rows to be yielded. If `read_options.index_name`
   *     is set, names keys in that index; otherwise names keys in the primary
   *     index of `table`. It is not an error for `keys` to name rows that do
   *     not exist in the database; `Read` yields nothing for nonexistent rows.
   * @param columns The columns of `table` to be returned for each row matching
   *     this request.
   * @param read_options `ReadOptions` used for this request.
   *
   * @par Example
   * @snippet samples.cc read-data
   *
   * @note No individual row in the `ReadResult` can exceed 100 MiB, and no
   *     column value can exceed 10 MiB.
   */
  RowStream Read(std::string table, KeySet keys,
                 std::vector<std::string> columns,
                 ReadOptions read_options = {});

  /**
   * @copydoc Read
   *
   * @param transaction_options Execute this read in a single-use transaction
   * with these options.
   */
  RowStream Read(Transaction::SingleUseOptions transaction_options,
                 std::string table, KeySet keys,
                 std::vector<std::string> columns,
                 ReadOptions read_options = {});

  /**
   * @copydoc Read
   *
   * @param transaction Execute this read as part of an existing transaction.
   */
  RowStream Read(Transaction transaction, std::string table, KeySet keys,
                 std::vector<std::string> columns,
                 ReadOptions read_options = {});
  //@}

  /**
   * Reads rows from a subset of rows in a database. Requires a prior call
   * to `PartitionRead` to obtain the partition information; see the
   * documentation of that method for full details.
   *
   * @param partition A `ReadPartition`, obtained by calling `PartitionRead`.
   *
   * @note No individual row in the `ReadResult` can exceed 100 MiB, and no
   *     column value can exceed 10 MiB.
   *
   * @par Example
   * @snippet samples.cc read-read-partition
   */
  RowStream Read(ReadPartition const& partition);

  /**
   * Creates a set of partitions that can be used to execute a read
   * operation in parallel.  Each of the returned partitions can be passed
   * to `Read` to specify a subset of the read result to read.
   *
   * There are no ordering guarantees on rows returned among the returned
   * partition, or even within each individual `Read` call issued with a given
   * partition.
   *
   * Partitions become invalid when the session used to create them is deleted,
   * is idle for too long, begins a new transaction, or becomes too old. When
   * any of these happen, it is not possible to resume the read, and the whole
   * operation must be restarted from the beginning.
   *
   * @param transaction The transaction to execute the operation in.
   *     **Must** be a read-only snapshot transaction.
   * @param table The name of the table in the database to be read.
   * @param keys Identifies the rows to be yielded. If `read_options.index_name`
   *     is set, names keys in that index; otherwise names keys in the primary
   *     index of `table`. It is not an error for `keys` to name rows that do
   *     not exist in the database; `Read` yields nothing for nonexistent rows.
   * @param columns The columns of `table` to be returned for each row matching
   *     this request.
   * @param read_options `ReadOptions` used for this request.
   * @param partition_options `PartitionOptions` used for this request.
   *
   * @return A `StatusOr` containing a vector of `ReadPartition` or error
   *     status on failure.
   *
   * @par Example
   * @snippet samples.cc partition-read
   */
  StatusOr<std::vector<ReadPartition>> PartitionRead(
      Transaction transaction, std::string table, KeySet keys,
      std::vector<std::string> columns, ReadOptions read_options = {},
      PartitionOptions const& partition_options = PartitionOptions{});

  //@{
  /**
   * Executes a SQL query.
   *
   * Operations inside read-write transactions might return `ABORTED`. If this
   * occurs, the application should restart the transaction from the beginning.
   *
   * Callers can optionally supply a `Transaction` or
   * `Transaction::SingleUseOptions` used to create a single-use transaction -
   * or neither, in which case a single-use transaction with default options
   * is used.
   *
   * `SELECT * ...` queries are supported, but there's no guarantee about the
   * order, nor number, of returned columns. Therefore, the caller must look up
   * the wanted values in each row by column name. When the desired column
   * names are known in advance, it is better to list them explicitly in the
   * query's SELECT statement, so that unnecessary values are not
   * returned/ignored, and the column order is known. This enables more
   * efficient and simpler code.
   *
   * @par Example with explicitly selected columns.
   * @snippet samples.cc spanner-query-data
   *
   * @par Example using SELECT *
   * @snippet samples.cc spanner-query-data-select-star
   *
   * @param statement The SQL statement to execute.
   * @param opts The `QueryOptions` to use for this call. If given, these will
   *     take precedence over the options set at the client and environment
   *     levels.
   *
   * @note No individual row in the `RowStream` can exceed 100 MiB, and no
   *     column value can exceed 10 MiB.
   */
  RowStream ExecuteQuery(SqlStatement statement, QueryOptions const& opts = {});

  /**
   * @copydoc ExecuteQuery(SqlStatement, QueryOptions const&)
   *
   * @param transaction_options Execute this query in a single-use transaction
   *     with these options.
   */
  RowStream ExecuteQuery(Transaction::SingleUseOptions transaction_options,
                         SqlStatement statement, QueryOptions const& opts = {});

  /**
   * @copydoc ExecuteQuery(SqlStatement, QueryOptions const&)
   *
   * @param transaction Execute this query as part of an existing transaction.
   */
  RowStream ExecuteQuery(Transaction transaction, SqlStatement statement,
                         QueryOptions const& opts = {});
  /**
   * Executes a SQL query on a subset of rows in a database. Requires a prior
   * call to `PartitionQuery` to obtain the partition information; see the
   * documentation of that method for full details.
   *
   * @param partition A `QueryPartition`, obtained by calling `PartitionRead`.
   * @param opts The `QueryOptions` to use for this call. If given, these will
   *     take precedence over the options set at the client and environment
   *     levels.
   *
   * @note No individual row in the `RowStream` can exceed 100 MiB, and no
   *     column value can exceed 10 MiB.
   *
   * @par Example
   * @snippet samples.cc execute-sql-query-partition
   */
  RowStream ExecuteQuery(QueryPartition const& partition,
                         QueryOptions const& opts = {});
  //@}

  //@{
  /**
   * Profiles a SQL query.
   *
   * Profiling executes the query, provides the resulting rows, `ExecutionPlan`,
   * and execution statistics.
   *
   * Operations inside read-write transactions might return `kAborted`. If this
   * occurs, the application should restart the transaction from the beginning.
   *
   * Callers can optionally supply a `Transaction` or
   * `Transaction::SingleUseOptions` used to create a single-use transaction -
   * or neither, in which case a single-use transaction with default options
   * is used.
   *
   * @note Callers must consume all rows from the result before execution
   *     statistics and `ExecutionPlan` are available.
   *
   * @param statement The SQL statement to execute.
   * @param opts The `QueryOptions` to use for this call. If given, these will
   *     take precedence over the options set at the client and environment
   *     levels.
   *
   * @note No individual row in the `ProfileQueryResult` can exceed 100 MiB, and
   *     no column value can exceed 10 MiB.
   *
   * @par Example
   * @snippet samples.cc profile-query
   */
  ProfileQueryResult ProfileQuery(SqlStatement statement,
                                  QueryOptions const& opts = {});

  /**
   * @copydoc ProfileQuery(SqlStatement, QueryOptions const&)
   *
   * @param transaction_options Execute this query in a single-use transaction
   *     with these options.
   */
  ProfileQueryResult ProfileQuery(
      Transaction::SingleUseOptions transaction_options, SqlStatement statement,
      QueryOptions const& opts = {});

  /**
   * @copydoc ProfileQuery(SqlStatement, QueryOptions const&)
   *
   * @param transaction Execute this query as part of an existing transaction.
   */
  ProfileQueryResult ProfileQuery(Transaction transaction,
                                  SqlStatement statement,
                                  QueryOptions const& opts = {});
  //@}

  /**
   * Creates a set of partitions that can be used to execute a query
   * operation in parallel.  Each of the returned partitions can be passed
   * to `ExecuteQuery` to specify a subset of the query result to read.
   *
   * Partitions become invalid when the session used to create them is deleted,
   * is idle for too long, begins a new transaction, or becomes too old. When
   * any of these happen, it is not possible to resume the query, and the whole
   * operation must be restarted from the beginning.
   *
   * @param transaction The transaction to execute the operation in.
   *     **Must** be a read-only snapshot transaction.
   * @param statement The SQL statement to execute.
   * @param partition_options `PartitionOptions` used for this request.
   *
   * @return A `StatusOr` containing a vector of `QueryPartition`s or error
   *     status on failure.
   *
   * @par Example
   * @snippet samples.cc partition-query
   */
  StatusOr<std::vector<QueryPartition>> PartitionQuery(
      Transaction transaction, SqlStatement statement,
      PartitionOptions const& partition_options = PartitionOptions{});

  /**
   * Executes a SQL DML statement.
   *
   * Operations inside read-write transactions might return `ABORTED`. If this
   * occurs, the application should restart the transaction from the beginning.
   *
   * @note Single-use transactions are not supported with DML statements.
   *
   * @param transaction Execute this query as part of an existing transaction.
   * @param statement The SQL statement to execute.
   * @param opts The `QueryOptions` to use for this call. If given, these will
   *     take precedence over the options set at the client and environment
   *     levels.
   *
   * @par Example
   * @snippet samples.cc execute-dml
   */
  StatusOr<DmlResult> ExecuteDml(Transaction transaction,
                                 SqlStatement statement,
                                 QueryOptions const& opts = {});

  /**
   * Profiles a SQL DML statement.
   *
   * Profiling executes the DML statement, provides the modified row count,
   * `ExecutionPlan`, and execution statistics.
   *
   * Operations inside read-write transactions might return `ABORTED`. If this
   * occurs, the application should restart the transaction from the beginning.
   *
   * @note Single-use transactions are not supported with DML statements.
   *
   * @param transaction Execute this query as part of an existing transaction.
   * @param statement The SQL statement to execute.
   * @param opts The `QueryOptions` to use for this call. If given, these will
   *     take precedence over the options set at the client and environment
   *     levels.
   *
   * @par Example:
   * @snippet samples.cc profile-dml
   */
  StatusOr<ProfileDmlResult> ProfileDml(Transaction transaction,
                                        SqlStatement statement,
                                        QueryOptions const& opts = {});

  /**
   * Analyzes the execution plan of a SQL statement.
   *
   * Analyzing provides the `ExecutionPlan`, but does not execute the SQL
   * statement.
   *
   * Operations inside read-write transactions might return `ABORTED`. If this
   * occurs, the application should restart the transaction from the beginning.
   *
   * @note Single-use transactions are not supported with DML statements.
   *
   * @param transaction Execute this query as part of an existing transaction.
   * @param statement The SQL statement to execute.
   * @param opts The `QueryOptions` to use for this call. If given, these will
   *     take precedence over the options set at the client and environment
   *     levels.
   *
   * @par Example:
   * @snippet samples.cc analyze-query
   */
  StatusOr<ExecutionPlan> AnalyzeSql(Transaction transaction,
                                     SqlStatement statement,
                                     QueryOptions const& opts = {});

  /**
   * Executes a batch of SQL DML statements. This method allows many statements
   * to be run with lower latency than submitting them sequentially with
   * `ExecuteDml`.
   *
   * Statements are executed in order, sequentially. Execution will stop at the
   * first failed statement; the remaining statements will not run.
   *
   * As with all read-write transactions, the results will not be visible
   * outside of the transaction until it is committed. For that reason, it is
   * advisable to run this method from a `Commit` mutator.
   *
   * @warning A returned status of OK from this function does not imply that
   *     all the statements were executed successfully. For that, you need to
   *     inspect the `BatchDmlResult::status` field.
   *
   * @param transaction The read-write transaction to execute the operation in.
   * @param statements The list of statements to execute in this batch.
   *     Statements are executed serially, such that the effects of statement i
   *     are visible to statement i+1. Each statement must be a DML statement.
   *     Execution will stop at the first failed statement; the remaining
   *     statements will not run. Must not be empty.
   *
   * @par Example
   * @snippet samples.cc execute-batch-dml
   */
  StatusOr<BatchDmlResult> ExecuteBatchDml(
      Transaction transaction, std::vector<SqlStatement> statements);

  /**
   * Commits a read-write transaction.
   *
   * Calls the @p mutator in the context of a new read-write transaction.
   * The @p mutator can execute read/write operations using the transaction,
   * and returns any additional `Mutations` to commit.
   *
   * If the @p mutator succeeds and the transaction commits, then `Commit()`
   * returns the `CommitResult`.
   *
   * If the @p mutator returns a non-rerunnable status (according to the
   * @p rerun_policy), the transaction is rolled back and that status is
   * returned. Similarly, if the transaction fails to commit with a non-
   * rerunnable status, that status is returned.
   *
   * Otherwise the whole process repeats (subject to @p rerun_policy and
   * @p backoff_policy), by building a new transaction and re-running the
   * @p mutator.  The lock priority of the operation increases after each
   * rerun, meaning that the next attempt has a slightly better chance of
   * success.
   *
   * Note that the @p mutator should only return a rerunnable status when
   * the transaction is no longer usable (e.g., it was aborted). Otherwise
   * the transaction will be leaked.
   *
   * @param mutator the function called to create mutations
   * @param rerun_policy controls for how long (or how many times) the mutator
   *     will be rerun after the transaction aborts.
   * @param backoff_policy controls how long `Commit` waits between reruns.
   *
   * @throw Rethrows any exception thrown by @p `mutator` (after rolling back
   *     the transaction). However, a `RuntimeStatusError` exception is
   *     instead consumed and converted into a `mutator` return value of the
   *     enclosed `Status`.
   *
   * @par Example
   * @snippet samples.cc commit-with-policies
   */
  StatusOr<CommitResult> Commit(
      std::function<StatusOr<Mutations>(Transaction)> const& mutator,
      std::unique_ptr<TransactionRerunPolicy> rerun_policy,
      std::unique_ptr<BackoffPolicy> backoff_policy);

  /**
   * Commits a read-write transaction.
   *
   * Same as above, but uses the default rerun and backoff policies.
   *
   * @param mutator the function called to create mutations
   *
   * @par Example
   * @snippet samples.cc commit-with-mutator
   */
  StatusOr<CommitResult> Commit(
      std::function<StatusOr<Mutations>(Transaction)> const& mutator);

  /**
   * Commits the given @p mutations atomically in order.
   *
   * This function uses the re-run loop described above with the default
   * policies.
   *
   * @par Example
   * @snippet samples.cc commit-with-mutations
   */
  StatusOr<CommitResult> Commit(Mutations mutations);

  /**
   * Commits a read-write transaction.
   *
   * The commit might return a `kAborted` error. This can occur at any time.
   * Commonly the cause is conflicts with concurrent transactions, however
   * it can also happen for a variety of other reasons. If `Commit` returns
   * `kAborted`, the caller may try to reapply the mutations within a new
   * read-write transaction (which should share lock priority with the aborted
   * transaction so that the new attempt has a slightly better chance of
   * success).
   *
   * @note Prefer the previous `Commit` overloads if you want to simply reapply
   *     mutations after a `kAborted` error.
   *
   * @warning It is an error to call `Commit` with a read-only transaction.
   *
   * @param transaction The transaction to commit.
   * @param mutations The mutations to be executed when this transaction
   *     commits. All mutations are applied atomically, in the order they appear
   *     in this list.
   *
   * @return A `StatusOr` containing the result of the commit or error status
   *     on failure.
   */
  StatusOr<CommitResult> Commit(Transaction transaction, Mutations mutations);

  /**
   * Rolls back a read-write transaction, releasing any locks it holds.
   *
   * At any time before `Commit`, the client can call `Rollback` to abort the
   * transaction. It is a good idea to call this for any read-write transaction
   * that includes  one or more `Read`, `ExecuteQuery`, or `ExecuteDml` requests
   * and ultimately decides not to commit.
   *
   * @warning It is an error to call `Rollback` with a read-only transaction.
   *
   * @param transaction The transaction to roll back.
   *
   * @return The error status of the rollback.
   */
  Status Rollback(Transaction transaction);

  /**
   * Executes a Partitioned DML SQL query.
   *
   * @param statement the SQL statement to execute. Please see the
   *     [spanner documentation][dml-partitioned] for the restrictions on the
   *     SQL statements supported by this function.
   *
   * @par Example
   * @snippet samples.cc execute-sql-partitioned
   *
   * @see [Partitioned DML Transactions][txn-partitioned] for an overview of
   *     Partitioned DML transactions.
   * @see [Partitioned DML][dml-partitioned] for a description of which SQL
   *     statements are supported in Partitioned DML transactions.
   * [txn-partitioned]:
   * https://cloud.google.com/spanner/docs/transactions#partitioned_dml_transactions
   * [dml-partitioned]: https://cloud.google.com/spanner/docs/dml-partitioned
   */
  StatusOr<PartitionedDmlResult> ExecutePartitionedDml(SqlStatement statement);

 private:
  QueryOptions OverlayQueryOptions(QueryOptions const&);

  std::shared_ptr<Connection> conn_;
  ClientOptions opts_;
};

/**
 * Returns a Connection object that can be used for interacting with Spanner.
 *
 * The returned connection object should not be used directly, rather it should
 * be given to a `Client` instance, and methods should be invoked on `Client`.
 *
 * @see `Connection`
 *
 * @param db See `Database`.
 * @param connection_options (optional) configure the `Connection` created by
 *     this function.
 * @param session_pool_options (optional) configure the `SessionPool` created
 *     by the `Connection`.
 */
std::shared_ptr<Connection> MakeConnection(
    Database const& db,
    ConnectionOptions const& connection_options = ConnectionOptions(),
    SessionPoolOptions session_pool_options = SessionPoolOptions());

/**
 * @copydoc MakeConnection(Database const&, ConnectionOptions const&, SessionPoolOptions)
 *
 * @param retry_policy override the default `RetryPolicy`, controls how long
 *     the returned `Connection` object retries requests on transient
 *     failures.
 * @param backoff_policy override the default `BackoffPolicy`, controls how
 *     long the `Connection` object waits before retrying a failed request.
 */
std::shared_ptr<Connection> MakeConnection(
    Database const& db, ConnectionOptions const& connection_options,
    SessionPoolOptions session_pool_options,
    std::unique_ptr<RetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy);

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CLIENT_H
