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

#include "bigtable/client/row_reader.h"
#include "bigtable/client/internal/make_unique.h"
#include "bigtable/client/internal/throw_delegate.h"
#include <thread>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// RowReader::iterator must satisfy the requirements of an InputIterator.
static_assert(std::is_base_of<std::iterator<std::input_iterator_tag, Row>,
                              RowReader::iterator>::value,
              "RowReader::iterator must be an InputIterator");
static_assert(std::is_copy_constructible<RowReader::iterator>::value,
              "RowReader::iterator must be CopyConstructible");
static_assert(std::is_move_constructible<RowReader::iterator>::value,
              "RowReader::iterator must be MoveConstructible");
static_assert(std::is_copy_assignable<RowReader::iterator>::value,
              "RowReader::iterator must be CopyAssignable");
static_assert(std::is_move_assignable<RowReader::iterator>::value,
              "RowReader::iterator must be MoveAssignable");
static_assert(std::is_destructible<RowReader::iterator>::value,
              "RowReader::iterator must be Destructible");
static_assert(
    std::is_convertible<decltype(*std::declval<RowReader::iterator>()),
                        RowReader::iterator::value_type>::value,
    "*it when it is of RowReader::iterator type must be convertible to "
    "RowReader::iterator::value_type>");
static_assert(std::is_same<decltype(++std::declval<RowReader::iterator>()),
                           RowReader::iterator&>::value,
              "++it when it is of RowReader::iterator type must be a "
              "RowReader::iterator &>");

RowReader::RowReader(
    std::shared_ptr<DataClient> client, std::string table_name, RowSet row_set,
    std::int64_t rows_limit, Filter filter,
    std::unique_ptr<RPCRetryPolicy> retry_policy,
    std::unique_ptr<RPCBackoffPolicy> backoff_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::unique_ptr<internal::ReadRowsParserFactory> parser_factory)
    : RowReader(std::move(client), std::move(table_name), std::move(row_set),
                rows_limit, std::move(filter), std::move(retry_policy),
                std::move(backoff_policy), std::move(metadata_update_policy),
                std::move(parser_factory), true) {}

RowReader::RowReader(
    std::shared_ptr<DataClient> client, std::string table_name, RowSet row_set,
    std::int64_t rows_limit, Filter filter,
    std::unique_ptr<RPCRetryPolicy> retry_policy,
    std::unique_ptr<RPCBackoffPolicy> backoff_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::unique_ptr<internal::ReadRowsParserFactory> parser_factory,
    bool raise_on_error)
    : client_(std::move(client)),
      table_name_(std::move(table_name)),
      row_set_(std::move(row_set)),
      rows_limit_(rows_limit),
      filter_(std::move(filter)),
      retry_policy_(std::move(retry_policy)),
      backoff_policy_(std::move(backoff_policy)),
      metadata_update_policy_(std::move(metadata_update_policy)),
      context_(),
      parser_factory_(std::move(parser_factory)),
      stream_is_open_(false),
      operation_cancelled_(false),
      processed_chunks_count_(0),
      rows_count_(0),
      status_(grpc::Status::OK),
      raise_on_error_(raise_on_error),
      error_retrieved_(raise_on_error) {}

RowReader::iterator RowReader::begin() {
  if (operation_cancelled_) {
    if (raise_on_error_) {
      internal::RaiseRuntimeError("Operation already cancelled.");
    } else {
      status_ = grpc::Status::CANCELLED;
      return internal::RowReaderIterator(this, true);
    }
  }
  if (not stream_) {
    MakeRequest();
  }
  // Increment the iterator to read a row.
  return ++internal::RowReaderIterator(this, false);
}

RowReader::iterator RowReader::end() {
  return internal::RowReaderIterator(this, true);
}

