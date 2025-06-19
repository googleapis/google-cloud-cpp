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

#include "google/cloud/bigtable/internal/default_row_reader.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/retry_loop_helpers.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

DefaultRowReader::DefaultRowReader(
    std::shared_ptr<BigtableStub> stub, std::string app_profile_id,
    std::string table_name, bigtable::RowSet row_set, std::int64_t rows_limit,
    bigtable::Filter filter, bool reverse,
    std::unique_ptr<bigtable::DataRetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy, bool enable_server_retries,
    Sleeper sleeper)
    : stub_(std::move(stub)),
      app_profile_id_(std::move(app_profile_id)),
      table_name_(std::move(table_name)),
      row_set_(std::move(row_set)),
      rows_limit_(rows_limit),
      filter_(std::move(filter)),
      reverse_(reverse),
      retry_policy_(std::move(retry_policy)),
      backoff_policy_(std::move(backoff_policy)),
      enable_server_retries_(enable_server_retries),
      sleeper_(std::move(sleeper)) {}

void DefaultRowReader::MakeRequest() {
  response_ = {};
  processed_chunks_count_ = 0;

  google::bigtable::v2::ReadRowsRequest request;
  request.set_table_name(table_name_);
  request.set_app_profile_id(app_profile_id_);
  request.set_reversed(reverse_);

  auto row_set_proto = row_set_.as_proto();
  request.mutable_rows()->Swap(&row_set_proto);

  auto filter_proto = filter_.as_proto();
  request.mutable_filter()->Swap(&filter_proto);

  if (rows_limit_ != bigtable::RowReader::NO_ROWS_LIMIT) {
    request.set_rows_limit(rows_limit_ - rows_count_);
  }

  auto const& options = internal::CurrentOptions();
  context_ = std::make_shared<grpc::ClientContext>();
  internal::ConfigureContext(*context_, options);
  retry_context_.PreCall(*context_);
  stream_ = stub_->ReadRows(context_, options, request);
  stream_is_open_ = true;

  parser_ = bigtable::internal::ReadRowsParserFactory().Create(reverse_);
}

bool DefaultRowReader::NextChunk() {
  ++processed_chunks_count_;
  while (processed_chunks_count_ >= response_.chunks_size()) {
    processed_chunks_count_ = 0;
    auto v = stream_->Read();
    if (absl::holds_alternative<Status>(v)) {
      last_status_ = absl::get<Status>(std::move(v));
      response_ = {};
      retry_context_.PostCall(*context_);
      context_.reset();
      return false;
    }
    response_ = absl::get<google::bigtable::v2::ReadRowsResponse>(std::move(v));
    if (!response_.last_scanned_row_key().empty()) {
      last_read_row_key_ = std::move(*response_.mutable_last_scanned_row_key());
    }
  }
  return true;
}

absl::variant<Status, bigtable::Row> DefaultRowReader::Advance() {
  if (operation_cancelled_) {
    return internal::CancelledError(
        "call cancelled",
        GCP_ERROR_INFO().WithMetadata("gl-cpp.error.origin", "client"));
  }
  while (true) {
    auto variant = AdvanceOrFail();
    if (absl::holds_alternative<bigtable::Row>(variant)) {
      if (first_response_) {
        first_response_ = false;
        retry_context_.FirstResponse(*context_);
      }
      return absl::get<bigtable::Row>(std::move(variant));
    }

    auto status = absl::get<Status>(std::move(variant));
    if (status.ok()) return Status{};

    // In the unlikely case when we have already reached the requested
    // number of rows and still receive an error (the parser can throw
    // an error at end of stream for example), there is no need to
    // retry and we have no good value for rows_limit anyway.
    if (rows_limit_ != bigtable::RowReader::NO_ROWS_LIMIT &&
        rows_limit_ <= rows_count_) {
      return Status{};
    }

    if (!last_read_row_key_.empty()) {
      // We've returned some rows and need to make sure we don't
      // request them again.
      if (reverse_) {
        google::bigtable::v2::RowRange range;
        range.set_end_key_open(last_read_row_key_);
        row_set_ = row_set_.Intersect(bigtable::RowRange(std::move(range)));
      } else {
        row_set_ = row_set_.Intersect(
            bigtable::RowRange::Open(last_read_row_key_, ""));
      }
    }

    // If we receive an error, but the retryable set is empty, stop.
    if (row_set_.IsEmpty()) return Status{};

    auto delay = internal::Backoff(status, "AsyncReadRows", *retry_policy_,
                                   *backoff_policy_, Idempotency::kIdempotent,
                                   enable_server_retries_);
    if (!delay) return std::move(delay).status();
    sleeper_(*delay);

    // If we reach this place, we failed and need to restart the call.
    MakeRequest();
  }
}

absl::variant<Status, bigtable::Row> DefaultRowReader::AdvanceOrFail() {
  grpc::Status status;
  if (!stream_) {
    MakeRequest();
  }
  while (!parser_->HasNext()) {
    if (NextChunk()) {
      parser_->HandleChunk(
          std::move(*(response_.mutable_chunks(processed_chunks_count_))),
          status);
      if (!status.ok()) return MakeStatusFromRpcError(status);
      continue;
    }

    // Here, there are no more chunks to look at. Close the stream,
    // finalize the parser and return OK with no rows unless something
    // fails during cleanup.
    stream_is_open_ = false;
    if (!last_status_.ok()) return last_status_;
    parser_->HandleEndOfStream(status);
    return MakeStatusFromRpcError(status);
  }

  // We have a complete row in the parser.
  bigtable::Row parsed_row = parser_->Next(status);

  if (!status.ok()) return MakeStatusFromRpcError(status);

  ++rows_count_;
  last_read_row_key_ = parsed_row.row_key();
  return parsed_row;
}

void DefaultRowReader::Cancel() {
  operation_cancelled_ = true;
  if (!stream_is_open_) return;
  stream_->Cancel();

  // Also drain any data left unread
  while (absl::holds_alternative<google::bigtable::v2::ReadRowsResponse>(
      stream_->Read())) {
  }

  context_.reset();
  stream_is_open_ = false;
}

DefaultRowReader::~DefaultRowReader() {
  // Make sure we don't leave open streams.
  Cancel();
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
