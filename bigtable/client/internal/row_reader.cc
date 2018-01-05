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

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {}  // namespace BIGTABLE_CLIENT_NS

RowReader::RowReader(std::shared_ptr<DataClient> client,
                     absl::string_view table_name)
    : client_(std::move(client)),
      table_name_(table_name),
      context_(absl::make_unique<grpc::ClientContext>()),
      parser_(absl::make_unique<ReadRowsParser>()),
      response_(),
      processed_chunks_(0),
      rows_count_(0),
      row_() {
  google::bigtable::v2::ReadRowsRequest request;
  request.set_table_name(std::string(table_name_));
  stream_ = client_->Stub().ReadRows(context_.get(), request);
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
  do {
    if (NextChunk()) {
      parser_->HandleChunk(
          std::move(*(response_.mutable_chunks(processed_chunks_))));
    } else {
      parser_->HandleEndOfStream();
      break;
    }
  } while (not parser_->HasNext());

  if (parser_->HasNext()) {
    row_.emplace(parser_->Next());
    ++rows_count_;
  } else {
    row_.reset();
  }
}

}  // namespace bigtable
