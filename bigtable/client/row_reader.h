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

#ifndef GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_READER_H_
#define GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_READER_H_

#include "bigtable/client/version.h"

#include "bigtable/client/internal/readrowsparser.h"
#include "bigtable/client/row.h"
#include "bigtable/client/table.h"

#include <google/bigtable/v2/bigtable.grpc.pb.h>

#include <absl/memory/memory.h>
#include <grpc++/grpc++.h>

#include <iterator>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Object returned by Table::ReadRows(), enumerates rows in the response.
 *
 *
 *
 * Example:
 * @code
 * @endcode
 *
 */
class RowReader {
  class RowReaderIterator;

 public:
  // TODO(#32): Add arguments for RowSet, rows limit, RowFilter
  RowReader(std::shared_ptr<DataClient> client, absl::string_view table_name);

  using iterator = RowReaderIterator;

  /**
   * Input iterator over rows in the response.
   *
   * All iterators that do not point to end() are equivalent, that is,
   * incrementing one iterator will increment all by advancing the
   * stream.
   */
  iterator begin();

  /// End iterator over the rows in the response.
  iterator end();

  /**
   * Stop the read call and clean up the connection.
   *
   * Invalidates iterators.
   */
  void Cancel();

 private:
  /**
   * Reads and parses the next row in the response.
   *
   * Invalidates the previous row and feeds response chunks into the
   * parser until another row is available.
   *
   * This call possibly blocks waiting for data.
   */
  void Advance();

  bool NextChunk();

  /// The input iterator returned by begin() and end()
  class RowReaderIterator : public std::iterator<std::input_iterator_tag, Row> {
   public:
    RowReaderIterator(RowReader* owner, bool is_end)
        : owner_(owner), is_end_(is_end) {}

    RowReaderIterator& operator++();

    const Row& operator*() { return owner_->rows_[0]; }
    const Row* operator->() { return &owner_->rows_[0]; }

    bool operator==(const RowReaderIterator& that) const {
      // All non-end iterators are equal.
      return is_end_ == that.is_end_;
    }

    bool operator!=(const RowReaderIterator& that) const {
      return !(*this == that);
    }

   private:
    RowReader* const owner_;
    bool is_end_;
  };

  std::shared_ptr<DataClient> client_;
  absl::string_view table_name_;
  std::unique_ptr<grpc::ClientContext> context_;

  std::unique_ptr<ReadRowsParser> parser_;
  std::unique_ptr<
      grpc::ClientReaderInterface<google::bigtable::v2::ReadRowsResponse>>
      stream_;
  google::bigtable::v2::ReadRowsResponse response_;
  int processed_chunks_;
  std::vector<Row> rows_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_READER_H_
