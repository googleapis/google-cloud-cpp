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

#include "google/cloud/spanner/results.h"
#include "absl/types/optional.h"
#include <google/spanner/v1/result_set.pb.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

absl::optional<Timestamp> GetReadTimestamp(
    std::unique_ptr<spanner_internal::ResultSourceInterface> const& source) {
  auto metadata = source->Metadata();
  absl::optional<Timestamp> timestamp;
  if (metadata.has_value() && metadata->has_transaction() &&
      metadata->transaction().has_read_timestamp()) {
    auto ts = MakeTimestamp(metadata->transaction().read_timestamp());
    if (ts) timestamp = *std::move(ts);
  }
  return timestamp;
}

std::int64_t GetRowsModified(
    std::unique_ptr<spanner_internal::ResultSourceInterface> const& source) {
  auto stats = source->Stats();
  return stats ? stats->row_count_exact() : 0;
}

absl::optional<std::unordered_map<std::string, std::string>> GetExecutionStats(
    std::unique_ptr<spanner_internal::ResultSourceInterface> const& source) {
  auto stats = source->Stats();
  if (stats && stats->has_query_stats()) {
    std::unordered_map<std::string, std::string> execution_stats;
    for (auto const& entry : stats->query_stats().fields()) {
      execution_stats.insert(
          std::make_pair(entry.first, entry.second.string_value()));
    }
    return execution_stats;
  }
  return {};
}

absl::optional<spanner::ExecutionPlan> GetExecutionPlan(
    std::unique_ptr<spanner_internal::ResultSourceInterface> const& source) {
  auto stats = source->Stats();
  if (stats && stats->has_query_plan()) {
    return source->Stats()->query_plan();
  }
  return {};
}
}  // namespace

std::int64_t RowStream::RowsModified() const {
  return GetRowsModified(source_);
}

absl::optional<Timestamp> RowStream::ReadTimestamp() const {
  return GetReadTimestamp(source_);
}

std::int64_t DmlResult::RowsModified() const {
  return GetRowsModified(source_);
}

absl::optional<Timestamp> ProfileQueryResult::ReadTimestamp() const {
  return GetReadTimestamp(source_);
}

absl::optional<std::unordered_map<std::string, std::string>>
ProfileQueryResult::ExecutionStats() const {
  return GetExecutionStats(source_);
}

absl::optional<spanner::ExecutionPlan> ProfileQueryResult::ExecutionPlan()
    const {
  return GetExecutionPlan(source_);
}

std::int64_t ProfileDmlResult::RowsModified() const {
  return GetRowsModified(source_);
}

absl::optional<std::unordered_map<std::string, std::string>>
ProfileDmlResult::ExecutionStats() const {
  return GetExecutionStats(source_);
}

absl::optional<spanner::ExecutionPlan> ProfileDmlResult::ExecutionPlan() const {
  return GetExecutionPlan(source_);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
