// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DEFAULT_ROW_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DEFAULT_ROW_READER_H

#include "google/cloud/bigtable/filters.h"
#include "google/cloud/bigtable/internal/bigtable_stub.h"
#include "google/cloud/bigtable/internal/readrowsparser.h"
#include "google/cloud/bigtable/internal/row_reader_impl.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/bigtable/row.h"
#include "google/cloud/bigtable/row_set.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "absl/types/variant.h"
#include <grpcpp/grpcpp.h>
#include <cinttypes>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * `RowReaderImpl` that interacts with the Bigtable service using a
 * `BigtableStub`.
 */
class DefaultRowReader : public RowReaderImpl {
 public:
  DefaultRowReader(std::shared_ptr<BigtableStub> stub,
                   std::string app_profile_id, std::string table_name,
                   bigtable::RowSet row_set, std::int64_t rows_limit,
                   bigtable::Filter filter,
                   std::unique_ptr<bigtable::DataRetryPolicy> retry_policy,
                   std::unique_ptr<BackoffPolicy> backoff_policy);

  ~DefaultRowReader() override;

  void Cancel() override;

  /**
   * Read and parse the next row in the response.
   *
   * @param row receives the next row on success, and is reset on failure or if
   * there are no more rows.
   *
   * This call possibly blocks waiting for data until a full row is available.
   */
  absl::variant<Status, bigtable::Row> Advance() override;

 private:
  /// Called by Advance(), does not handle retries.
  absl::variant<Status, bigtable::Row> AdvanceOrFail();

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

  std::shared_ptr<BigtableStub> stub_;
  std::string app_profile_id_;
  std::string table_name_;
  bigtable::RowSet row_set_;
  std::int64_t rows_limit_;
  bigtable::Filter filter_;
  std::unique_ptr<bigtable::DataRetryPolicy> retry_policy_;
  std::unique_ptr<BackoffPolicy> backoff_policy_;

  std::unique_ptr<grpc::ClientContext> context_;

  std::unique_ptr<bigtable::internal::ReadRowsParser> parser_;
  std::unique_ptr<
      internal::StreamingReadRpc<google::bigtable::v2::ReadRowsResponse>>
      stream_;
  bool stream_is_open_ = false;
  bool operation_cancelled_ = false;

  /// The end of stream Status.
  Status last_status_;
  /// The last received response, chunks are being parsed one by one from it.
  google::bigtable::v2::ReadRowsResponse response_;
  /// Number of chunks already parsed in response_.
  int processed_chunks_count_ = 0;

  /// Number of rows read so far, used to set row_limit in retries.
  std::int64_t rows_count_ = 0;
  /// Holds the last read row key, for retries.
  bigtable::RowKeyType last_read_row_key_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_DEFAULT_ROW_READER_H
