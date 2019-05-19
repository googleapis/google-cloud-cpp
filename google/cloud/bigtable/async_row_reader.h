// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_ROW_READER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_ROW_READER_H_

#include "google/cloud/bigtable/bigtable_strong_types.h"
#include "google/cloud/bigtable/completion_queue.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/filters.h"
#include "google/cloud/bigtable/internal/readrowsparser.h"
#include "google/cloud/bigtable/internal/rowreaderiterator.h"
#include "google/cloud/bigtable/metadata_update_policy.h"
#include "google/cloud/bigtable/row.h"
#include "google/cloud/bigtable/row_set.h"
#include "google/cloud/bigtable/rpc_backoff_policy.h"
#include "google/cloud/bigtable/rpc_retry_policy.h"
#include "google/cloud/bigtable/table_strong_types.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/future.h"
#include "google/cloud/optional.h"
#include "google/cloud/status_or.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <queue>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * Objects of this class represent the state of reading rows via AsyncReadRows.
 *
 * @warning This is an early version of the asynchronous APIs for Cloud
 *     Bigtable. These APIs might be changed in backward-incompatible ways. It
 *     is not subject to any SLA or deprecation policy.
 */
class AsyncRowReader : public std::enable_shared_from_this<AsyncRowReader> {
 public:
  static std::int64_t constexpr NO_ROWS_LIMIT = 0;
  // Callbacks keep pointers to these objects.
  AsyncRowReader(AsyncRowReader&&) = delete;
  AsyncRowReader(AsyncRowReader const&) = delete;

  ~AsyncRowReader();

  /**
   * Asynchronously obtain next row from a requested range.
   *
   * One can call this function many times, even before the first row is
   * fetched. The futures will be satisfied in the order they were obtained. In
   * case the stream ends either with an error or successfully, all further
   * futures will be satisfied with either that error or empty optional
   * indicating a successful end of stream.
   *
   * @return a future which will be satisfied once (a) a row is fetched - in
   *     such a case the future contains the row, or (b) end of range is reached
   *     - in such a case and empty optional is returned, or (c) an unretriable
   *     error occurs or retry policy is exhausted.
   */
  future<StatusOr<optional<Row>>> Next();

 private:
  using Response = StatusOr<optional<Row>>;
  static std::shared_ptr<AsyncRowReader> Create(
      CompletionQueue cq, std::shared_ptr<DataClient> client,
      bigtable::AppProfileId app_profile_id, bigtable::TableId table_name,
      RowSet row_set, std::int64_t rows_limit, Filter filter,
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::unique_ptr<internal::ReadRowsParserFactory> parser_factory);

  AsyncRowReader(
      CompletionQueue cq, std::shared_ptr<DataClient> client,
      bigtable::AppProfileId app_profile_id, bigtable::TableId table_name,
      RowSet row_set, std::int64_t rows_limit, Filter filter,
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::unique_ptr<internal::ReadRowsParserFactory> parser_factory);

  void MakeRequest();
  /// Called when lower layers provide us with a response chunk.
  future<bool> OnDataReceived(google::bigtable::v2::ReadRowsResponse response);
  /// Called when the whole stream finishes.
  void OnStreamFinished(Status status);
  /**
   * Enter a terminal state of the whole scan. No more attempts to read more
   * data will be made.
   */
  void FinishScan(Status status, std::unique_lock<std::mutex>& lk);
  /// Process everything that is accumulated in the parser.
  Status DrainParser();
  /// Parse the data from the response.
  Status ConsumeResponse(google::bigtable::v2::ReadRowsResponse response);

  friend class Table;

  std::mutex mu_;
  CompletionQueue cq_;
  std::shared_ptr<DataClient> client_;
  bigtable::AppProfileId app_profile_id_;
  bigtable::TableId table_name_;
  RowSet row_set_;
  std::int64_t rows_limit_;
  Filter filter_;
  std::unique_ptr<RPCRetryPolicy> rpc_retry_policy_;
  std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy_;
  MetadataUpdatePolicy metadata_update_policy_;
  std::unique_ptr<internal::ReadRowsParserFactory> parser_factory_;
  std::unique_ptr<internal::ReadRowsParser> parser_;
  /// Number of rows read so far, used to set row_limit in retries.
  std::int64_t rows_count_;
  /// Holds the last read row key, for retries.
  std::string last_read_row_key_;
  /// The queue of rows which we already received but no one has asked for them.
  std::queue<Row> ready_rows_;
  /**
   * The promises of rows which we have made, but couldn't satisfy yet.
   *
   * We want this object to exist for as long as we haven't satisfied all the
   * promises it made or the user has a pointer to it. This means that we want
   * to interrupt the stream of responses when we detect that the user has no
   * more pointers to this object.
   *
   * Usually we'd give both the user and the underlying layers `shared_ptr<>`s
   * to this object. This wouldn't allow us to easily tell, whether the user
   * holds any pointer to it. In order to workaround it, the lower layers will
   * hold `weak_ptr<>`s, so in order to extend this object's lifetime until all
   * promises are satisfied, we keep a self-reference next to every unsatisfied
   * promise.
   */
  std::queue<std::pair<promise<Response>, std::shared_ptr<AsyncRowReader>>>
      promised_results_;
  /**
   * The promise to the underlying stream to either continue reading or cancel.
   *
   * If the optional is empty, it means that either the whole scan is finished
   * or the underlying layers are already trying to fetch more data.
   *
   * If the optional is not empty, the lower layers are waiting for this to be
   * satisfied before they start fetching more data.
   */
  optional<promise<bool>> continue_reading_;
  /// The final status of the operation.
  optional<Status> whole_op_finished_;
  /**
   * Override for overall stream status.
   *
   * If an error occurs while parsing the incoming chunks, we should stop and
   * potentially retry. However, if we instruct the lower layers to prematurely
   * finish the stream, the stream status will not reflect what the reason for
   * finishing it was. In order to workaround it, we store the actual reason in
   * this member. If it is not OK, the logic deciding whether to retry, should
   * consider this status, rather than what the lower layers return as the
   * stream status.
   */
  Status stream_res_override_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_ROW_READER_H_
