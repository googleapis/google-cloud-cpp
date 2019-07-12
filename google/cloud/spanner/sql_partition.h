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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_SQL_PARTITION_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_SQL_PARTITION_H_

#include "google/cloud/spanner/sql_statement.h"
#include "google/cloud/status_or.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

class SqlPartition;
/**
 * Serializes an instance of `SqlPartition` for transmission to another process.
 *
 * @param sql_partition - instance to be serialized.
 *
 * @par Example
 *
 * @code
 * spanner::SqlStatement stmt("select * from Albums");
 * std::vector<spanner::SqlPartition> partitions =
 *   spanner_client.PartitionSql(stmt);
 * for (auto const& partition : partitions) {
 *   auto serialized_partition = spanner::SerializeSqlPartition(partition);
 *   if (serialized_partition.ok()) {
 *     SendToRemoteMachine(*serialized_partition);
 *   }
 * }
 * @endcode
 */
StatusOr<std::string> SerializeSqlPartition(SqlPartition const& sql_partition);

/**
 * Deserializes the provided string into a `SqlPartition`, if able.
 *
 * Returned `Status` should be checked to determine if deserialization was
 * successful.
 *
 * @param serialized_sql_partition
 *
 * @par Example
 *
 * @code
 * std::string serialized_partition = ReceiveFromRemoteMachine();
 * spanner::SqlPartition partition =
 *   spanner::DeserializeSqlPartition(serialized_partition);
 * auto rows = spanner_client.ExecuteSql(partition);
 * @endcode
 */
StatusOr<SqlPartition> DeserializeSqlPartition(
    std::string const& serialized_sql_partition);

// Internal implementation details that callers should not use.
namespace internal {

SqlPartition MakeSqlPartition(std::string const& transaction_id,
                              std::string const& session_id,
                              std::string const& partition_token,
                              SqlStatement const& sql_statement);
}  // namespace internal

/**
 * The `SqlPartition` class is a semi-regular type that represents a single
 * slice of a parallel SQL read.
 *
 * Instances of `SqlPartition` are created by `Client::PartitionSql`. Once
 * created, `SqlPartition` objects can be serialized, transmitted to separate
 * process, and used to read data in parallel using `Client::ExecuteSql`.
 */
class SqlPartition {
 public:
  /**
   * Constructs an instance of `SqlPartition` that is not associated with any
   * `SqlStatement`.
   */
  SqlPartition() = default;

  // Copy and move.
  SqlPartition(SqlPartition const&) = default;
  SqlPartition(SqlPartition&&) = default;
  SqlPartition& operator=(SqlPartition const&) = default;
  SqlPartition& operator=(SqlPartition&&) = default;

  /**
   * Accessor for the `SqlStatement` associated with this `SqlPartition`.
   */
  SqlStatement const& sql_statement() const { return sql_statement_; }

 private:
  friend class SqlPartitionTester;
  friend SqlPartition internal::MakeSqlPartition(
      std::string const& transaction_id, std::string const& session_id,
      std::string const& partition_token, SqlStatement const& sql_statement);
  friend StatusOr<std::string> SerializeSqlPartition(
      SqlPartition const& sql_partition);
  friend StatusOr<SqlPartition> DeserializeSqlPartition(
      std::string const& serialized_sql_partition);

  explicit SqlPartition(std::string transaction_id, std::string session_id,
                        std::string partition_token,
                        SqlStatement sql_statement);

  // Accessor methods for use by friends.
  std::string const& partition_token() const { return partition_token_; }
  std::string const& session_id() const { return session_id_; }
  std::string const& transaction_id() const { return transaction_id_; }

  std::string transaction_id_;
  std::string session_id_;
  std::string partition_token_;
  SqlStatement sql_statement_;
};

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_SQL_PARTITION_H_
