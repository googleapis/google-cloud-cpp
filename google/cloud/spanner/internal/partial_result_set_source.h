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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H

#include "google/cloud/spanner/internal/partial_result_set_reader.h"
#include "google/cloud/spanner/results.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <google/spanner/v1/spanner.grpc.pb.h>
#include <google/spanner/v1/spanner.pb.h>
#include <grpcpp/grpcpp.h>
#include <deque>
#include <memory>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

/**
 * This class serves as a bridge between the gRPC `PartialResultSet` streaming
 * reader and the spanner `ResultSet`, which is used to iterate over the rows
 * returned from a read operation.
 */
class PartialResultSetSource : public internal::ResultSourceInterface {
 public:
  /// Factory method to create a PartialResultSetSource.
  static StatusOr<std::unique_ptr<ResultSourceInterface>> Create(
      std::unique_ptr<PartialResultSetReader> reader);

  ~PartialResultSetSource() override;

  StatusOr<Row> NextRow() override;

  absl::optional<google::spanner::v1::ResultSetMetadata> Metadata() override {
    return metadata_;
  }

  absl::optional<google::spanner::v1::ResultSetStats> Stats() const override {
    return stats_;
  }

 private:
  explicit PartialResultSetSource(
      std::unique_ptr<PartialResultSetReader> reader)
      : reader_(std::move(reader)) {}

  Status ReadFromStream();

  std::unique_ptr<PartialResultSetReader> reader_;
  absl::optional<google::spanner::v1::ResultSetMetadata> metadata_;
  absl::optional<google::spanner::v1::ResultSetStats> stats_;
  std::deque<google::protobuf::Value> buffer_;
  absl::optional<google::protobuf::Value> chunk_;
  std::shared_ptr<std::vector<std::string>> columns_;
  bool finished_ = false;
};

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H
