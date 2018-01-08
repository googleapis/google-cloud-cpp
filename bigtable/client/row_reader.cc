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

#include <thread>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {}  // namespace BIGTABLE_CLIENT_NS

// RowReader::iterator must satisfy the requirements of an InputIterator.
namespace {
using base_iterator = std::iterator<std::input_iterator_tag, Row>;

static_assert(std::is_base_of<base_iterator, RowReader::iterator>::value,
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

// TODO(C++17): std::is_swappable

// TODO: check that if o is of type iterator then ++o and *o
// are valid expressions with the usual return types ...

}  // anonymous namespace

RowReader::RowReader(std::shared_ptr<DataClient> client,
                     absl::string_view table_name, RowSet row_set,
                     std::int64_t rows_limit, Filter filter,
                     std::unique_ptr<RPCRetryPolicy> retry_policy,
                     std::unique_ptr<RPCBackoffPolicy> backoff_policy)
    : client_(std::move(client)),
      table_name_(table_name),
      row_set_(std::move(row_set)),
      rows_limit_(rows_limit),
      filter_(std::move(filter)),
      retry_policy_(std::move(retry_policy)),
      backoff_policy_(std::move(backoff_policy)),
      context_(absl::make_unique<grpc::ClientContext>()),
      parser_(absl::make_unique<ReadRowsParser>()),
      response_(),
      processed_chunks_(0),
      rows_count_(0),
      last_read_row_key_() {}

RowReader::iterator RowReader::begin() {
  if (not stream_) {
    MakeRequest();
  }
  // Increment the iterator to read a row.
  return ++RowReader::RowReaderIterator(this, false);
}

RowReader::iterator RowReader::end() {
  return RowReader::RowReaderIterator(this, true);
}

RowReader::RowReaderIterator& RowReader::RowReaderIterator::operator++() {
  owner_->Advance(row_);
  return *this;
}

void RowReader::MakeRequest() {
  response_ = {};
  processed_chunks_ = 0;

  google::bigtable::v2::ReadRowsRequest request;
  request.set_table_name(std::string(table_name_));

  if (not last_read_row_key_.empty()) {
    // There is a previous read row, so this is a restarted call and
    // we need to clip the RowSet at the last read row key.
    row_set_.ClipUpTo(last_read_row_key_);
  }

  auto row_set_proto = row_set_.as_proto();
  request.mutable_rows()->Swap(&row_set_proto);

  auto filter_proto = filter_.as_proto();
  request.mutable_filter()->Swap(&filter_proto);

  if (rows_limit_ != NO_ROWS_LIMIT) {
    request.set_rows_limit(rows_limit_ - rows_count_);
  }

  retry_policy_->setup(*context_.get());
  backoff_policy_->setup(*context_.get());
  stream_ = client_->Stub().ReadRows(context_.get(), request);
}

bool RowReader::NextChunk() {
  ++processed_chunks_;
  while (processed_chunks_ >= response_.chunks_size()) {
    processed_chunks_ = 0;
    bool response_is_valid = stream_->Read(&response_);
    if (not response_is_valid) {
      response_ = {};
      return false;
    }
  }
  return true;
}

void RowReader::Advance(absl::optional<Row>& row) {
  while (true) {
    grpc::Status status = grpc::Status::OK;

    try {
      status = AdvanceOrFail(row);
    } catch (std::exception ex) {
      // Parser exceptions arrive here
      status = grpc::Status(grpc::INTERNAL, ex.what());
    }

    if (status.ok()) {
      return;
    }

    // In the unlikely case when we have already reached the requested
    // number of rows and still receive an error (the parser can throw
    // an error at end of stream for example), there is no need to
    // retry and we have no good value for rows_limit anyway.
    if (rows_limit_ != NO_ROWS_LIMIT and rows_limit_ <= rows_count_) {
      row.reset();
      return;
    }

    if (not status.ok() and not retry_policy_->on_failure(status)) {
      throw std::runtime_error("Unretriable error: " + status.error_message());
    }

    auto delay = backoff_policy_->on_completion(status);
    std::this_thread::sleep_for(delay);

    // If we reach this place, we failed and need to restart the call
    MakeRequest();
  }
}

grpc::Status RowReader::AdvanceOrFail(absl::optional<Row>& row) {
  do {
    if (NextChunk()) {
      parser_->HandleChunk(
          std::move(*(response_.mutable_chunks(processed_chunks_))));
    } else {
      grpc::Status status = stream_->Finish();
      if (status.ok()) {
        parser_->HandleEndOfStream();
        break;
      } else {
        return status;
      }
    }
  } while (not parser_->HasNext());

  if (parser_->HasNext()) {
    row.emplace(parser_->Next());
    ++rows_count_;
    last_read_row_key_ = std::string(row->row_key());
  } else {
    row.reset();
  }

  return grpc::Status::OK;
}

}  // namespace bigtable
