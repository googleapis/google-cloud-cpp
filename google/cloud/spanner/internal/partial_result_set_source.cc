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

#include "google/cloud/spanner/internal/partial_result_set_source.h"
#include "google/cloud/spanner/internal/merge_chunk.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {

StatusOr<std::unique_ptr<ResultSourceInterface>> PartialResultSetSource::Create(
    std::unique_ptr<PartialResultSetReader> reader) {
  std::unique_ptr<PartialResultSetSource> source(
      new PartialResultSetSource(std::move(reader)));

  // Do the first read so the metadata is immediately available.
  auto status = source->ReadFromStream();
  if (!status.ok()) {
    return status;
  }

  // The first response must contain metadata.
  if (!source->metadata_) {
    return Status(StatusCode::kInternal, "response contained no metadata");
  }

  return {std::move(source)};
}

StatusOr<Row> PartialResultSetSource::NextRow() {
  if (finished_) {
    return Row();
  }

  while (buffer_.empty() || buffer_.size() < columns_->size()) {
    auto status = ReadFromStream();
    if (!status.ok()) {
      return status;
    }
    if (finished_) {
      if (chunk_) {
        return Status(StatusCode::kInternal,
                      "incomplete chunked_value at end of stream");
      }
      if (!buffer_.empty()) {
        return Status(StatusCode::kInternal, "incomplete row at end of stream");
      }
      return Row();
    }
  }

  auto const& fields = metadata_->row_type().fields();
  if (fields.empty()) {
    return Status(StatusCode::kInternal,
                  "response metadata is missing row type information");
  }

  std::vector<Value> values;
  values.reserve(fields.size());
  auto iter = buffer_.begin();
  for (auto const& field : fields) {
    values.push_back(FromProto(field.type(), std::move(*iter)));
    ++iter;
  }
  buffer_.erase(buffer_.begin(), iter);
  return internal::MakeRow(std::move(values), columns_);
}

PartialResultSetSource::~PartialResultSetSource() {
  if (!finished_) {
    // If there is actual data in the streaming RPC Finish() can deadlock, so
    // before trying to read the final status we need to cancel the streaming
    // RPC.
    reader_->TryCancel();
    // The user didn't iterate over all the data; finish the stream on their
    // behalf, but we have no way to communicate error status.
    auto finish_status = reader_->Finish();
    if (!finish_status.ok() && finish_status.code() != StatusCode::kCancelled) {
      GCP_LOG(WARNING) << "Finish() failed in destructor: " << finish_status;
    }
  }
}

Status PartialResultSetSource::ReadFromStream() {
  auto result_set = reader_->Read();
  if (!result_set) {
    // Read() returns false for end of stream, whether we read all the data or
    // encountered an error. Finish() tells us the status.
    finished_ = true;
    return reader_->Finish();
  }

  if (result_set->has_metadata()) {
    // If we got metadata more than once, log it, but use the first one.
    if (metadata_) {
      GCP_LOG(WARNING) << "Unexpectedly received two sets of metadata";
    } else {
      metadata_ = std::move(*result_set->mutable_metadata());
      // Copies the column names into a shared_ptr that will be shared with
      // every Row object returned from NextRow().
      columns_ = std::make_shared<std::vector<std::string>>();
      for (auto const& field : metadata_->row_type().fields()) {
        columns_->push_back(field.name());
      }
    }
  }

  if (result_set->has_stats()) {
    // If we got stats more than once, log it, but use the last one.
    if (stats_) {
      GCP_LOG(WARNING) << "Unexpectedly received two sets of stats";
    }
    stats_ = std::move(*result_set->mutable_stats());
  }

  auto& new_values = *result_set->mutable_values();

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
  if (chunk_) {
    if (new_values.empty()) {
      return Status(StatusCode::kInternal,
                    "PartialResultSet contained no values "
                    "to merge with prior chunked_value");
    }
    auto& front = new_values[0];
    auto merge_status = MergeChunk(*chunk_, std::move(front));
    if (!merge_status.ok()) {
      return merge_status;
    }
    using std::swap;
    swap(*chunk_, front);
    chunk_ = {};
  }

  if (result_set->chunked_value()) {
    if (new_values.empty()) {
      return Status(StatusCode::kInternal,
                    "PartialResultSet had chunked_value "
                    "set true but contained no values");
    }
    chunk_ = std::move(new_values[new_values.size() - 1]);
    new_values.RemoveLast();
  }

  // Moves all the remaining in new_values to buffer_
  for (auto& value_proto : new_values) {
    buffer_.push_back(std::move(value_proto));
  }

  return {};  // OK
}

}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google
