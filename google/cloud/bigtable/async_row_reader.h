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
 */
template <typename RowFunctor, typename FinishFunctor>
class AsyncRowReader : public std::enable_shared_from_this<
                           AsyncRowReader<RowFunctor, FinishFunctor>> {
 public:
  /// Special value to be used as rows_limit indicating no limit.
  static std::int64_t constexpr NO_ROWS_LIMIT = 0;
  // Callbacks keep pointers to these objects.
  AsyncRowReader(AsyncRowReader&&) = delete;
  AsyncRowReader(AsyncRowReader const&) = delete;

 private:
  static_assert(google::cloud::internal::is_invocable<RowFunctor, Row>::value,
                "RowFunctor must be invocable with Row.");
  static_assert(
      google::cloud::internal::is_invocable<FinishFunctor, Status>::value,
      "RowFunctor must be invocable with Status.");
  static_assert(
      std::is_same<google::cloud::internal::invoke_result_t<RowFunctor, Row>,
                   future<bool>>::value,
      "RowFunctor should return a future<bool>.");

  static std::shared_ptr<AsyncRowReader> Create(
      CompletionQueue cq, std::shared_ptr<DataClient> client,
      std::string app_profile_id, std::string table_name, RowFunctor on_row,
      FinishFunctor on_finish, RowSet row_set, std::int64_t rows_limit,
      Filter filter, std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::unique_ptr<internal::ReadRowsParserFactory> parser_factory) {
    std::shared_ptr<AsyncRowReader> res(new AsyncRowReader(
        std::move(cq), std::move(client), std::move(app_profile_id),
        std::move(table_name), std::move(on_row), std::move(on_finish),
        std::move(row_set), rows_limit, std::move(filter),
        std::move(rpc_retry_policy), std::move(rpc_backoff_policy),
        std::move(metadata_update_policy), std::move(parser_factory)));
    res->MakeRequest();
    return res;
  }

  AsyncRowReader(
      CompletionQueue cq, std::shared_ptr<DataClient> client,
      std::string app_profile_id, std::string table_name, RowFunctor on_row,
      FinishFunctor on_finish, RowSet row_set, std::int64_t rows_limit,
      Filter filter, std::unique_ptr<RPCRetryPolicy> rpc_retry_policy,
      std::unique_ptr<RPCBackoffPolicy> rpc_backoff_policy,
      MetadataUpdatePolicy metadata_update_policy,
      std::unique_ptr<internal::ReadRowsParserFactory> parser_factory)
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
        parser_factory_(std::move(parser_factory)),
        rows_count_(0),
        whole_op_finished_(),
        recursion_level_() {}

  void MakeRequest() {
    status_ = Status();
    google::bigtable::v2::ReadRowsRequest request;

    request.set_app_profile_id(app_profile_id_);
    request.set_table_name(table_name_);
    auto row_set_proto = row_set_.as_proto();
    request.mutable_rows()->Swap(&row_set_proto);

    auto filter_proto = filter_.as_proto();
    request.mutable_filter()->Swap(&filter_proto);

    if (rows_limit_ != NO_ROWS_LIMIT) {
      request.set_rows_limit(rows_limit_ - rows_count_);
    }
    parser_ = parser_factory_->Create();

    auto context = google::cloud::internal::make_unique<grpc::ClientContext>();
    rpc_retry_policy_->Setup(*context);
    rpc_backoff_policy_->Setup(*context);
    metadata_update_policy_.Setup(*context);

    auto client = client_;
    auto self = this->shared_from_this();
    cq_.MakeStreamingReadRpc(
        [client](grpc::ClientContext* context,
                 google::bigtable::v2::ReadRowsRequest const& request,
                 grpc::CompletionQueue* cq) {
          return client->PrepareAsyncReadRows(context, request, cq);
        },
        request, std::move(context),
        [self](google::bigtable::v2::ReadRowsResponse r) {
          return self->OnDataReceived(std::move(r));
        },
        [self](Status s) { self->OnStreamFinished(std::move(s)); });
  }

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
  void TryGiveRowToUser() {
    // The user is likely to ask for more rows immediately after receiving a
    // row, which means that this function will be called recursively. The depth
    // of the recursion can be as deep as the size of ready_rows_, which might
    // be significant and potentially lead to stack overflow. The way to
    // overcome this is to always switch thread to a CompletionQueue thread.
    // Switching thread for every row has a non-trivial cost, though. To find a
    // good balance, we allow for recursion no deeper than 100 and achieve it by
    // tracking the level in `recursion_level_`.
    //
    // The magic value 100 is arbitrary, but back-of-the-envelope calculation
    // indicates it should cap this stack usage to below 100K. Default stack
    // size is usually 1MB.
    struct CountFrame {
      explicit CountFrame(int& cntr) : cntr(++cntr){};
      ~CountFrame() { --cntr; }
      int& cntr;
    };
    CountFrame frame(recursion_level_);

    if (ready_rows_.empty()) {
      if (whole_op_finished_) {
        // The scan is finished for good, there will be no more rows.
        on_finish_(status_);
        return;
      }
      // assert(continue_reading_);
      // No rows, but we can fetch some.
      auto continue_reading = std::move(continue_reading_);
      continue_reading_.reset();
      continue_reading->set_value(true);
      return;
    }

    // Yay! We have something to give to the user and they want it.
    auto row = std::move(ready_rows_.front());
    ready_rows_.pop();

    auto self = this->shared_from_this();
    bool const break_recursion = recursion_level_ >= 100;
    on_row_(std::move(row)).then([self, break_recursion](future<bool> fut) {
      bool should_cancel;
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
      try {
        should_cancel = !fut.get();
      } catch (std::exception& ex) {
        self->Cancel(
            std::string("future<> returned from the user callback threw an "
                        "exception: ") +
            ex.what());
        return;
      } catch (...) {
        self->Cancel(
            "future<> returned from the user callback threw an unknown "
            "exception");
        return;
      }
#else   // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
      should_cancel = !fut.get();
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
      if (should_cancel) {
        self->Cancel("User cancelled");
        return;
      }
      if (break_recursion) {
        self->cq_.RunAsync([self](CompletionQueue&) { self->UserWantsRows(); });
        return;
      }
      self->UserWantsRows();
    });
  }

  /// Called when lower layers provide us with a response chunk.
  future<bool> OnDataReceived(google::bigtable::v2::ReadRowsResponse response) {
    // assert(!whole_op_finished_);
    // assert(!continue_reading_);
    // assert(status_.ok());
    status_ = ConsumeResponse(std::move(response));
    // We've processed the response.
    //
    // If there were errors (e.g. malformed response from the server), we should
    // interrupt this stream. Interrupting it will yield lower layers calling
    // `OnStreamFinished` with a status unrelated to the real reason, so we
    // store the actual reason in status_ and proceed exactly the
    // same way as if the stream was broken for other reasons.
    //
    // Even if status_ is not OK, we might have consumed some rows,
    // but, don't give them to the user yet. We want to keep the invariant that
    // either the user doesn't hold a `future<>` when we're fetching more rows.
    // Retries (successful or not) will do it. Improving this behavior makes
    // little sense because parser errors are very unexpected and probably not
    // retriable anyway.

    if (status_.ok()) {
      continue_reading_.emplace(promise<bool>());
      auto res = continue_reading_->get_future();
      TryGiveRowToUser();
      return res;
    }
    return make_ready_future<bool>(false);
  }

  /// Called when the whole stream finishes.
  void OnStreamFinished(Status status) {
    // assert(!continue_reading_);
    if (status_.ok()) {
      status_ = std::move(status);
    }
    grpc::Status parser_status;
    parser_->HandleEndOfStream(parser_status);
    if (!parser_status.ok() && status_.ok()) {
      // If there stream finished with an error ignore what the parser says.
      status_ = internal::MakeStatusFromRpcError(parser_status);
    }

    // In the unlikely case when we have already reached the requested
    // number of rows and still receive an error (the parser can throw
    // an error at end of stream for example), there is no need to
    // retry and we have no good value for rows_limit anyway.
    if (rows_limit_ != NO_ROWS_LIMIT && rows_limit_ <= rows_count_) {
      status_ = Status();
    }

    if (!last_read_row_key_.empty()) {
      // We've returned some rows and need to make sure we don't
      // request them again.
      row_set_ = row_set_.Intersect(RowRange::Open(last_read_row_key_, ""));
    }

    // If we receive an error, but the retriable set is empty, consider it a
    // success.
    if (row_set_.IsEmpty()) {
      status_ = Status();
    }

    if (status_.ok()) {
      // We've successfully finished the scan.
      whole_op_finished_ = true;
      TryGiveRowToUser();
      return;
    }

    if (!rpc_retry_policy_->OnFailure(status_)) {
      // Can't retry.
      whole_op_finished_ = true;
      TryGiveRowToUser();
      return;
    }
    auto self = this->shared_from_this();
    cq_.MakeRelativeTimer(rpc_backoff_policy_->OnCompletion(status_))
        .then([self](future<std::chrono::system_clock::time_point>) {
          self->MakeRequest();
        });
  }

  /// User satisfied the future returned from the row callback with false.
  void Cancel(std::string const& reason) {
    ready_rows_ = std::queue<Row>();
    auto continue_reading = std::move(continue_reading_);
    continue_reading_.reset();
    Status status(StatusCode::kCancelled, reason);
    if (!continue_reading) {
      // If we're not in the middle of the stream fire some user callbacks, but
      // also override the overall status.
      // assert(whole_op_finished_);
      status_ = std::move(status);
      TryGiveRowToUser();
      return;
    }
    // If we are in the middle of the stream, cancel the stream.
    status_ = std::move(status);
    continue_reading->set_value(false);
  }

  /// Process everything that is accumulated in the parser.
  Status DrainParser() {
    grpc::Status status;
    while (parser_->HasNext()) {
      Row parsed_row = parser_->Next(status);
      if (!status.ok()) {
        return internal::MakeStatusFromRpcError(status);
      }
      ++rows_count_;
      last_read_row_key_ = std::string(parsed_row.row_key());
      ready_rows_.emplace(std::move(parsed_row));
    }
    return Status();
  }

  /// Parse the data from the response.
  Status ConsumeResponse(google::bigtable::v2::ReadRowsResponse response) {
    for (auto& chunk : *response.mutable_chunks()) {
      grpc::Status status;
      parser_->HandleChunk(std::move(chunk), status);
      if (!status.ok()) {
        return internal::MakeStatusFromRpcError(status);
      }
      Status parser_status = DrainParser();
      if (!parser_status.ok()) {
        return parser_status;
      }
    }
    return Status();
  }

  friend class Table;

  std::mutex mu_;
  CompletionQueue cq_;
  std::shared_ptr<DataClient> client_;
  std::string app_profile_id_;
  std::string table_name_;
  RowFunctor on_row_;
  FinishFunctor on_finish_;
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
  bool whole_op_finished_;
  /**
   * The status of the last retry attempt_.
   *
   * It is reset to OK at the beginning of every retry. If an error is
   * encountered (be it while parsing the response or on stream finish), it is
   * stored here (unless a different error had already been stored).
   */
  Status status_;
  /// Tracks the level of recursion of TryGiveRowToUser
  int recursion_level_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_ASYNC_ROW_READER_H_
