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

#include "google/cloud/spanner/read_partition.h"
#include <google/spanner/v1/spanner.pb.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

ReadPartition::ReadPartition(std::string transaction_id, std::string session_id,
                             std::string partition_token,
                             std::string table_name,
                             google::cloud::spanner::KeySet key_set,
                             std::vector<std::string> column_names,
                             google::cloud::spanner::ReadOptions read_options) {
  proto_.set_session(std::move(session_id));
  proto_.mutable_transaction()->set_id(std::move(transaction_id));
  proto_.set_table(std::move(table_name));
  proto_.set_index(std::move(read_options.index_name));
  for (auto& column : column_names) {
    *proto_.mutable_columns()->Add() = std::move(column);
  }
  *proto_.mutable_key_set() = internal::ToProto(std::move(key_set));
  proto_.set_limit(read_options.limit);
  proto_.set_partition_token(std::move(partition_token));
}

bool operator==(ReadPartition const& lhs, ReadPartition const& rhs) {
  google::protobuf::util::MessageDifferencer differencer;
  return differencer.Compare(lhs.proto_, rhs.proto_);
}

bool operator!=(ReadPartition const& lhs, ReadPartition const& rhs) {
  return !(lhs == rhs);
}

StatusOr<std::string> SerializeReadPartition(
    ReadPartition const& read_partition) {
  std::string serialized_proto;
  if (read_partition.proto_.SerializeToString(&serialized_proto)) {
    return serialized_proto;
  }
  return Status(StatusCode::kInvalidArgument,
                "Failed to serialize SqlPartition");
}

StatusOr<ReadPartition> DeserializeReadPartition(
    std::string const& serialized_read_partition) {
  google::spanner::v1::ReadRequest proto;
  if (!proto.ParseFromString(serialized_read_partition)) {
    return Status(StatusCode::kInvalidArgument,
                  "Failed to deserialize into SqlPartition");
  }
  return ReadPartition(std::move(proto));
}

namespace internal {
ReadPartition MakeReadPartition(std::string transaction_id,
                                std::string session_id,
                                std::string partition_token,
                                std::string table_name, KeySet key_set,
                                std::vector<std::string> column_names,
                                ReadOptions read_options) {
  return ReadPartition(std::move(transaction_id), std::move(session_id),
                       std::move(partition_token), std::move(table_name),
                       std::move(key_set), std::move(column_names),
                       std::move(read_options));
}

Connection::ReadParams MakeReadParams(ReadPartition const& read_partition) {
  return Connection::ReadParams{
      MakeTransactionFromIds(read_partition.SessionId(),
                             read_partition.TransactionId()),
      read_partition.TableName(),
      FromProto(read_partition.KeySet()),
      read_partition.ColumnNames(),
      read_partition.ReadOptions(),
      read_partition.PartitionToken()};
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
