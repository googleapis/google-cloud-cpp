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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LEGACY_ASYNC_ROW_READER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LEGACY_ASYNC_ROW_READER_H

#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/filters.h"
#include "google/cloud/bigtable/internal/readrowsparser.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/row.h"
#include "google/cloud/bigtable/row_set.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <chrono>
#include <queue>
#include <string>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Objects of this class represent the state of reading rows via AsyncReadRows.
 */
class LegacyAsyncRowReader
    : public std::enable_shared_from_this<LegacyAsyncRowReader> {
  using RowFunctor = std::function<future<bool>(bigtable::Row)>;
  using FinishFunctor = std::function<void(Status)>;

 public:
  /// Special value to be used as rows_limit indicating no limit.
  // NOLINTNEXTLINE(readability-identifier-naming)
  static std::int64_t constexpr NO_ROWS_LIMIT = 0;
  // Callbacks keep pointers to these objects.
  LegacyAsyncRowReader(LegacyAsyncRowReader&&) = delete;
  LegacyAsyncRowReader(LegacyAsyncRowReader const&) = delete;

  static std::shared_ptr<LegacyAsyncRowReader> Create(
      CompletionQueue cq, std::shared_ptr<bigtable::DataClient> client,
      std::string app_profile_id, std::string table_name, RowFunctor on_row,
      FinishFunctor on_finish, bigtable::RowSet row_set,
      std::int64_t rows_limit, bigtable::Filter filter,
      std::unique_ptr<bigtable::RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<bigtable::RPCBackoffPolicy> rpc_backoff_policy,
      bigtable::MetadataUpdatePolicy metadata_update_policy,
      std::unique_ptr<bigtable::internal::ReadRowsParserFactory>
          parser_factory) {
    std::shared_ptr<LegacyAsyncRowReader> res(new LegacyAsyncRowReader(
        std::move(cq), std::move(client), std::move(app_profile_id),
        std::move(table_name), std::move(on_row), std::move(on_finish),
        std::move(row_set), rows_limit, std::move(filter),
        std::move(rpc_retry_policy), std::move(rpc_backoff_policy),
        std::move(metadata_update_policy), std::move(parser_factory)));
    res->MakeRequest();
    return res;
  }

 private:
  LegacyAsyncRowReader(
      CompletionQueue cq, std::shared_ptr<bigtable::DataClient> client,
      std::string app_profile_id, std::string table_name, RowFunctor on_row,
      FinishFunctor on_finish, bigtable::RowSet row_set,
      std::int64_t rows_limit, bigtable::Filter filter,
      std::unique_ptr<bigtable::RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<bigtable::RPCBackoffPolicy> rpc_backoff_policy,
      bigtable::MetadataUpdatePolicy metadata_update_policy,
      std::unique_ptr<bigtable::internal::ReadRowsParserFactory> parser_factory)
      : cq_(std::move(cq)),
        client_(std::move(client)),
        app_profile_id_(std::move(app_profile_id)),
        table_name_(std::move(table_name)),
        on_row_(std::move(on_row)),
        on_finish_(std::move(on_finish)),
        row_set_(std::move(row_set)),
        rows_limit_(rows_limit),
        filter_(std::move(filter)),
        rpc_retry_policy_(std::move(rpc_retry_policy)),
        rpc_backoff_policy_(std::move(rpc_backoff_policy)),
        metadata_update_policy_(std::move(metadata_update_policy)),
        parser_factory_(std::move(parser_factory)) {}

  void MakeRequest();

  /**
   * Called when the user asks for more rows via satisfying the future returned
   * from the row callback.
   */
  void UserWantsRows() { TryGiveRowToUser(); }

  /**
   * Attempt to call a user callback.
   *
   * If no rows are ready, this will not call the callback immediately and
   * instead ask lower layers for more data.
   */
  void TryGiveRowToUser();

  /// Called when lower layers provide us with a response chunk.
  future<bool> OnDataReceived(google::bigtable::v2::ReadRowsResponse response);

  /// Called when the whole stream finishes.
  void OnStreamFinished(Status status);

  /// User satisfied the future returned from the row callback with false.
  void Cancel(std::string const& reason);

  /// Process everything that is accumulated in the parser.
  Status DrainParser();

  /// Parse the data from the response.
  Status ConsumeResponse(google::bigtable::v2::ReadRowsResponse response);

  std::mutex mu_;
  CompletionQueue cq_;
  std::shared_ptr<bigtable::DataClient> client_;
  std::string app_profile_id_;
  std::string table_name_;
  RowFunctor on_row_;
  FinishFunctor on_finish_;
  bigtable::RowSet row_set_;
  std::int64_t rows_limit_;
  bigtable::Filter filter_;
  std::unique_ptr<bigtable::RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<bigtable::RPCBackoffPolicy> rpc_backoff_policy_;
  bigtable::MetadataUpdatePolicy metadata_update_policy_;
  std::unique_ptr<bigtable::internal::ReadRowsParserFactory> parser_factory_;
  std::unique_ptr<bigtable::internal::ReadRowsParser> parser_;
  /// Number of rows read so far, used to set row_limit in retries.
  std::int64_t rows_count_ = 0;
  /// Holds the last read row key, for retries.
  bigtable::RowKeyType last_read_row_key_;
  /// The queue of rows which we already received but no one has asked for them.
  std::queue<bigtable::Row> ready_rows_;
  /**
   * The promise to the underlying stream to either continue reading or cancel.
   *
   * If the `absl::optional` is empty, it means that either the whole scan is
   * finished or the underlying layers are already trying to fetch more data.
   *
   * If the `absl::optional` is not empty, the lower layers are waiting for this
   * to be satisfied before they start fetching more data.
   */
  absl::optional<promise<bool>> continue_reading_;
  /// The final status of the operation.
  bool whole_op_finished_ = false;
  /**
   * The status of the last retry attempt_.
   *
   * It is reset to OK at the beginning of every retry. If an error is
   * encountered (be it while parsing the response or on stream finish), it is
   * stored here (unless a different error had already been stored).
   */
  Status status_;
  /// Tracks the level of recursion of TryGiveRowToUser
  int recursion_level_ = 0;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_LEGACY_ASYNC_ROW_READER_H
