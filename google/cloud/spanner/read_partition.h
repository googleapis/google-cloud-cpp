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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_READ_PARTITION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_READ_PARTITION_H

#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/keys.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status_or.h"
#include <google/spanner/v1/spanner.pb.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class ReadPartitionTester;
struct ReadPartitionInternals;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal

namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Serializes an instance of `ReadPartition` to a string of bytes.
 *
 * The serialized string of bytes is suitable for writing to disk or
 * transmission to another process.
 *
 * @note The serialized string may contain NUL and other non-printable
 *     characters. Therefore, callers should avoid [formatted IO][formatted-io]
 *     functions that may incorrectly reformat the string data.
 *
 * @param read_partition - instance to be serialized.
 *
 * @par Example
 * @snippet samples.cc serialize-read-partition
 *
 * [formatted-io]:
 * https://en.cppreference.com/w/cpp/string/basic_string/operator_ltltgtgt
 */
StatusOr<std::string> SerializeReadPartition(
    ReadPartition const& read_partition);

/**
 * Deserializes the provided string into a `ReadPartition`.
 *
 * The @p serialized_read_partition argument must be a string that was
 * previously returned by a call to `SerializeReadPartition()`.
 *
 * @note The serialized string may contain NUL and other non-printable
 *     characters. Therefore, callers should avoid [formatted IO][formatted-io]
 *     functions that may incorrectly reformat the string data.
 *
 * @param serialized_read_partition - string representation to be deserialized.
 *
 * @par Example
 * @snippet samples.cc deserialize-read-partition
 *
 * [formatted-io]:
 * https://en.cppreference.com/w/cpp/string/basic_string/operator_ltltgtgt
 */
StatusOr<ReadPartition> DeserializeReadPartition(
    std::string const& serialized_read_partition);

/**
 * The `ReadPartition` class is a regular type that represents a single
 * slice of a parallel Read operation.
 *
 * Instances of `ReadPartition` are created by `Client::PartitionRead`.
 * Once created, `ReadPartition` objects can be serialized, transmitted to
 * separate processes, and used to read data in parallel using `Client::Read`.
 * If `data_boost` is set, those requests will be executed using the
 * independent compute resources of Cloud Spanner Data Boost.
 */
class ReadPartition {
 public:
  /**
   * Constructs an instance of `ReadPartition` that does not specify any table
   * or columns to be read.
   */
  ReadPartition() = default;

  /// @name Copy and move.
  ///@{
  ReadPartition(ReadPartition const&) = default;
  ReadPartition(ReadPartition&&) = default;
  ReadPartition& operator=(ReadPartition const&) = default;
  ReadPartition& operator=(ReadPartition&&) = default;
  ///@}

  std::string TableName() const { return proto_.table(); }

  std::vector<std::string> ColumnNames() const {
    auto const& columns = proto_.columns();
    return std::vector<std::string>(columns.begin(), columns.end());
  }

  google::cloud::spanner::ReadOptions ReadOptions() const;

  /// @name Equality
  ///@{
  friend bool operator==(ReadPartition const& lhs, ReadPartition const& rhs);
  friend bool operator!=(ReadPartition const& lhs, ReadPartition const& rhs) {
    return !(lhs == rhs);
  }
  ///@}

 private:
  friend class spanner_internal::ReadPartitionTester;
  friend struct spanner_internal::ReadPartitionInternals;
  friend StatusOr<std::string> SerializeReadPartition(
      ReadPartition const& read_partition);
  friend StatusOr<ReadPartition> DeserializeReadPartition(
      std::string const& serialized_read_partition);

  explicit ReadPartition(google::spanner::v1::ReadRequest proto)
      : proto_(std::move(proto)) {}
  ReadPartition(std::string transaction_id, bool route_to_leader,
                std::string transaction_tag, std::string session_id,
                std::string partition_token, std::string table_name,
                google::cloud::spanner::KeySet key_set,
                std::vector<std::string> column_names, bool data_boost,
                google::cloud::spanner::ReadOptions read_options);

  // Accessor methods for use by friends.
  std::string TransactionId() const { return proto_.transaction().id(); }
  bool RouteToLeader() const;
  std::string TransactionTag() const {
    return proto_.request_options().transaction_tag();
  }
  std::string SessionId() const { return proto_.session(); }
  std::string PartitionToken() const { return proto_.partition_token(); }
  google::spanner::v1::KeySet KeySet() const { return proto_.key_set(); }
  bool DataBoost() const { return proto_.data_boost_enabled(); }

  google::spanner::v1::ReadRequest proto_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner

// Internal implementation details that callers should not use.
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

struct ReadPartitionInternals {
  static spanner::ReadPartition MakeReadPartition(
      std::string transaction_id, bool route_to_leader,
      std::string transaction_tag, std::string session_id,
      std::string partition_token, std::string table_name,
      spanner::KeySet key_set, std::vector<std::string> column_names,
      bool data_boost, spanner::ReadOptions read_options) {
    return spanner::ReadPartition(
        std::move(transaction_id), route_to_leader, std::move(transaction_tag),
        std::move(session_id), std::move(partition_token),
        std::move(table_name), std::move(key_set), std::move(column_names),
        data_boost, std::move(read_options));
  }

  static spanner::Connection::ReadParams MakeReadParams(
      spanner::ReadPartition const& read_partition,
      spanner::DirectedReadOption::Type directed_read_option,
      spanner::LockHint lock_hint) {
    return spanner::Connection::ReadParams{
        MakeTransactionFromIds(
            read_partition.SessionId(), read_partition.TransactionId(),
            read_partition.RouteToLeader(), read_partition.TransactionTag()),
        read_partition.TableName(),
        FromProto(read_partition.KeySet()),
        read_partition.ColumnNames(),
        read_partition.ReadOptions(),
        read_partition.PartitionToken(),
        read_partition.DataBoost(),
        std::move(directed_read_option),
        lock_hint};
  }
};

inline spanner::ReadPartition MakeReadPartition(
    std::string transaction_id, bool route_to_leader,
    std::string transaction_tag, std::string session_id,
    std::string partition_token, std::string table_name,
    spanner::KeySet key_set, std::vector<std::string> column_names,
    bool data_boost, spanner::ReadOptions read_options) {
  return ReadPartitionInternals::MakeReadPartition(
      std::move(transaction_id), route_to_leader, std::move(transaction_tag),
      std::move(session_id), std::move(partition_token), std::move(table_name),
      std::move(key_set), std::move(column_names), data_boost,
      std::move(read_options));
}

inline spanner::Connection::ReadParams MakeReadParams(
    spanner::ReadPartition const& read_partition,
    spanner::DirectedReadOption::Type directed_read_option,
    spanner::LockHint lock_hint) {
  return ReadPartitionInternals::MakeReadParams(
      read_partition, std::move(directed_read_option), lock_hint);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_READ_PARTITION_H
