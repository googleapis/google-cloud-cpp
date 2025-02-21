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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_ROW_STREAMER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_ROW_STREAMER_H

#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include "google/cloud/bigtable/emulator/cell_view.h"
#include <grpcpp/server.h>
#include "absl/types/optional.h"

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

class RowStreamer {
 public:
  RowStreamer(
      grpc::ServerWriter<google::bigtable::v2::ReadRowsResponse>& writer);
  bool Stream(CellView const& cell_view);

  bool Flush(bool stream_finished);

 private:
  grpc::ServerWriter<google::bigtable::v2::ReadRowsResponse>& writer_;
  absl::optional<std::string> current_row_key_;
  absl::optional<std::string> current_column_family_;
  absl::optional<std::string> current_column_qualifier_;
  std::vector<google::bigtable::v2::ReadRowsResponse::CellChunk>
      pending_chunks_;
};

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_ROW_STREAMER_H

