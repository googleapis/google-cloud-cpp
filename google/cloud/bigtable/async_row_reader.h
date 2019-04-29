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
 */
class AsyncRowReader : public std::enable_shared_from_this<AsyncRowReader> {
 public:
  static std::int64_t constexpr NO_ROWS_LIMIT = 0;
  // Callbacks keep pointers to these objects.
  AsyncRowReader(AsyncRowReader&&) = delete;
  AsyncRowReader(AsyncRowReader const&) = delete;

  ~AsyncRowReader();

  static std::shared_ptr<AsyncRowReader> Create(
      CompletionQueue cq, std::shared_ptr<DataClient> client,
      bigtable::AppProfileId app_profile_id, bigtable::TableId table_name,
      RowSet row_set, std::int64_t rows_limit, Filter filter,
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::unique_ptr<internal::ReadRowsParserFactory> parser_factory);

  using Response = StatusOr<optional<Row>>;
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
  future<Response> Next();

 private:
  AsyncRowReader(
      CompletionQueue cq, std::shared_ptr<DataClient> client,
      bigtable::AppProfileId app_profile_id, bigtable::TableId table_name,
      RowSet row_set, std::int64_t rows_limit, Filter filter,
      std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::unique_ptr<internal::ReadRowsParserFactory> parser_factory);

  void MakeRequest();
  future<bool> OnDataReceived(google::bigtable::v2::ReadRowsResponse response);
  void OnStreamFinished(Status status);
  void OperationComplete(Status status, std::unique_lock<std::mutex>& lk);
  Status ConsumeFromParser();
  Status ConsumeResponse(google::bigtable::v2::ReadRowsResponse response);

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
  /// The promises of rows which we have made, but couldn't satisfy yet.
  std::queue<promise<Response>> promised_results_;
  /// The promise to the underlying stream to either continue reading or cancel.
  optional<promise<bool>> continue_reading_;
  /// The final status of the operation.
  optional<Status> whole_op_finished_;
  /// On end of stream, consider this error rather than what gRPC returns.
  Status stream_res_override_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_ROW_READER_H_
