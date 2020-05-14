// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_READROWSPARSER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_READROWSPARSER_H

#include "google/cloud/bigtable/cell.h"
#include "google/cloud/bigtable/row.h"
#include "google/cloud/bigtable/version.h"
#include "absl/memory/memory.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
/**
 * Transforms a stream of chunks as returned by the ReadRows streaming
 * RPC into a sequence of rows.
 *
 * A simplified example of correctly using this class:
 *
 * @code
 * while (!stream.End()) {
 *   chunk = stream.NextChunk();
 *   parser.HandleChunk(chunk);
 *   if (parser.HasNext()) {
 *     row = parser.Next();  // you now own `row`
 *   }
 * }
 * parser.HandleEndOfStream();
 * @endcode
 *
 * NO RECYCLING of the parser object: This is a stateful class, and a
 * single and unique parser should be used for each stream of ReadRows
 * responses. If errors occur, an exception is thrown as documented by
 * each method and the parser object is left in an undefined state.
 */
class ReadRowsParser {
 public:
  ReadRowsParser() : row_key_(""), last_seen_row_key_("") {}

  virtual ~ReadRowsParser() = default;

  /**
   * Pass an input chunk proto to the parser.
   *
   * @throws std::runtime_error if called while a row is available
   * (HasNext() is true).
   *
   * @throws std::runtime_error if validation failed.
   */
  virtual void HandleChunk(
      google::bigtable::v2::ReadRowsResponse_CellChunk chunk,
      grpc::Status& status);

  /**
   * Signal that the input stream reached the end.
   *
   * @throws std::runtime_error if more data was expected to finish
   * the current row.
   */
  virtual void HandleEndOfStream(grpc::Status& status);

  /**
   * True if the data parsed so far yielded a Row.
   *
   * Call Next() to take the row.
   */
  virtual bool HasNext() const;

  /**
   * Extract and take ownership of the data in a row.
   *
   * Use HasNext() first to find out if there are rows available.
   *
   * @throws std::runtime_error if HasNext() is false.
   */
  virtual Row Next(grpc::Status& status);

 private:
  /// Holds partially formed data until a full Row is ready.
  struct ParseCell {
    RowKeyType row;
    std::string family;
    ColumnQualifierType column;
    int64_t timestamp;
    CellValueType value;
    std::vector<std::string> labels;
  };

  /**
   * Moves partial results into a Cell class.
   *
   * Also helps handle string ownership correctly. The value is moved
   * when converting to a result cell, but the key, family and column
   * are copied, because they are possibly reused by following cells.
   */
  Cell MovePartialToCell();

  /// Row key for the current row.
  RowKeyType row_key_;

  /// Parsed cells of a yet unfinished row.
  std::vector<Cell> cells_;

  /// Is the next incoming chunk the first in a cell?
  bool cell_first_chunk_{true};

  /// Stores partial fields.
  ParseCell cell_;

  /// Set when a row is ready.
  RowKeyType last_seen_row_key_;

  /// True iff cells_ make up a complete row.
  bool row_ready_{false};

  /// Have we received the end of stream call?
  bool end_of_stream_{false};
};

/// Factory for creating parser instances, defined for testability.
class ReadRowsParserFactory {
 public:
  virtual ~ReadRowsParserFactory() = default;

  /// Returns a newly created parser instance.
  virtual std::unique_ptr<ReadRowsParser> Create() {
    return absl::make_unique<ReadRowsParser>();
  }
};
}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_READROWSPARSER_H
