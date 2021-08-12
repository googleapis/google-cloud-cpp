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

ReadPartition::ReadPartition(std::string transaction_id,
                             std::string transaction_tag,
                             std::string session_id,
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
  *proto_.mutable_key_set() = spanner_internal::ToProto(std::move(key_set));
  proto_.set_limit(read_options.limit);
  proto_.set_partition_token(std::move(partition_token));
  if (read_options.request_priority) {
    auto* request_options = proto_.mutable_request_options();
    switch (*read_options.request_priority) {
      case spanner::RequestPriority::kLow:
        request_options->set_priority(
            google::spanner::v1::RequestOptions::PRIORITY_LOW);
        break;
      case spanner::RequestPriority::kMedium:
        request_options->set_priority(
            google::spanner::v1::RequestOptions::PRIORITY_MEDIUM);
        break;
      case spanner::RequestPriority::kHigh:
        request_options->set_priority(
            google::spanner::v1::RequestOptions::PRIORITY_HIGH);
        break;
    }
  }
  if (read_options.request_tag.has_value()) {
    proto_.mutable_request_options()->set_request_tag(
        *std::move(read_options.request_tag));
  }
  proto_.mutable_request_options()->set_transaction_tag(
      std::move(transaction_tag));
}

google::cloud::spanner::ReadOptions ReadPartition::ReadOptions() const {
  google::cloud::spanner::ReadOptions options;
  options.index_name = proto_.index();
  options.limit = proto_.limit();
  if (proto_.has_request_options()) {
    switch (proto_.request_options().priority()) {
      case google::spanner::v1::RequestOptions::PRIORITY_LOW:
        options.request_priority = spanner::RequestPriority::kLow;
        break;
      case google::spanner::v1::RequestOptions::PRIORITY_MEDIUM:
        options.request_priority = spanner::RequestPriority::kMedium;
        break;
      case google::spanner::v1::RequestOptions::PRIORITY_HIGH:
        options.request_priority = spanner::RequestPriority::kHigh;
        break;
      default:
        break;
    }
    if (!proto_.request_options().request_tag().empty()) {
      options.request_tag = proto_.request_options().request_tag();
    }
  }
  return options;
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

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
