// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class QueryPartitionTester;
struct QueryPartitionInternals;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal

namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

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
 * Instances of `QueryPartition` are created by `Client::PartitionQuery`. Once
 * created, `QueryPartition` objects can be serialized, transmitted to separate
 * processes, and used to read data in parallel using `Client::ExecuteQuery`.
 * If `data_boost` is set, those requests will be executed using the
 * independent compute resources of Cloud Spanner Data Boost.
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
  friend class spanner_internal::QueryPartitionTester;
  friend struct spanner_internal::QueryPartitionInternals;
  friend StatusOr<std::string> SerializeQueryPartition(
      QueryPartition const& query_partition);
  friend StatusOr<QueryPartition> DeserializeQueryPartition(
      std::string const& serialized_query_partition);

  QueryPartition(std::string transaction_id, bool route_to_leader,
                 std::string transaction_tag, std::string session_id,
                 std::string partition_token, bool data_boost,
                 SqlStatement sql_statement);

  // Accessor methods for use by friends.
  std::string const& transaction_id() const { return transaction_id_; }
  bool route_to_leader() const { return route_to_leader_; }
  std::string const& transaction_tag() const { return transaction_tag_; }
  std::string const& session_id() const { return session_id_; }
  std::string const& partition_token() const { return partition_token_; }
  bool data_boost() const { return data_boost_; }

  std::string transaction_id_;
  bool route_to_leader_;
  std::string transaction_tag_;
  std::string session_id_;
  std::string partition_token_;
  bool data_boost_;
  SqlStatement sql_statement_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner

// Internal implementation details that callers should not use.
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct QueryPartitionInternals {
  static spanner::QueryPartition MakeQueryPartition(
      std::string const& transaction_id, bool route_to_leader,
      std::string const& transaction_tag, std::string const& session_id,
      std::string const& partition_token, bool data_boost,
      spanner::SqlStatement const& sql_statement) {
    return spanner::QueryPartition(transaction_id, route_to_leader,
                                   transaction_tag, session_id, partition_token,
                                   data_boost, sql_statement);
  }

  static spanner::Connection::SqlParams MakeSqlParams(
      spanner::QueryPartition const& query_partition,
      spanner::QueryOptions const& query_options,
      spanner::DirectedReadOption::Type directed_read_option) {
    return {MakeTransactionFromIds(query_partition.session_id(),
                                   query_partition.transaction_id(),
                                   query_partition.route_to_leader(),
                                   query_partition.transaction_tag()),
            query_partition.sql_statement(),
            query_options,
            query_partition.partition_token(),
            query_partition.data_boost(),
            std::move(directed_read_option)};
  }
};

inline spanner::QueryPartition MakeQueryPartition(
    std::string const& transaction_id, bool route_to_leader,
    std::string const& transaction_tag, std::string const& session_id,
    std::string const& partition_token, bool data_boost,
    spanner::SqlStatement const& sql_statement) {
  return QueryPartitionInternals::MakeQueryPartition(
      transaction_id, route_to_leader, transaction_tag, session_id,
      partition_token, data_boost, sql_statement);
}

inline spanner::Connection::SqlParams MakeSqlParams(
    spanner::QueryPartition const& query_partition,
    spanner::QueryOptions const& query_options,
    spanner::DirectedReadOption::Type directed_read_option) {
  return QueryPartitionInternals::MakeSqlParams(
      query_partition, query_options, std::move(directed_read_option));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_QUERY_PARTITION_H
