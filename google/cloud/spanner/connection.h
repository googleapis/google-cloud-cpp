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

#include "google/cloud/spanner/client_options.h"
#include "google/cloud/spanner/commit_result.h"
#include "google/cloud/spanner/keys.h"
#include "google/cloud/spanner/mutations.h"
#include "google/cloud/spanner/result_set.h"
#include "google/cloud/spanner/sql_statement.h"
#include "google/cloud/spanner/transaction.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

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

  struct ReadParams {
    Transaction transaction;
    std::string table;
    KeySet keys;
    std::vector<std::string> columns;
    ReadOptions read_options;
  };
  virtual StatusOr<ResultSet> Read(ReadParams) = 0;

  struct ExecuteSqlParams {
    Transaction transaction;
    SqlStatement statement;
  };
  virtual StatusOr<ResultSet> ExecuteSql(ExecuteSqlParams) = 0;

  /**
   * Commits a transaction.
   *
   * The commit might return an `ABORTED` error. This can occur at any time;
   * commonly, the cause is conflicts with concurrent transactions. However, it
   * can also happen for a variety of other reasons. If `Commit` returns
   * `ABORTED`, the caller should re-attempt the transaction from the beginning,
   * re-using the same session.
   */
  struct CommitParams {
    Transaction transaction;
    std::vector<Mutation> mutations;
  };
  virtual StatusOr<CommitResult> Commit(CommitParams) = 0;

  /**
   * Rollback a read-write transaction.
   *
   * At any time before `Commit`, the client can send a `Rollback` request
   * to abort the transaction. (Read-only transactions do not need to call
   * `Commit` or `Rollback`. In fact, they are not permitted to do so).
   */
  struct RollbackParams {
    Transaction transaction;
  };
  virtual Status Rollback(RollbackParams) = 0;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_SPANNER_GOOGLE_CLOUD_SPANNER_CONNECTION_H_
