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

#include "google/cloud/spanner/internal/merge_chunk.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

Status MergeChunk(google::protobuf::Value& value,  // NOLINT(misc-no-recursion)
                  google::protobuf::Value&& chunk) {
  if (value.kind_case() != chunk.kind_case()) {
    return Status(StatusCode::kInvalidArgument, "mismatched types");
  }
  switch (value.kind_case()) {
    case google::protobuf::Value::kBoolValue:
    case google::protobuf::Value::kNumberValue:
    case google::protobuf::Value::kNullValue:
    case google::protobuf::Value::kStructValue:
      return Status(StatusCode::kInvalidArgument, "invalid type");

    case google::protobuf::Value::kStringValue: {
      *value.mutable_string_value() += chunk.string_value();
      return Status();
    }

    case google::protobuf::Value::kListValue: {
      auto& value_list = *value.mutable_list_value()->mutable_values();
      if (value_list.empty()) {
        value = std::move(chunk);
        return Status();
      }

      auto& chunk_list = *chunk.mutable_list_value()->mutable_values();
      if (chunk_list.empty()) {
        // There's nothing to merge.
        return Status();
      }

      // Recursively merge the last element of value_list with the first
      // element of chunk_list if necessary.
      auto value_it = value_list.rbegin();
      auto chunk_it = chunk_list.begin();
      if (value_it->kind_case() == google::protobuf::Value::kStringValue ||
          value_it->kind_case() == google::protobuf::Value::kListValue) {
        auto status = MergeChunk(*value_it, std::move(*chunk_it++));
        if (!status.ok()) return status;
      }

      // Moves all the remaining elements over.
      while (chunk_it != chunk_list.end()) {
        *value.mutable_list_value()->add_values() = std::move(*chunk_it++);
      }

      return Status();
    }

    default:
      break;
  }
  return Status(StatusCode::kUnknown, "unknown Value type");
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
