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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CONNECTION_H

#include "google/cloud/spanner/batch_dml_result.h"
#include "google/cloud/spanner/commit_result.h"
#include "google/cloud/spanner/connection_options.h"
#include "google/cloud/spanner/keys.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/partition_options.h"
#include "google/cloud/spanner/partitioned_dml_result.h"
#include "google/cloud/spanner/query_options.h"
#include "google/cloud/spanner/read_options.h"
#include "google/cloud/spanner/results.h"
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
 * interface, and similar for `Client`'s other methods. This allows users to
 * inject custom behavior (e.g., with a Google Mock object) in a `Client`
 * object for use in their own tests.
 *
 * To create a concrete instance that connects you to a real Spanner database,
 * see `MakeConnection()`.
 */
class Connection {
 public:
  virtual ~Connection() = default;

  //@{
  /**
   * @name Defines the arguments for each member function.
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
  };

  /// Wrap the arguments to `PartitionRead()`.
  struct PartitionReadParams {
    ReadParams read_params;
    PartitionOptions partition_options;
  };

  /// Wrap the arguments to `ExecuteQuery()`, `ExecuteDml()`, `ProfileQuery()`,
  /// `ProfileDml()`, and `AnalyzeSql()`.
  struct SqlParams {
    Transaction transaction;
    SqlStatement statement;
    QueryOptions query_options;
    google::cloud::optional<std::string> partition_token;
  };

  /// Wrap the arguments to `ExecutePartitionedDml()`.
  struct ExecutePartitionedDmlParams {
    SqlStatement statement;
  };

  /// Wrap the arguments to `PartitionQuery()`.
  struct PartitionQueryParams {
    Transaction transaction;
    SqlStatement statement;
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

  /// Defines the interface for `Client::Read()`
  virtual RowStream Read(ReadParams) = 0;

  /// Defines the interface for `Client::PartitionRead()`
  virtual StatusOr<std::vector<ReadPartition>> PartitionRead(
      PartitionReadParams) = 0;

  /// Defines the interface for `Client::ExecuteQuery()`
  virtual RowStream ExecuteQuery(SqlParams) = 0;

  /// Defines the interface for `Client::ExecuteDml()`
  virtual StatusOr<DmlResult> ExecuteDml(SqlParams) = 0;

  /// Defines the interface for `Client::ProfileQuery()`
  virtual ProfileQueryResult ProfileQuery(SqlParams) = 0;

  /// Defines the interface for `Client::ProfileDml()`
  virtual StatusOr<ProfileDmlResult> ProfileDml(SqlParams) = 0;

  /// Defines the interface for `Client::AnalyzeSql()`
  virtual StatusOr<ExecutionPlan> AnalyzeSql(SqlParams) = 0;

  /// Defines the interface for `Client::ExecutePartitionedDml()`
  virtual StatusOr<PartitionedDmlResult> ExecutePartitionedDml(
      ExecutePartitionedDmlParams) = 0;

  /// Defines the interface for `Client::PartitionQuery()`
  virtual StatusOr<std::vector<QueryPartition>> PartitionQuery(
      PartitionQueryParams) = 0;

  /// Defines the interface for `Client::ExecuteBatchDml()`
  virtual StatusOr<BatchDmlResult> ExecuteBatchDml(ExecuteBatchDmlParams) = 0;

  /// Defines the interface for `Client::Commit()`
  virtual StatusOr<CommitResult> Commit(CommitParams) = 0;

  /// Defines the interface for `Client::Rollback()`
  virtual Status Rollback(RollbackParams) = 0;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_CONNECTION_H
