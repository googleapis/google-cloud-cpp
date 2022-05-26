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
#include "absl/memory/memory.h"
#include <thread>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

DefaultRowReader::DefaultRowReader(
    std::shared_ptr<BigtableStub> stub, std::string app_profile_id,
    std::string table_name, bigtable::RowSet row_set, std::int64_t rows_limit,
    bigtable::Filter filter, std::unique_ptr<DataRetryPolicy> retry_policy,
    std::unique_ptr<BackoffPolicy> backoff_policy)
    : stub_(std::move(stub)),
      app_profile_id_(std::move(app_profile_id)),
      table_name_(std::move(table_name)),
      row_set_(std::move(row_set)),
      rows_limit_(rows_limit),
      filter_(std::move(filter)),
      retry_policy_(std::move(retry_policy)),
      backoff_policy_(std::move(backoff_policy)) {}

void DefaultRowReader::MakeRequest() {
  response_ = {};
  processed_chunks_count_ = 0;

  google::bigtable::v2::ReadRowsRequest request;
  request.set_table_name(table_name_);
  request.set_app_profile_id(app_profile_id_);

  auto row_set_proto = row_set_.as_proto();
  request.mutable_rows()->Swap(&row_set_proto);

  auto filter_proto = filter_.as_proto();
  request.mutable_filter()->Swap(&filter_proto);

  if (rows_limit_ != bigtable::RowReader::NO_ROWS_LIMIT) {
    request.set_rows_limit(rows_limit_ - rows_count_);
  }

  auto const& options = internal::CurrentOptions();
  auto context = absl::make_unique<grpc::ClientContext>();
  internal::ConfigureContext(*context, options);
  stream_ = stub_->ReadRows(std::move(context), request);
  stream_is_open_ = true;

  parser_ = bigtable::internal::ReadRowsParserFactory().Create();
}

bool DefaultRowReader::NextChunk() {
  ++processed_chunks_count_;
  while (processed_chunks_count_ >= response_.chunks_size()) {
    processed_chunks_count_ = 0;
    auto v = stream_->Read();
    if (absl::holds_alternative<Status>(v)) {
      last_status_ = absl::get<Status>(std::move(v));
      response_ = {};
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
    return Status(StatusCode::kCancelled, "Operation cancelled.");
  }
  while (true) {
    auto variant = AdvanceOrFail();
    if (absl::holds_alternative<bigtable::Row>(variant)) {
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
      row_set_ =
          row_set_.Intersect(bigtable::RowRange::Open(last_read_row_key_, ""));
    }

    // If we receive an error, but the retryable set is empty, stop.
    if (row_set_.IsEmpty()) return Status{};

    if (!retry_policy_->OnFailure(status)) return status;

    auto delay = backoff_policy_->OnCompletion();
    std::this_thread::sleep_for(delay);

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
