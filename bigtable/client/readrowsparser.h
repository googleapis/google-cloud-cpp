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

#ifndef BIGTABLE_CLIENT_READROWSPARSER_H_
#define BIGTABLE_CLIENT_READROWSPARSER_H_

#include "bigtable/client/version.h"

#include "bigtable/client/cell.h"
#include "bigtable/client/row.h"

#include "google/bigtable/v2/bigtable.grpc.pb.h"

#include <absl/strings/string_view.h>
#include <vector>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * The internal module responsible for transforming ReadRowsResponse
 * protobufs into Row objects.
 *
 * Users are expected to do something like:
 *   while (!stream.EOT()) {
 *     chunk = stream.NextChunk();
 *     parser.HandleChunk(chunk);
 *     if (parser.HasNext()) {
 *       row = parser.Next();  // you now own `row`
 *     }
 *   }
 *   parser.HandleEOT();
 */
class ReadRowsParser {
 public:
  /**
   * Pass an input chunk proto to the parser. May throw errors, in
   * which case valid data read before the error is still accessible.
   */
  void HandleChunk(
      const google::bigtable::v2::ReadRowsResponse_CellChunk& chunk);

  /**
   * Signal that the input stream reached the end. May throw errors if
   * more data was expected, in which case valid data read before the
   * error is still accessible.
   */
  void HandleEOT();

  /**
   * True if the data parsed so far yielded a Row. Call Next() to take
   * the row. Throws an error if there is wrong or incomplete data in
   * the chunks parsed.
   */
  bool HasNext();

  /**
   *  Extract and take ownership of the data in a row. Use HasNext()
   *  first to find out if there are rows available.
   *  Throws runtime_error if HasNext() is false.
   */
  Row Next();

 private:
  /**
   *  Helper class to handle string ownership correctly. The value is
   *  moved when converting to a result cell, but the key, family and
   *  column are copied.
   */
  class ParseCell {
   public:
    /// Moves partial results into a Cell class.
    Cell MoveToCell() {
      Cell cell(row_, family_, column_, timestamp_, std::move(value_),
                std::move(labels_));
      value_ = "";
      labels_.clear();
      return cell;
    }

    std::string row_;
    std::string family_;
    std::string column_;
    int64_t timestamp_;
    std::string value_;
    std::vector<std::string> labels_;
  };

  // Row key for the current row.
  std::string row_key_;

  // Parsed cells of a yet unfinished row.
  std::vector<Cell> cells_;

  // Is the next incoming chunk the first in a cell?
  bool cell_first_chunk_;

  // Stores partial fields.
  ParseCell cell_;

  // Set when a row is ready.
  std::string last_seen_row_key_;

  // True iff cells_ make up a complete row.
  bool row_ready_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // BIGTABLE_CLIENT_READROWSPARSER_H_
