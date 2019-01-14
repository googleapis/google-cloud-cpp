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

#include "google/cloud/bigtable/row_reader.h"
#include "google/cloud/bigtable/internal/table.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/internal/throw_delegate.h"
#include "google/cloud/log.h"
#include <thread>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
// RowReader::iterator must satisfy the requirements of an InputIterator.
static_assert(
    std::is_same<std::iterator_traits<RowReader::iterator>::iterator_category,
                 std::input_iterator_tag>::value,
    "RowReader::iterator should be an InputIterator");
static_assert(
    std::is_same<std::iterator_traits<RowReader::iterator>::value_type,
                 Row>::value,
    "RowReader::iterator should be an InputIterator of Row");
static_assert(std::is_same<std::iterator_traits<RowReader::iterator>::pointer,
                           Row*>::value,
              "RowReader::iterator should be an InputIterator of Row");
static_assert(std::is_same<std::iterator_traits<RowReader::iterator>::reference,
                           Row&>::value,
              "RowReader::iterator should be an InputIterator of Row");
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
    std::shared_ptr<DataClient> client, bigtable::TableId table_name,
    RowSet row_set, std::int64_t rows_limit, Filter filter,
    std::unique_ptr<RPCRetryPolicy> retry_policy,
    std::unique_ptr<RPCBackoffPolicy> backoff_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::unique_ptr<internal::ReadRowsParserFactory> parser_factory)
    : RowReader(std::move(client), std::move(table_name), std::move(row_set),
                rows_limit, std::move(filter), std::move(retry_policy),
                std::move(backoff_policy), std::move(metadata_update_policy),
                std::move(parser_factory), true) {}

RowReader::RowReader(
    std::shared_ptr<DataClient> client, bigtable::TableId table_name,
    RowSet row_set, std::int64_t rows_limit, Filter filter,
    std::unique_ptr<RPCRetryPolicy> retry_policy,
    std::unique_ptr<RPCBackoffPolicy> backoff_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::unique_ptr<internal::ReadRowsParserFactory> parser_factory,
    bool raise_on_error)
    : RowReader(std::move(client), bigtable::AppProfileId(""),
                std::move(table_name), std::move(row_set), rows_limit,
                std::move(filter), std::move(retry_policy),
                std::move(backoff_policy), std::move(metadata_update_policy),
                std::move(parser_factory), raise_on_error) {}

RowReader::RowReader(
    std::shared_ptr<DataClient> client, bigtable::AppProfileId app_profile_id,
    bigtable::TableId table_name, RowSet row_set, std::int64_t rows_limit,
    Filter filter, std::unique_ptr<RPCRetryPolicy> retry_policy,
    std::unique_ptr<RPCBackoffPolicy> backoff_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::unique_ptr<internal::ReadRowsParserFactory> parser_factory)
    : RowReader(std::move(client), std::move(app_profile_id),
                std::move(table_name), std::move(row_set), rows_limit,
                std::move(filter), std::move(retry_policy),
                std::move(backoff_policy), std::move(metadata_update_policy),
                std::move(parser_factory), true) {}

RowReader::RowReader(
    std::shared_ptr<DataClient> client, bigtable::AppProfileId app_profile_id,
    bigtable::TableId table_name, RowSet row_set, std::int64_t rows_limit,
    Filter filter, std::unique_ptr<RPCRetryPolicy> retry_policy,
    std::unique_ptr<RPCBackoffPolicy> backoff_policy,
    MetadataUpdatePolicy metadata_update_policy,
    std::unique_ptr<internal::ReadRowsParserFactory> parser_factory,
    bool raise_on_error)
    : client_(std::move(client)),
      app_profile_id_(std::move(app_profile_id)),
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

// The name must be all lowercase to work with range-for loops.
// NOLINTNEXTLINE(readability-identifier-naming)
RowReader::iterator RowReader::begin() {
  if (operation_cancelled_) {
    if (raise_on_error_) {
      google::cloud::internal::ThrowRuntimeError(
          "Operation already cancelled.");
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

// The name must be all lowercase to work with range-for loops.
// NOLINTNEXTLINE(readability-identifier-naming)
RowReader::iterator RowReader::end() {
  return internal::RowReaderIterator(this, true);
}

void RowReader::MakeRequest() {
  response_ = {};
  processed_chunks_count_ = 0;

  google::bigtable::v2::ReadRowsRequest request;

  bigtable::internal::SetCommonTableOperationRequest<
      google::bigtable::v2::ReadRowsRequest>(request, app_profile_id_.get(),
                                             table_name_.get());
  auto row_set_proto = row_set_.as_proto();
  request.mutable_rows()->Swap(&row_set_proto);

  auto filter_proto = filter_.as_proto();
  request.mutable_filter()->Swap(&filter_proto);

  if (rows_limit_ != NO_ROWS_LIMIT) {
    request.set_rows_limit(rows_limit_ - rows_count_);
  }

  context_ = google::cloud::internal::make_unique<grpc::ClientContext>();
  retry_policy_->Setup(*context_);
  backoff_policy_->Setup(*context_);
  metadata_update_policy_.Setup(*context_);
  stream_ = client_->ReadRows(context_.get(), request);
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

    if (not status.ok() and not retry_policy_->OnFailure(status)) {
      if (raise_on_error_) {
        google::cloud::internal::ThrowRuntimeError("Unretriable error: " +
                                                   status.error_message());
        /*NOTREACHED*/
      }
      return;
    }

    auto delay = backoff_policy_->OnCompletion(status);
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
    GCP_LOG(ERROR)
        << "Exceptions are disabled, RowReader has an error,"
        << " and the error status was not retrieved by the application: "
        << "status_code=" << status_.error_code()
        << ", error_message=" << status_.error_message();
  }
}
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google
