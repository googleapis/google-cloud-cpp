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

#ifndef GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CONNECTION_H_
#define GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CONNECTION_H_

#include "google/cloud/spanner/batch_dml_result.h"
#include "google/cloud/spanner/commit_result.h"
#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/spanner/keys.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/partitioned_dml_result.h"
#include "google/cloud/spanner/read_options.h"
#include "google/cloud/spanner/result_set.h"
#include "google/cloud/spanner/sql_statement.h"
#include "google/cloud/spanner/transaction.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class ReadPartition;
class QueryPartition;

/**
 * A connection to a Spanner database instance.
 *
 * This interface defines pure-virtual methods for each of the user-facing
 * overload sets in `Client`. That is, all of `Client`'s `Read()` overloads
 * will forward to the one pure-virtual `Read()` method declared in this
 * interface. This allows users to inject custom behavior (e.g., with a Google
 * Mock object) in a `Client` object for use in their own tests.
 *
 * To create a concrete instance that connects you to a real Spanner database,
 * see `MakeConnection()`.
 */
class Connection {
 public:
  virtual ~Connection() = default;

  //@{
  /**
   * Define the arguments for each member function.
   *
   * Applications may define classes derived from `Connection`, for example,
   * because they want to mock the class. To avoid breaking all such derived
   * classes when we change the number or type of the arguments to the member
   * functions we define light weight structures to pass the arguments.
   */
  /// Wrap the arguments to `Read()`.
  struct ReadParams {
    Transaction transaction;
    std::string table;
    KeySet keys;
    std::vector<std::string> columns;
    ReadOptions read_options;
    google::cloud::optional<std::string> partition_token;

    ReadParams(Transaction transaction, std::string table, KeySet keys,
               std::vector<std::string> columns, ReadOptions read_options,
               google::cloud::optional<std::string> partition_token = {})
        : transaction(std::move(transaction)),
          table(std::move(table)),
          keys(std::move(keys)),
          columns(std::move(columns)),
          read_options(std::move(read_options)),
          partition_token(std::move(partition_token)) {}
  };

  /// Wrap the arguments to `PartitionRead()`.
  struct PartitionReadParams {
    ReadParams read_params;
    PartitionOptions partition_options;
  };

  /// Wrap the arguments to `ExecuteSql()`.
  struct ExecuteSqlParams {
    Transaction transaction;
    SqlStatement statement;
    google::cloud::optional<std::string> partition_token;

    ExecuteSqlParams(Transaction transaction, SqlStatement statement,
                     google::cloud::optional<std::string> partition_token = {})
        : transaction(std::move(transaction)),
          statement(std::move(statement)),
          partition_token(std::move(partition_token)) {}
  };

  /// Wrap the arguments to `ExecutePartitionedDmlParams()`.
  struct ExecutePartitionedDmlParams {
    SqlStatement statement;
  };

  /// Wrap the arguments to `PartitionQuery()`.
  struct PartitionQueryParams {
    ExecuteSqlParams sql_params;
    PartitionOptions partition_options;
  };

  /// Wrap the arguments to `ExecuteBatchDml()`.
  struct ExecuteBatchDmlParams {
    Transaction transaction;
    std::vector<SqlStatement> statements;
  };

  /// Wrap the arguments to `Commit()`.
  struct CommitParams {
    Transaction transaction;
    Mutations mutations;
  };

  /// Wrap the arguments to `Rollback()`.
  struct RollbackParams {
    Transaction transaction;
  };
  //@}

  /// Define the interface for a google.spanner.v1.Spanner.Read RPC
  virtual ReadResult Read(ReadParams) = 0;

  /// Define the interface for a google.spanner.v1.Spanner.PartitionRead RPC
  virtual StatusOr<std::vector<ReadPartition>> PartitionRead(
      PartitionReadParams) = 0;

  /// Define the interface for a google.spanner.v1.Spanner.ExecuteSql RPC
  virtual ExecuteQueryResult ExecuteQuery(ExecuteSqlParams) = 0;

  /// Define the interface for a google.spanner.v1.Spanner.ExecuteSql RPC
  virtual StatusOr<ExecuteDmlResult> ExecuteDml(ExecuteSqlParams) = 0;

  /// Define the interface for a google.spanner.v1.Spanner.ExecutePartitionedDml
  /// RPC
  virtual StatusOr<PartitionedDmlResult> ExecutePartitionedDml(
      ExecutePartitionedDmlParams) = 0;

  /// Define the interface for a google.spanner.v1.Spanner.PartitionQuery RPC
  virtual StatusOr<std::vector<QueryPartition>> PartitionQuery(
      PartitionQueryParams) = 0;

  /// Define the interface for a google.spanner.v1.Spanner.ExecuteBatchDml RPC
  virtual StatusOr<BatchDmlResult> ExecuteBatchDml(ExecuteBatchDmlParams) = 0;

  /// Define the interface for a google.spanner.v1.Spanner.Commit RPC
  virtual StatusOr<CommitResult> Commit(CommitParams) = 0;

  /// Define the interface for a google.spanner.v1.Spanner.Rollback RPC
  virtual Status Rollback(RollbackParams) = 0;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CONNECTION_H_
