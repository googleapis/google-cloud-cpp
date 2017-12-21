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

#include "bigtable/client/detail/readrowsparser.h"

#include <stdexcept>

namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
using google::bigtable::v2::ReadRowsResponse_CellChunk;

void ReadRowsParser::HandleChunk(const ReadRowsResponse_CellChunk& chunk) {
  if (not chunk.row_key().empty()) {
    if (last_seen_row_key_.compare(chunk.row_key()) >= 0) {
      throw std::runtime_error("Row keys are expected in increasing order");
    }
    cell_.row = chunk.row_key();
  }

  if (chunk.has_family_name()) {
    if (not chunk.has_qualifier()) {
      throw std::runtime_error("New column family must specify qualifier");
    }
    cell_.family = chunk.family_name().value();
  }

  if (chunk.has_qualifier()) {
    cell_.column = chunk.qualifier().value();
  }

  if (cell_first_chunk_) {
    cell_.timestamp = chunk.timestamp_micros();
  }
  cell_first_chunk_ = false;

  for (const auto& l : chunk.labels()) {
    cell_.labels.push_back(l);
  }

  if (chunk.value_size() > 0) {
    cell_.value.reserve(chunk.value_size());
  }
  cell_.value.append(chunk.value());

  // Last chunk in the cell has zero for value size
  if (chunk.value_size() == 0) {
    if (cells_.empty()) {
      if (cell_.row.empty()) {
        throw std::runtime_error("Missing row key at last chunk in cell");
      }
      row_key_ = cell_.row;
    } else {
      if (row_key_.compare(cell_.row) != 0) {
        throw std::runtime_error("Different row key in cell chunk");
      }
    }
    cells_.emplace_back(cell_.MoveToCell());
    cell_first_chunk_ = true;
  }

  if (chunk.reset_row()) {
    cells_.clear();
    cell_ = {};
    if (not cell_first_chunk_) {
      throw std::runtime_error("Reset row with an unfinished cell");
    }
  } else if (chunk.commit_row()) {
    if (not cell_first_chunk_) {
      throw std::runtime_error("Commit row with an unfinished cell");
    }
    if (cells_.empty()) {
      throw std::runtime_error("Commit row missing the row key");
    }
    row_ready_ = true;
    last_seen_row_key_ = row_key_;
    cell_.row.clear();
  }
}

void ReadRowsParser::HandleEOT() {
  if (not cell_first_chunk_) {
    throw std::runtime_error("EOT with unfinished cell");
  }

  if (cells_.begin() != cells_.end() and not row_ready_) {
    throw std::runtime_error("EOT with unfinished row");
  }
}

bool ReadRowsParser::HasNext() { return row_ready_; }

Row ReadRowsParser::Next() {
  if (not row_ready_) {
    throw std::runtime_error("Next with row not ready");
  }
  row_ready_ = false;

  Row row(std::move(row_key_), std::move(cells_));
  row_key_ = "";
  cells_.clear();

  return row;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
