// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_READER_H

#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/filters.h"
#include "google/cloud/bigtable/internal/readrowsparser.h"
#include "google/cloud/bigtable/internal/row_reader_impl.h"
#include "google/cloud/bigtable/internal/rowreaderiterator.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/row_set.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
class RowReader;
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
bigtable::RowReader MakeRowReader(std::shared_ptr<RowReaderImpl> impl);
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

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

  GOOGLE_CLOUD_CPP_BIGTABLE_ROW_READER_CTOR_DEPRECATED()
  RowReader(std::shared_ptr<DataClient> client, std::string table_name,
            RowSet row_set, std::int64_t rows_limit, Filter filter,
            std::unique_ptr<RPCRetryPolicy> retry_policy,
            std::unique_ptr<RPCBackoffPolicy> backoff_policy,
            MetadataUpdatePolicy metadata_update_policy,
            std::unique_ptr<internal::ReadRowsParserFactory> parser_factory);

  GOOGLE_CLOUD_CPP_BIGTABLE_ROW_READER_CTOR_DEPRECATED()
  RowReader(std::shared_ptr<DataClient> client, std::string app_profile_id,
            std::string table_name, RowSet row_set, std::int64_t rows_limit,
            Filter filter, std::unique_ptr<RPCRetryPolicy> retry_policy,
            std::unique_ptr<RPCBackoffPolicy> backoff_policy,
            MetadataUpdatePolicy metadata_update_policy,
            std::unique_ptr<internal::ReadRowsParserFactory> parser_factory);

  RowReader(RowReader&&) = default;

  ~RowReader() = default;

  using iterator = bigtable_internal::RowReaderIterator;
  friend class bigtable_internal::RowReaderIterator;

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
  friend RowReader bigtable_internal::MakeRowReader(
      std::shared_ptr<bigtable_internal::RowReaderImpl>);
  explicit RowReader(std::shared_ptr<bigtable_internal::RowReaderImpl> impl)
      : impl_(std::move(impl)) {}

  std::shared_ptr<bigtable_internal::RowReaderImpl> impl_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ROW_READER_H
