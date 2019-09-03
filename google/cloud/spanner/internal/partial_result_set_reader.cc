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
#include "google/cloud/spanner/internal/merge_chunk.h"
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
  while (next_value_index_ >= values_.size()) {
    // Ran out of buffered values - try to read some more from gRPC.
    next_value_index_ = 0;
    auto status = ReadFromStream();
    if (!status.ok()) {
      return status;
    }
    if (finished_) {
      if (partial_chunked_value_.has_value()) {
        return Status(StatusCode::kInternal,
                      "incomplete chunked_value at end of stream");
      }
      return optional<Value>();
    }
    // If the response contained any values, the loop will exit, otherwise
    // continue reading until we do get at least one value.
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
                    std::move(*values_.Mutable(next_value_index_++)))};
}

PartialResultSetReader::~PartialResultSetReader() {
  if (!finished_) {
    // If there is actual data in the streaming RPC Finish() can deadlock, so
    // before trying to read the final status we need to cancel the streaming
    // RPC.
    context_->TryCancel();
    // The user didn't iterate over all the data; finish the stream on their
    // behalf, but we have no way to communicate error status.
    grpc::Status finish_status = grpc_reader_->Finish();
    if (!finish_status.ok() &&
        finish_status.error_code() != grpc::StatusCode::CANCELLED) {
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

  values_.Swap(result_set.mutable_values());

  // Merge values if necessary, as described in:
  // https://cloud.google.com/spanner/docs/reference/rpc/google.spanner.v1#google.spanner.v1.PartialResultSet
  //
  // As an example, if we receive the following 4 responses (assume the values
  // are all `string_value`s of type `STRING`):
  //
  // ```
  // { { values: ["A", "B", "C1"] }  chunked_value: true }
  // { { values: ["C2", "D", "E1"] } chunked_value: true }
  // { { values: ["E2"] },           chunked_value: true }
  // { { values: ["E3", "F"] }       chunked_value: false }
  // ```
  //
  // The final values yielded are: `A`, `B`, `C1C2`, `D`, `E1E2E3`, `F`.
  //
  // n.b. One value can span more than two responses (the `E1E2E3` case above);
  // the code "just works" without needing to treat that as a special-case.

  // If the last response had `chunked_value` set, `partial_chunked_value_`
  // contains the partial value from that response; merge it with the first
  // value in this response and set the first entry in `values_` to the result.
  if (partial_chunked_value_.has_value()) {
    if (values_.empty()) {
      return Status(StatusCode::kInternal,
                    "PartialResultSet contained no values to merge with prior "
                    "chunked_value");
    }
    auto merge_status =
        MergeChunk(*partial_chunked_value_, std::move(values_[0]));
    if (!merge_status.ok()) {
      return merge_status;
    }
    // Move the merged value to the front of the array and make
    // `partial_chunked_value_` empty.
    values_[0] = *std::move(partial_chunked_value_);
    partial_chunked_value_.reset();
  }

  // If `chunked_value` is set, the last value in *this* response is incomplete
  // and needs to be merged with the first value in the *next* response. Move it
  // into `partial_chunked_value_`; this ensures everything in `values_` is a
  // complete value, which simplifies things elsewhere.
  if (result_set.chunked_value()) {
    if (values_.empty()) {
      return Status(StatusCode::kInternal,
                    "PartialResultSet had chunked_value set true but contained "
                    "no values");
    }
    partial_chunked_value_ = std::move(values_[values_.size() - 1]);
    values_.RemoveLast();
  }
  return Status();
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
