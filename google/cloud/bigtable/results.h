// Copyright 2025 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RESULTS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RESULTS_H

#include "google/cloud/bigtable/query_row.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/status_or.h"
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Defines the interface for `RowStream` implementations.
 *
 * The `RowStream` class represents a stream of `QueryRows` returned from
 * `bigtable::Client::ExecuteQuery()`.
 */
class ResultSourceInterface {
 public:
  virtual ~ResultSourceInterface() = default;

  /**
   * Returns the next row in the stream.
   *
   * @return if the stream is interrupted due to a failure the
   *   `StatusOr<bigtable::QueryRow>` contains the error.  The function returns
   * a successful `StatusOr<>` with a `bigtable::QueryRow` with an empty
   * row_key() to indicate end-of-stream.
   */
  virtual StatusOr<bigtable::QueryRow> NextRow() = 0;

  /**
   * Returns metadata about the result set.
   */
  virtual absl::optional<google::bigtable::v2::ResultSetMetadata>
  Metadata() = 0;
};

/**
 * Represents the stream of `QueryRows` returned from
 * `bigtable::Client::ExecuteQuery`.
 *
 */
class RowStream {
 public:
  RowStream() = default;
  explicit RowStream(std::unique_ptr<ResultSourceInterface> source)
      : source_(std::move(source)) {}

  // This class is movable but not copyable.
  RowStream(RowStream&&) = default;
  RowStream& operator=(RowStream&&) = default;

  /// Returns a `RowStreamIterator` defining the beginning of this range.
  RowStreamIterator begin() {
    return RowStreamIterator(
        [this]() mutable { return source_->NextRow(); });
  }

  /// Returns a `RowStreamIterator` defining the end of this range.
  RowStreamIterator end() { return {}; }

 private:
  std::unique_ptr<ResultSourceInterface> source_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RESULTS_H
