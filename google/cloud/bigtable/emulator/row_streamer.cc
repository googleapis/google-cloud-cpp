// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/emulator/row_streamer.h"
#include <google/bigtable/v2/bigtable.grpc.pb.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

namespace btproto = ::google::bigtable::v2;

RowStreamer::RowStreamer(grpc::ServerWriter<btproto::ReadRowsResponse>& writer)
    : writer_(writer) {}

bool RowStreamer::Stream(
    std::tuple<std::string const&, std::string const&, std::string const&,
               std::int64_t, std::string const&> const& cell) {
  std::cout << "Attempting to stream" << std::endl;
  btproto::ReadRowsResponse::CellChunk chunk;
  if (!current_row_key_ || (&current_row_key_->get() != &std::get<0>(cell) && 
                           current_row_key_->get() != std::get<0>(cell))) {
    if (!pending_chunks_.empty()) {
      pending_chunks_.back().set_commit_row(true);
    }
    current_row_key_ = std::cref(std::get<0>(cell));
    current_column_family_ = std::cref(std::get<1>(cell));
    current_column_qualifier_ = std::cref(std::get<2>(cell));
    chunk.set_row_key(std::get<0>(cell));
    chunk.mutable_family_name()->set_value(std::get<1>(cell));
    chunk.mutable_qualifier()->set_value(std::get<2>(cell));
  }
  if (&current_column_family_->get() != &std::get<1>(cell) && 
                           current_row_key_->get() != std::get<1>(cell)) {
    current_column_family_ = std::cref(std::get<1>(cell));
    current_column_qualifier_ = std::cref(std::get<2>(cell));
    chunk.mutable_family_name()->set_value(std::get<1>(cell));
    chunk.mutable_qualifier()->set_value(std::get<2>(cell));
  }
  if (&current_column_qualifier_->get() != &std::get<2>(cell) && 
                           current_row_key_->get() != std::get<2>(cell)) {
    current_column_qualifier_ = std::cref(std::get<2>(cell));
    chunk.mutable_qualifier()->set_value(std::get<2>(cell));
  }
  chunk.set_timestamp_micros(std::get<3>(cell));
  chunk.set_value(std::get<4>(cell));
  pending_chunks_.emplace_back(std::move(chunk));
  if (pending_chunks_.size() > 200) {
    return Flush(false);
  }
  std::cout << "Not flushing" << std::endl;
  return true;
}

bool RowStreamer::Flush(bool stream_finished) {
    std::cout << "Flushing" << std::endl;
  absl::optional<btproto::ReadRowsResponse::CellChunk> dont_flush_this;
  if (stream_finished) {
    if (!pending_chunks_.empty()) {
      pending_chunks_.back().set_commit_row(true);
    }
    current_row_key_.reset();
    current_column_family_.reset();
    current_column_qualifier_.reset();
  } else {
    if (!pending_chunks_.empty()) {
      dont_flush_this = std::move(pending_chunks_.back());
      pending_chunks_.resize(pending_chunks_.size() - 1);
    }
  }
  btproto::ReadRowsResponse resp;
  for (auto &chunk : pending_chunks_) {
    *resp.add_chunks() = std::move(chunk);
  }
  pending_chunks_.resize(0);
  if (dont_flush_this) {
    pending_chunks_.emplace_back(*std::move(dont_flush_this));
  }
  std::cout << "Writing: " << resp.DebugString() << std::endl;
  return writer_.Write(resp);
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

