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

#include "bigtable/client/internal/readrowsparser.h"
#include "bigtable/client/internal/throw_delegate.h"

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
using google::bigtable::v2::ReadRowsResponse_CellChunk;

void ReadRowsParser::HandleChunk(ReadRowsResponse_CellChunk chunk,
                                 grpc::Status& status) {
  if (end_of_stream_) {
    status = grpc::Status(grpc::StatusCode::INTERNAL,
                          "HandleChunk after end of stream");
    return;
  }
  if (HasNext()) {
    status = grpc::Status(grpc::StatusCode::INTERNAL,
                          "HandleChunk called before taking the previous row");
    return;
  }

  if (not chunk.row_key().empty()) {
    if (last_seen_row_key_.compare(chunk.row_key()) >= 0) {
      status = grpc::Status(grpc::StatusCode::INTERNAL,
                            "Row keys are expected in increasing order");
      return;
    }
    chunk.mutable_row_key()->swap(cell_.row);
  }

  if (chunk.has_family_name()) {
    if (not chunk.has_qualifier()) {
      status = grpc::Status(grpc::StatusCode::INTERNAL,
                            "New column family must specify qualifier");
      return;
    }
    chunk.mutable_family_name()->mutable_value()->swap(cell_.family);
  }

  if (chunk.has_qualifier()) {
    chunk.mutable_qualifier()->mutable_value()->swap(cell_.column);
  }

  if (cell_first_chunk_) {
    cell_.timestamp = chunk.timestamp_micros();
  }

  std::move(chunk.mutable_labels()->begin(), chunk.mutable_labels()->end(),
            std::back_inserter(cell_.labels));

  if (cell_first_chunk_) {
    // Most common case, move the value
    chunk.mutable_value()->swap(cell_.value);
  } else {
    cell_.value.append(chunk.value());
  }

  cell_first_chunk_ = false;

  // This is a hint we get about the total size
  if (chunk.value_size() > 0) {
    cell_.value.reserve(chunk.value_size());
  }

  // Last chunk in the cell has zero for value size
  if (chunk.value_size() == 0) {
    if (cells_.empty()) {
      if (cell_.row.empty()) {
        status = grpc::Status(grpc::StatusCode::INTERNAL,
                              "Missing row key at last chunk in cell");
        return;
      }
      row_key_ = cell_.row;
    } else {
      if (row_key_ != cell_.row) {
        status = grpc::Status(grpc::StatusCode::INTERNAL,
                              "Different row key in cell chunk");
        return;
      }
    }
    cells_.emplace_back(MovePartialToCell());
    cell_first_chunk_ = true;
  }

  if (chunk.reset_row()) {
    cells_.clear();
    cell_ = {};
    if (not cell_first_chunk_) {
      status = grpc::Status(grpc::StatusCode::INTERNAL,
                            "Reset row with an unfinished cell");
      return;
    }
  } else if (chunk.commit_row()) {
    if (not cell_first_chunk_) {
      status = grpc::Status(grpc::StatusCode::INTERNAL,
                            "Commit row with an unfinished cell");
      return;
    }
    if (cells_.empty()) {
      status = grpc::Status(grpc::StatusCode::INTERNAL,
                            "Commit row missing the row key");
      return;
    }
    row_ready_ = true;
    last_seen_row_key_ = row_key_;
    cell_.row.clear();
  }
}

void ReadRowsParser::HandleEndOfStream(grpc::Status& status) {
  if (end_of_stream_) {
    status = grpc::Status(grpc::StatusCode::INTERNAL,
                          "HandleEndOfStream called twice");
    return;
  }
  end_of_stream_ = true;

  if (not cell_first_chunk_) {
    status = grpc::Status(grpc::StatusCode::INTERNAL,
                          "end of stream with unfinished cell");
    return;
  }

  if (cells_.begin() != cells_.end() and not row_ready_) {
    status = grpc::Status(grpc::StatusCode::INTERNAL,
                          "end of stream with unfinished row");
    return;
  }
}

bool ReadRowsParser::HasNext() const { return row_ready_; }

Row ReadRowsParser::Next(grpc::Status& status) {
  if (not row_ready_) {
    status =
        grpc::Status(grpc::StatusCode::INTERNAL, "Next with row not ready");
    return Row("", {});
  }
  row_ready_ = false;

  Row row(std::move(row_key_), std::move(cells_));
  row_key_.clear();

  return row;
}

Cell ReadRowsParser::MovePartialToCell() {
  // The row, family, and column are explicitly copied because the
  // ReadRows v2 may reuse them in future chunks. See the CellChunk
  // message comments in bigtable.proto.
  Cell cell(cell_.row, cell_.family, cell_.column, cell_.timestamp,
            std::move(cell_.value), std::move(cell_.labels));
  cell_.value.clear();
  return cell;
}
}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
