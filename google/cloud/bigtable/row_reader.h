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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_READER_H

#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/filters.h"
#include "google/cloud/bigtable/internal/readrowsparser.h"
#include "google/cloud/bigtable/internal/rowreaderiterator.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/row.h"
#include "google/cloud/bigtable/row_set.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/optional.h"
#include "absl/types/optional.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <grpcpp/grpcpp.h>
#include <cinttypes>
#include <iterator>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Object returned by Table::ReadRows(), enumerates rows in the response.
 *
 * @par Thread-safety
 * Two threads operating concurrently on the same instance of this class or the
 * iterators obtained from it are **not** guaranteed to work.
 *
 * Iterate over the results of ReadRows() using the STL idioms.
 */
class RowReader {
 public:
  /**
   * A constant for the magic value that means "no limit, get all rows".
   *
   * Zero is used as a magic value that means "get all rows" in the
   * Cloud Bigtable RPC protocol.
   */
  // NOLINTNEXTLINE(readability-identifier-naming)
  static std::int64_t constexpr NO_ROWS_LIMIT = 0;

  RowReader(std::shared_ptr<DataClient> client, std::string table_name,
            RowSet row_set, std::int64_t rows_limit, Filter filter,
            std::unique_ptr<RPCRetryPolicy> retry_policy,
            std::unique_ptr<RPCBackoffPolicy> backoff_policy,
            MetadataUpdatePolicy metadata_update_policy,
            std::unique_ptr<internal::ReadRowsParserFactory> parser_factory);

  RowReader(std::shared_ptr<DataClient> client, std::string app_profile_id,
            std::string table_name, RowSet row_set, std::int64_t rows_limit,
            Filter filter, std::unique_ptr<RPCRetryPolicy> retry_policy,
            std::unique_ptr<RPCBackoffPolicy> backoff_policy,
            MetadataUpdatePolicy metadata_update_policy,
            std::unique_ptr<internal::ReadRowsParserFactory> parser_factory);

  RowReader(RowReader&&) noexcept = default;

  ~RowReader();

  using iterator = internal::RowReaderIterator;
  friend class internal::RowReaderIterator;

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
   */
  iterator begin();

  /// End iterator over the rows in the response.
  iterator end();

  /**
   * Gracefully terminate a streaming read.
   *
   * Invalidates iterators.
   */
  void Cancel();

 private:
  using OptionalRow = absl::optional<Row>;

  /**
   * Read and parse the next row in the response.
   *
   * @param row receives the next row on success, and is reset on failure or if
   * there are no more rows.
   *
   * This call possibly blocks waiting for data until a full row is available.
   */
  StatusOr<OptionalRow> Advance();

  /// Called by Advance(), does not handle retries.
  grpc::Status AdvanceOrFail(OptionalRow& row);

  /**
   * Move the `processed_chunks_count_` index to the next chunk,
   * reading data if needed.
   *
   * Returns false if no more chunks are available.
   *
   * This call is used internally by AdvanceOrFail to prepare data for
   * parsing. When it returns true, the value of
   * `response_.chunks(processed_chunks_count_)` is valid and holds
   * the next chunk to parse.
   */
  bool NextChunk();

  /// Sends the ReadRows request to the stub.
  void MakeRequest();

  std::shared_ptr<DataClient> client_;
  std::string app_profile_id_;
  std::string table_name_;
  RowSet row_set_;
  std::int64_t rows_limit_;
  Filter filter_;
  std::unique_ptr<RPCRetryPolicy> retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> backoff_policy_;
  MetadataUpdatePolicy metadata_update_policy_;

  std::unique_ptr<grpc::ClientContext> context_;

  std::unique_ptr<internal::ReadRowsParserFactory> parser_factory_;
  std::unique_ptr<internal::ReadRowsParser> parser_;
  std::unique_ptr<
      grpc::ClientReaderInterface<google::bigtable::v2::ReadRowsResponse>>
      stream_;
  bool stream_is_open_;
  bool operation_cancelled_;

  /// The last received response, chunks are being parsed one by one from it.
  google::bigtable::v2::ReadRowsResponse response_;
  /// Number of chunks already parsed in response_.
  int processed_chunks_count_;

  /// Number of rows read so far, used to set row_limit in retries.
  std::int64_t rows_count_;
  /// Holds the last read row key, for retries.
  RowKeyType last_read_row_key_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_READER_H
