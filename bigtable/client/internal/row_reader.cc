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

#include "bigtable/client/internal/row_reader.h"

#include <thread>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {}  // namespace BIGTABLE_CLIENT_NS

RowReader::RowReader(std::shared_ptr<DataClient> client,
                     absl::string_view table_name, RowSet row_set,
                     int rows_limit, Filter filter,
                     std::unique_ptr<RPCRetryPolicy> retry_policy,
                     std::unique_ptr<RPCBackoffPolicy> backoff_policy)
    : client_(std::move(client)),
      table_name_(table_name),
      row_set_(std::move(row_set)),
      rows_limit_(rows_limit),
      filter_(std::move(filter)),
      context_(absl::make_unique<grpc::ClientContext>()),
      parser_(absl::make_unique<ReadRowsParser>()),
      response_(),
      processed_chunks_(0),
      rows_count_(0),
      row_() {
  MakeRequest();
  Advance();
}

RowReader::iterator RowReader::begin() {
  if (not row_) {
    return end();
  }
  return RowReader::RowReaderIterator(this, false);
}

RowReader::iterator RowReader::end() {
  return RowReader::RowReaderIterator(this, true);
}

RowReader::RowReaderIterator& RowReader::RowReaderIterator::operator++() {
  owner_->Advance();
  if (not owner_->row_) {
    is_end_ = true;
  }
  return *this;
}

void RowReader::MakeRequest() {
  response_ = {};
  processed_chunks_ = 0;

  google::bigtable::v2::ReadRowsRequest request;
  request.set_table_name(std::string(table_name_));

  if (row_) {
    // There is a previous read row, so this is a restarted call and
    // we need to clip the RowSet at the last seen row key.
    row_set_.ClipUpTo(row_->row_key());
  }

  auto row_set_proto = row_set_.as_proto();
  request.mutable_rows()->Swap(&row_set_proto);

  auto filter_proto = filter_.as_proto();
  request.mutable_filter()->Swap(&filter_proto);

  if (rows_limit_ != NO_ROWS_LIMIT) {
    request.set_rows_limit(rows_limit_ - rows_count_);
  }

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

void RowReader::Advance() {
  while (true) {
    grpc::Status status = grpc::Status::OK;

    try {
      status = AdvanceOrFail();
    } catch (std::exception ex) {
      // Parser exceptions arrive here
      status = grpc::Status(grpc::INTERNAL, ex.what());
    }

    if (status.ok()) {
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

grpc::Status RowReader::AdvanceOrFail() {
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
    row_.emplace(parser_->Next());
    ++rows_count_;
  } else {
    row_.reset();
  }

  return grpc::Status::OK;
}

}  // namespace bigtable
