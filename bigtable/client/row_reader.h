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

#include "bigtable/client/data_client.h"
#include "bigtable/client/filters.h"
#include "bigtable/client/internal/readrowsparser.h"
#include "bigtable/client/row.h"
#include "bigtable/client/row_set.h"
#include "bigtable/client/rpc_backoff_policy.h"
#include "bigtable/client/rpc_retry_policy.h"

#include <google/bigtable/v2/bigtable.grpc.pb.h>

#include <absl/memory/memory.h>
#include <absl/types/optional.h>
#include <grpc++/grpc++.h>

#include <iterator>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Object returned by Table::ReadRows(), enumerates rows in the response.
 *
 * Iterate over the results of ReadRows() using the STL idioms.
 */
class RowReader {
  class RowReaderIterator;

 public:
  /// Signifies that there is no limit on the number of rows to read.
  static int constexpr NO_ROWS_LIMIT = 0;

  RowReader(std::shared_ptr<DataClient> client, absl::string_view table_name,
            RowSet row_set, int rows_limit, Filter filter,
            std::unique_ptr<RPCRetryPolicy> retry_policy,
            std::unique_ptr<RPCBackoffPolicy> backoff_policy);

  using iterator = RowReaderIterator;

  /**
   * Input iterator over rows in the response.
   *
   * The returned iterator is a single-pass input iterator that reads
   * rows from the RowReader when incremented. The first row may be
   * read when the iterator is constructed.
   *
   * Creating, and particularly incrementing, multiple iterators on
   * the same RowReader is unsupported and can produce incorrect
   * results.
   *
   * Retry and backoff policies are honored.
   *
   * @throws std::runtime_error if the read failed after retries.
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
   * Read and parse the next row in the response.
   *
   * Invalidates the previous row and feeds response chunks into the
   * parser until another row is available.
   *
   * This call possibly blocks waiting for data.
   */
  void Advance();

  /// Called by Advance(), does not handle retries.
  grpc::Status AdvanceOrFail();

  /**
   * Move the index to the next chunk, reading data if needed.
   *
   * Returns false if no more chunks are available.
   */
  bool NextChunk();

  /// Sends the ReadRows request to the stub.
  void MakeRequest();

  /// The input iterator returned by begin() and end()
  class RowReaderIterator : public std::iterator<std::input_iterator_tag, Row> {
   public:
    RowReaderIterator(RowReader* owner, bool is_end)
        : owner_(owner), is_end_(is_end) {}

    RowReaderIterator& operator++();

    Row const& operator*() { return owner_->row_.operator*(); }
    Row const* operator->() { return owner_->row_.operator->(); }

    bool operator==(RowReaderIterator const& that) const {
      // All non-end iterators are equal.
      return is_end_ == that.is_end_;
    }

    bool operator!=(RowReaderIterator const& that) const {
      return !(*this == that);
    }

   private:
    RowReader* owner_;
    bool is_end_;
  };

  std::shared_ptr<DataClient> client_;
  std::string table_name_;
  RowSet row_set_;
  int rows_limit_;
  Filter filter_;
  std::unique_ptr<RPCRetryPolicy> retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> backoff_policy_;

  std::unique_ptr<grpc::ClientContext> context_;

  std::unique_ptr<ReadRowsParser> parser_;
  std::unique_ptr<
      grpc::ClientReaderInterface<google::bigtable::v2::ReadRowsResponse>>
      stream_;

  /// The last received response, chunks are being parsed one by one from it.
  google::bigtable::v2::ReadRowsResponse response_;
  /// Number of chunks already parsed in response_.
  int processed_chunks_;

  /// Number of rows read so far, used to set row_limit in retries.
  int rows_count_;

  /// Holds the last read row, non-end() iterators all point to it.
  absl::optional<Row> row_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // GOOGLE_CLOUD_CPP_BIGTABLE_CLIENT_ROW_READER_H_
