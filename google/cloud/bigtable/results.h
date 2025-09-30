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

#include "google/cloud/bigtable/row.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Defines the interface for `RowStream` implementations.
 *
 * The `RowStream` class represents a stream of `Rows` returned from
 * `bigtable::Table::ReadRows()`. There are
 * different implementations depending the the RPC. Applications can also
 * mock this class when testing their code and mocking the `bigtable::Table`
 * behavior.
 */
class ResultSourceInterface {
 public:
  virtual ~ResultSourceInterface() = default;

  /**
   * Returns the next row in the stream.
   *
   * @return if the stream is interrupted due to a failure the
   *   `StatusOr<bigtable::Row>` contains the error.  The function returns a
   *   successful `StatusOr<>` with a `bigtable::Row` with an empty row_key()
   *   to indicate end-of-stream.
   */
  virtual StatusOr<bigtable::Row> NextRow() = 0;

  /**
   * Returns metadata about the result set.
   */
  virtual absl::optional<google::bigtable::v2::ResultSetMetadata>
  Metadata() = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_RESULTS_H
