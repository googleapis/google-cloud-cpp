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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_QUERY_PARTITION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_QUERY_PARTITION_H

#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/sql_statement.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
struct QueryPartitionInternals;
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal

namespace spanner {
inline namespace SPANNER_CLIENT_NS {
/**
 * Serializes an instance of `QueryPartition` to a string of bytes.
 *
 * The serialized string of bytes is suitable for writing to disk or
 * transmission to another process.
 *
 * @note The serialized string may contain NUL and other non-printable
 *     characters. Therefore, callers should avoid [formatted IO][formatted-io]
 *     functions that may incorrectly reformat the string data.
 *
 * @param query_partition - instance to be serialized.
 *
 * @par Example
 * @snippet samples.cc serialize-query-partition
 *
 * [formatted-io]:
 * https://en.cppreference.com/w/cpp/string/basic_string/operator_ltltgtgt
 */
StatusOr<std::string> SerializeQueryPartition(
    QueryPartition const& query_partition);

/**
 * Deserializes the provided string into a `QueryPartition`.
 *
 * The @p serialized_query_partition argument must be a string that was
 * previously returned by a call to `SerializeQueryPartition()`.
 *
 * @note The serialized string may contain NUL and other non-printable
 *     characters. Therefore, callers should avoid [formatted IO][formatted-io]
 *     functions that may incorrectly reformat the string data.
 *
 * @param serialized_query_partition
 *
 * @par Example
 * @snippet samples.cc deserialize-query-partition
 *
 * [formatted-io]:
 * https://en.cppreference.com/w/cpp/string/basic_string/operator_ltltgtgt
 */
StatusOr<QueryPartition> DeserializeQueryPartition(
    std::string const& serialized_query_partition);

/**
 * The `QueryPartition` class is a regular type that represents a single slice
 * of a parallel SQL read.
 *
 * Instances of `QueryPartition` are created by `Client::PartitionSql`. Once
 * created, `QueryPartition` objects can be serialized, transmitted to separate
 * process, and used to read data in parallel using `Client::ExecuteQuery`.
 */
class QueryPartition {
 public:
  /**
   * Constructs an instance of `QueryPartition` that is not associated with any
   * `SqlStatement`.
   */
  QueryPartition() = default;

  /// @name Copy and move
  ///@{
  QueryPartition(QueryPartition const&) = default;
  QueryPartition(QueryPartition&&) = default;
  QueryPartition& operator=(QueryPartition const&) = default;
  QueryPartition& operator=(QueryPartition&&) = default;
  ///@}

  /**
   * Accessor for the `SqlStatement` associated with this `QueryPartition`.
   */
  SqlStatement const& sql_statement() const { return sql_statement_; }

  /// @name Equality
  ///@{
  friend bool operator==(QueryPartition const& a, QueryPartition const& b);
  friend bool operator!=(QueryPartition const& a, QueryPartition const& b) {
    return !(a == b);
  }
  ///@}

 private:
  friend class QueryPartitionTester;
  friend struct spanner_internal::SPANNER_CLIENT_NS::QueryPartitionInternals;
  friend StatusOr<std::string> SerializeQueryPartition(
      QueryPartition const& query_partition);
  friend StatusOr<QueryPartition> DeserializeQueryPartition(
      std::string const& serialized_query_partition);

  QueryPartition(std::string transaction_id, std::string session_id,
                 std::string partition_token, SqlStatement sql_statement);

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
namespace spanner_internal {
inline namespace SPANNER_CLIENT_NS {
struct QueryPartitionInternals {
  static spanner::QueryPartition MakeQueryPartition(
      std::string const& transaction_id, std::string const& session_id,
      std::string const& partition_token,
      spanner::SqlStatement const& sql_statement) {
    return spanner::QueryPartition(transaction_id, session_id, partition_token,
                                   sql_statement);
  }

  static spanner::Connection::SqlParams MakeSqlParams(
      spanner::QueryPartition const& query_partition) {
    return {MakeTransactionFromIds(query_partition.session_id(),
                                   query_partition.transaction_id()),
            query_partition.sql_statement(), spanner::QueryOptions{},
            query_partition.partition_token()};
  }
};

inline spanner::QueryPartition MakeQueryPartition(
    std::string const& transaction_id, std::string const& session_id,
    std::string const& partition_token,
    spanner::SqlStatement const& sql_statement) {
  return QueryPartitionInternals::MakeQueryPartition(
      transaction_id, session_id, partition_token, sql_statement);
}

inline spanner::Connection::SqlParams MakeSqlParams(
    spanner::QueryPartition const& query_partition) {
  return QueryPartitionInternals::MakeSqlParams(query_partition);
}
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_QUERY_PARTITION_H
