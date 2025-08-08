// Copyright 2022 Google LLC
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

#include "google/cloud/spanner/connection.h"
#include "google/cloud/spanner/query_partition.h"
#include "google/cloud/spanner/read_partition.h"

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

class StatusOnlyResultSetSource : public ResultSourceInterface {
 public:
  explicit StatusOnlyResultSetSource(Status status)
      : status_(std::move(status)) {}

  StatusOr<Row> NextRow() override { return status_; }
  absl::optional<google::spanner::v1::ResultSetMetadata> Metadata() override {
    return {};
  }
  absl::optional<google::spanner::v1::ResultSetStats> Stats() const override {
    return {};
  }
  absl::optional<google::spanner::v1::MultiplexedSessionPrecommitToken>
  PrecommitToken() const override {
    return absl::nullopt;
  }

 private:
  Status status_;
};

}  // namespace

// NOLINTNEXTLINE(performance-unnecessary-value-param)
RowStream Connection::Read(ReadParams) {
  return RowStream(std::make_unique<StatusOnlyResultSetSource>(
      Status(StatusCode::kUnimplemented, "not implemented")));
}

StatusOr<std::vector<ReadPartition>> Connection::PartitionRead(
    PartitionReadParams) {  // NOLINT(performance-unnecessary-value-param)
  return Status(StatusCode::kUnimplemented, "not implemented");
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
RowStream Connection::ExecuteQuery(SqlParams) {
  return RowStream(std::make_unique<StatusOnlyResultSetSource>(
      Status(StatusCode::kUnimplemented, "not implemented")));
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
StatusOr<DmlResult> Connection::ExecuteDml(SqlParams) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
ProfileQueryResult Connection::ProfileQuery(SqlParams) {
  return ProfileQueryResult(std::make_unique<StatusOnlyResultSetSource>(
      Status(StatusCode::kUnimplemented, "not implemented")));
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
StatusOr<ProfileDmlResult> Connection::ProfileDml(SqlParams) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
StatusOr<ExecutionPlan> Connection::AnalyzeSql(SqlParams) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<PartitionedDmlResult> Connection::ExecutePartitionedDml(
    ExecutePartitionedDmlParams) {  // NOLINT(performance-unnecessary-value-param)
  return Status(StatusCode::kUnimplemented, "not implemented");
}

StatusOr<std::vector<QueryPartition>> Connection::PartitionQuery(
    PartitionQueryParams) {  // NOLINT(performance-unnecessary-value-param)
  return Status(StatusCode::kUnimplemented, "not implemented");
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
StatusOr<BatchDmlResult> Connection::ExecuteBatchDml(ExecuteBatchDmlParams) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
StatusOr<CommitResult> Connection::Commit(CommitParams) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
Status Connection::Rollback(RollbackParams) {
  return Status(StatusCode::kUnimplemented, "not implemented");
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
BatchedCommitResultStream Connection::BatchWrite(BatchWriteParams) {
  return internal::MakeStreamRange<BatchedCommitResult>(
      [] { return Status(StatusCode::kUnimplemented, "not implemented"); });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google
