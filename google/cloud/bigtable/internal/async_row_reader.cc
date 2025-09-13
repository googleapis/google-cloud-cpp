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

#include "google/cloud/bigtable/internal/async_row_reader.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/grpc_opentelemetry.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/retry_loop_helpers.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace v2 = ::google::bigtable::v2;

void AsyncRowReader::MakeRequest() {
  status_ = Status();
  v2::ReadRowsRequest request;

  request.set_app_profile_id(app_profile_id_);
  request.set_table_name(table_name_);
  request.set_reversed(reverse_);
  auto row_set_proto = row_set_.as_proto();
  request.mutable_rows()->Swap(&row_set_proto);

  auto filter_proto = filter_.as_proto();
  request.mutable_filter()->Swap(&filter_proto);

  if (rows_limit_ != NO_ROWS_LIMIT) {
    request.set_rows_limit(rows_limit_ - rows_count_);
  }
  parser_ = bigtable::internal::ReadRowsParserFactory().Create(reverse_);

  internal::ScopedCallContext scope(call_context_);
  client_context_ = std::make_shared<grpc::ClientContext>();
  internal::ConfigureContext(*client_context_, *call_context_.options);
  operation_context_->PreCall(*client_context_);

  auto self = this->shared_from_this();
  PerformAsyncStreamingRead(
      stub_->AsyncReadRows(cq_, client_context_, options_, request),
      [self](v2::ReadRowsResponse r) {
        return self->OnDataReceived(std::move(r));
      },
      [self](Status s) { self->OnStreamFinished(std::move(s)); });
}

void AsyncRowReader::UserWantsRows() {
  operation_context_->ElementRequest(*client_context_);
  TryGiveRowToUser();
}

void AsyncRowReader::TryGiveRowToUser() {
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
  struct CountFrames {
    explicit CountFrames(int& cntr) : cntr(++cntr) {}
    ~CountFrames() { --cntr; }
    int& cntr;
  } counter(recursion_level_);

  if (ready_rows_.empty()) {
    if (whole_op_finished_) {
      // The scan is finished for good, there will be no more rows.
      on_finish_(status_);
      return;
    }
    if (!continue_reading_) {
      GCP_LOG(FATAL)
          << "No rows are ready and we can't continue reading. This is a bug, "
             "please report it at "
             "https://github.com/googleapis/google-cloud-cpp/issues/new";
    }
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
  operation_context_->ElementDelivery(*client_context_);
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
      self->cq_.RunAsync([self] { self->UserWantsRows(); });
      return;
    }
    self->UserWantsRows();
  });
}

future<bool> AsyncRowReader::OnDataReceived(
    google::bigtable::v2::ReadRowsResponse response) {
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
  // retryable anyway.

  if (status_.ok()) {
    continue_reading_.emplace(promise<bool>());
    auto res = continue_reading_->get_future();
    TryGiveRowToUser();
    return res;
  }
  return make_ready_future<bool>(false);
}

void AsyncRowReader::OnStreamFinished(Status status) {
  // assert(!continue_reading_);
  if (status_.ok()) {
    status_ = std::move(status);
  }
  grpc::Status parser_status;
  parser_->HandleEndOfStream(parser_status);
  if (!parser_status.ok() && status_.ok()) {
    // If the stream finished with an error ignore what the parser says.
    status_ = MakeStatusFromRpcError(parser_status);
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
    if (reverse_) {
      google::bigtable::v2::RowRange range;
      range.set_end_key_open(last_read_row_key_);
      row_set_ = row_set_.Intersect(bigtable::RowRange(std::move(range)));
    } else {
      row_set_ =
          row_set_.Intersect(bigtable::RowRange::Open(last_read_row_key_, ""));
    }
  }

  // If we receive an error, but the retryable set is empty, consider it a
  // success.
  if (row_set_.IsEmpty()) {
    status_ = Status();
  }

  // grpc::ClientContext::GetServerInitialMetadata check fails if the metadata
  // has not been read. There is no way to check if the metadata is available
  // before calling GetServerInitialMetadata, and we do not want to add a call
  // to ClientReaderInterface::WaitForInitialMetadata, which would introduce
  // latency, just for the sake of telemetry. Therefore, we will only call
  // OperationContext::PostCall if we can guarantee that we've received some
  // data which will include the metadata.
  if (rows_count_ > 0) {
    operation_context_->PostCall(*client_context_, status_);
  }

  if (status_.ok()) {
    // We've successfully finished the scan.
    whole_op_finished_ = true;
    operation_context_->OnDone(status_);
    TryGiveRowToUser();
    return;
  }

  auto delay = internal::Backoff(status_, "AsyncReadRows", *retry_policy_,
                                 *backoff_policy_, Idempotency::kIdempotent,
                                 enable_server_retries_);
  if (!delay) {
    // Can't retry.
    status_ = std::move(delay).status();
    whole_op_finished_ = true;
    operation_context_->OnDone(status_);
    TryGiveRowToUser();
    return;
  }
  client_context_.reset();
  auto self = this->shared_from_this();
  internal::TracedAsyncBackoff(cq_, *call_context_.options, *delay,
                               "Async Backoff")
      .then([self](auto result) {
        if (auto tp = result.get()) {
          self->MakeRequest();
        } else {
          self->whole_op_finished_ = true;
          self->TryGiveRowToUser();
        }
      });
}

void AsyncRowReader::Cancel(std::string const& reason) {
  ready_rows_ = std::queue<bigtable::Row>();
  auto continue_reading = std::move(continue_reading_);
  continue_reading_.reset();
  Status status = internal::CancelledError(
      reason, GCP_ERROR_INFO().WithMetadata("gl-cpp.error.origin", "client"));
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

Status AsyncRowReader::DrainParser() {
  grpc::Status status;
  while (parser_->HasNext()) {
    bigtable::Row parsed_row = parser_->Next(status);
    if (!status.ok()) {
      return MakeStatusFromRpcError(status);
    }
    ++rows_count_;
    last_read_row_key_ = parsed_row.row_key();
    ready_rows_.emplace(std::move(parsed_row));
  }
  return Status();
}

Status AsyncRowReader::ConsumeResponse(
    google::bigtable::v2::ReadRowsResponse response) {
  for (auto& chunk : *response.mutable_chunks()) {
    grpc::Status status;
    parser_->HandleChunk(std::move(chunk), status);
    if (!status.ok()) {
      return MakeStatusFromRpcError(status);
    }
    Status parser_status = DrainParser();
    if (!parser_status.ok()) {
      return parser_status;
    }
  }
  if (!response.last_scanned_row_key().empty()) {
    last_read_row_key_ = std::move(*response.mutable_last_scanned_row_key());
  }
  return Status();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
