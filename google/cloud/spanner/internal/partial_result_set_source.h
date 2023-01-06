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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H

#include "google/cloud/spanner/internal/partial_result_set_reader.h"
#include "google/cloud/spanner/results.h"
#include "google/cloud/spanner/value.h"
#include "google/cloud/spanner/version.h"
#include "google/cloud/options.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/struct.pb.h>
#include <google/spanner/v1/spanner.pb.h>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * This class serves as a bridge between the gRPC `PartialResultSet` streaming
 * reader and the spanner `ResultSet`, and is used to iterate over the rows
 * returned from a read operation.
 */
class PartialResultSetSource : public ResultSourceInterface {
 public:
  /// Factory method to create a PartialResultSetSource.
  static StatusOr<std::unique_ptr<ResultSourceInterface>> Create(
      std::unique_ptr<PartialResultSetReader> reader);

  ~PartialResultSetSource() override;

  StatusOr<spanner::Row> NextRow() override;

  absl::optional<google::spanner::v1::ResultSetMetadata> Metadata() override {
    return metadata_;
  }

  absl::optional<google::spanner::v1::ResultSetStats> Stats() const override {
    return stats_;
  }

 private:
  explicit PartialResultSetSource(
      std::unique_ptr<PartialResultSetReader> reader);

  Status ReadFromStream();

  Options options_;
  std::unique_ptr<PartialResultSetReader> reader_;

  // The `PartialResultSet.metadata` we received in the first response, and
  // the column names it contained (which will be shared between rows).
  absl::optional<google::spanner::v1::ResultSetMetadata> metadata_;
  std::shared_ptr<std::vector<std::string>> columns_;

  // The `PartialResultSet.stats` received in the last response, corresponding
  // to the `QueryMode` implied by the particular streaming read/query type.
  absl::optional<google::spanner::v1::ResultSetStats> stats_;

  // `Row`s ready to be returned by `NextRow()`.
  std::deque<spanner::Row> rows_;

  // When engaged, the token we can use to resume the stream immediately after
  // any data in (or previously in) `rows_`. When disengaged, we have already
  // delivered data that would be replayed, so resumption is disabled until we
  // see a new token.
  absl::optional<std::string> resume_token_ = std::string{};

  // `Value`s that could be combined into `rows_` when we have enough to fill
  // an entire row, plus a token that would resume the stream after such rows.
  google::protobuf::RepeatedPtrField<google::protobuf::Value> values_;

  // Should the space used by `values_` get larger than this limit, we will
  // move complete rows into `rows_` and disable resumption until we see a
  // new token. During this time, an error in the stream will be returned by
  // `NextRow()`. No individual row in a result set can exceed 100 MiB, so we
  // set the default limit to twice that.
  std::size_t values_space_limit_ = 2 * 100 * (std::size_t{1} << 20);

  // `*values_.rbegin()` exists, but it is incomplete. The rest of the value
  // will be sent in subsequent `PartialResultSet` messages.
  bool values_back_incomplete_ = false;

  // The state of our PartialResultSetReader.
  enum : char {
    // `Read()` has yet to return nullopt.
    kReading,
    // `Read()` has returned nullopt, but we are yet to call `Finish()`.
    kEndOfStream,
    // `Finish()` has been called, which means `NextRow()` has returned
    // either an empty row or an error status.
    kFinished,
  } state_ = kReading;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_INTERNAL_PARTIAL_RESULT_SET_SOURCE_H
