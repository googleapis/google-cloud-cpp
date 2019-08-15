// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/partial_result_set_reader.h"
#include "google/cloud/grpc_utils/grpc_error_delegate.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

StatusOr<std::unique_ptr<PartialResultSetReader>>
PartialResultSetReader::Create(std::unique_ptr<grpc::ClientContext> context,
                               std::unique_ptr<GrpcReader> grpc_reader) {
  std::unique_ptr<PartialResultSetReader> reader(
      new PartialResultSetReader(std::move(context), std::move(grpc_reader)));

  // Do the first read so the metadata is immediately available.
  auto status = reader->ReadFromStream();
  if (!status.ok()) {
    return status;
  }

  // The first response must contain metadata.
  if (!reader->metadata_.has_value()) {
    return Status(StatusCode::kInternal, "response contained no metadata");
  }

  return {std::move(reader)};
}

StatusOr<optional<Value>> PartialResultSetReader::NextValue() {
  if (finished_) {
    return optional<Value>();
  }
  if (next_value_index_ >= values_.size()) {
    // Ran out of buffered values - try to read some more from gRPC.
    next_value_index_ = 0;
    auto status = ReadFromStream();
    if (!status.ok()) {
      return status;
    }
    if (finished_) {
      return optional<Value>();
    }
    if (values_.empty()) {
      return Status(StatusCode::kInternal, "response has no values");
    }
  }

  if (metadata_->row_type().fields().empty()) {
    return Status(StatusCode::kInternal,
                  "response metadata is missing row type information");
  }

  // The metadata tells us the sequence of types for the field Values;
  // when we reach the end of the sequence start over at the beginning.
  auto const& fields = metadata_->row_type().fields();
  if (next_value_type_index_ >= fields.size()) {
    next_value_type_index_ = 0;
  }

  return {FromProto(fields.Get(next_value_type_index_++).type(),
                    values_.Get(next_value_index_++))};
}

PartialResultSetReader::~PartialResultSetReader() {
  if (!finished_) {
    // The user didn't iterate over all the data; finish the stream on their
    // behalf, but we have no way to communicate error status.
    grpc::Status finish_status = grpc_reader_->Finish();
    if (!finish_status.ok()) {
      GCP_LOG(WARNING) << "Finish() failed in destructor: "
                       << grpc_utils::MakeStatusFromRpcError(finish_status);
    }
  }
}

Status PartialResultSetReader::ReadFromStream() {
  google::spanner::v1::PartialResultSet result_set;
  if (!grpc_reader_->Read(&result_set)) {
    // Read() returns false for end of stream, whether we read all the data or
    // encountered an error. Finish() tells us the status.
    finished_ = true;
    grpc::Status finish_status = grpc_reader_->Finish();
    return grpc_utils::MakeStatusFromRpcError(finish_status);
  }

  if (result_set.has_metadata()) {
    if (!metadata_.has_value()) {
      metadata_ = std::move(*result_set.mutable_metadata());
    } else {
      GCP_LOG(WARNING) << "Unexpectedly received two sets of metadata";
    }
  }

  if (result_set.has_stats()) {
    // We should only get stats once; if not, use the last one.
    if (stats_.has_value()) {
      GCP_LOG(WARNING) << "Unexpectedly received two sets of stats";
    }
    stats_ = std::move(*result_set.mutable_stats());
  }

  // TODO(#271) store and use resume_token.
  // TODO(#270) handle chunked_value.
  values_.Swap(result_set.mutable_values());
  return Status();
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