void RowReader::MakeRequest() {
  response_ = {};
  processed_chunks_count_ = 0;

  google::bigtable::v2::ReadRowsRequest request;
  request.set_table_name(std::string(table_name_));

  auto row_set_proto = row_set_.as_proto();
  request.mutable_rows()->Swap(&row_set_proto);

  auto filter_proto = filter_.as_proto();
  request.mutable_filter()->Swap(&filter_proto);

  if (rows_limit_ != NO_ROWS_LIMIT) {
    request.set_rows_limit(rows_limit_ - rows_count_);
  }

  context_ = bigtable::internal::make_unique<grpc::ClientContext>();
  retry_policy_->setup(*context_);
  backoff_policy_->setup(*context_);
  metadata_update_policy_.setup(*context_);
  stream_ = client_->Stub()->ReadRows(context_.get(), request);
  stream_is_open_ = true;

  parser_ = parser_factory_->Create();
}

bool RowReader::NextChunk() {
  ++processed_chunks_count_;
  while (processed_chunks_count_ >= response_.chunks_size()) {
    processed_chunks_count_ = 0;
    bool response_is_valid = stream_->Read(&response_);
    if (not response_is_valid) {
      response_ = {};
      return false;
    }
  }
  return true;
}

void RowReader::Advance(internal::OptionalRow& row) {
  while (true) {
    grpc::Status status;
    status_ = status = AdvanceOrFail(row);
    if (status.ok()) {
      return;
    }

    // In the unlikely case when we have already reached the requested
    // number of rows and still receive an error (the parser can throw
    // an error at end of stream for example), there is no need to
    // retry and we have no good value for rows_limit anyway.
    if (rows_limit_ != NO_ROWS_LIMIT and rows_limit_ <= rows_count_) {
      return;
    }

    if (not last_read_row_key_.empty()) {
      // We've returned some rows and need to make sure we don't
      // request them again.
      row_set_ = row_set_.Intersect(RowRange::Open(last_read_row_key_, ""));
    }

    // If we receive an error, but the retriable set is empty, stop.
    if (row_set_.IsEmpty()) {
      return;
    }

    if (not status.ok() and not retry_policy_->on_failure(status)) {
      if (raise_on_error_) {
        internal::RaiseRuntimeError("Unretriable error: " +
                                    status.error_message());
        /*NOTREACHED*/  // because internal::RaiseRuntimeError is [[noreturn]]
      }
      return;
    }

    auto delay = backoff_policy_->on_completion(status);
    std::this_thread::sleep_for(delay);

    // If we reach this place, we failed and need to restart the call.
    MakeRequest();
  }
}

grpc::Status RowReader::AdvanceOrFail(internal::OptionalRow& row) {
  grpc::Status status;
  row.reset();
  while (not parser_->HasNext()) {
    if (NextChunk()) {
      parser_->HandleChunk(
          std::move(*(response_.mutable_chunks(processed_chunks_count_))),
          status);
      if (not status.ok()) {
        return status;
      }
      continue;
    }

    // Here, there are no more chunks to look at. Close the stream,
    // finalize the parser and return OK with no rows unless something
    // fails during cleanup.
    stream_is_open_ = false;
    status = stream_->Finish();
    if (not status.ok()) {
      return status;
    }
    parser_->HandleEndOfStream(status);
    return status;
  }

  // We have a complete row in the parser.
  Row parsed_row = parser_->Next(status);
  if (not status.ok()) {
    return status;
  }
  row.emplace(std::move(parsed_row));
  ++rows_count_;
  last_read_row_key_ = std::string(row.value().row_key());

  return status;
}

void RowReader::Cancel() {
  operation_cancelled_ = true;
  if (not stream_is_open_) {
    return;
  }
  context_->TryCancel();

  // Also drain any data left unread
  google::bigtable::v2::ReadRowsResponse response;
  while (stream_->Read(&response)) {
  }

  stream_is_open_ = false;
  (void)stream_->Finish();  // ignore errors
}

RowReader::~RowReader() {
  // Make sure we don't leave open streams.
  Cancel();
  if (not raise_on_error_ and not error_retrieved_ and not status_.ok()) {
    internal::RaiseRuntimeError(
        "Exception is disabled and error is not retrieved");
  }
}
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
