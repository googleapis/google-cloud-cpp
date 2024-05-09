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

#include "google/cloud/spanner/internal/merge_chunk.h"
#include "google/cloud/internal/make_status.h"

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

Status MergeChunk(google::protobuf::Value& value,  // NOLINT(misc-no-recursion)
                  google::protobuf::Value&& chunk) {
  if (value.kind_case() != chunk.kind_case()) {
    return internal::InvalidArgumentError("mismatched types", GCP_ERROR_INFO());
  }
  switch (value.kind_case()) {
    case google::protobuf::Value::kBoolValue:
    case google::protobuf::Value::kNumberValue:
    case google::protobuf::Value::kNullValue:
    case google::protobuf::Value::kStructValue:
      return internal::InvalidArgumentError("invalid type", GCP_ERROR_INFO());

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
  return internal::UnknownError("unknown Value type", GCP_ERROR_INFO());
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
