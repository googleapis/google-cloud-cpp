// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/spanner/internal/partial_result_set_source.h"
#include "google/cloud/spanner/internal/merge_chunk.h"
#include "google/cloud/spanner/options.h"
#include "google/cloud/log.h"
#include "absl/container/fixed_array.h"

namespace google {
namespace cloud {
namespace spanner_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

using Values = google::protobuf::RepeatedPtrField<google::protobuf::Value>;

// Efficiently move values from one repeated field to another. Starts at
// index `start`, but always clears all of `src`. This is worth optimizing
// because it is on the primary path for getting bulk data to the user.
void ExtractSubrangeAndAppend(Values& src, int start, Values& dst) {
  auto* const src_arena = src.GetArena();
  auto* const dst_arena = dst.GetArena();
  // Note: I've tested both branches of this conditional, but we probably
  // need some mechanism to exercise both in ongoing automated tests.
  if (dst_arena == src_arena || src_arena == nullptr) {
    auto add_allocated = (dst_arena == src_arena)
                             ? &Values::UnsafeArenaAddAllocated
                             : &Values::AddAllocated;
    auto const n_values = src.size() - start;
    absl::FixedArray<google::protobuf::Value*> values(n_values);
    src.UnsafeArenaExtractSubrange(start, n_values, values.data());
    for (auto* value : values) {
      (dst.*add_allocated)(value);
    }
  } else {
    while (start != src.size()) {
      dst.Add(std::move(*src.Mutable(start++)));
    }
  }
  src.Clear();
}

}  // namespace

StatusOr<std::unique_ptr<ResultSourceInterface>> PartialResultSetSource::Create(
    std::unique_ptr<PartialResultSetReader> reader) {
  std::unique_ptr<PartialResultSetSource> source(
      new PartialResultSetSource(std::move(reader)));

  // Do an initial read from the stream to determine the fate of the factory.
  auto status = source->ReadFromStream();

  // If the initial read finished the stream, and `Finish()` failed, then
  // creating the `PartialResultSetSource` should fail with the same error.
  if (source->state_ == kFinished && !status.ok()) return status;

  // Otherwise we require that the first response contains the metadata.
  // Without it, creating the `PartialResultSetSource` should fail.
  if (!source->metadata_) {
    return Status(StatusCode::kInternal,
                  "PartialResultSetSource response contained no metadata");
  }

  return {std::move(source)};
}

PartialResultSetSource::PartialResultSetSource(
    std::unique_ptr<PartialResultSetReader> reader)
    : options_(internal::CurrentOptions()), reader_(std::move(reader)) {
  if (options_.has<spanner::StreamingResumabilityBufferSizeOption>()) {
    values_space_limit_ =
        options_.get<spanner::StreamingResumabilityBufferSizeOption>();
  }
}

PartialResultSetSource::~PartialResultSetSource() {
  internal::OptionsSpan span(options_);
  if (state_ == kReading) {
    // Finish() can deadlock if there is still data in the streaming RPC,
    // so before trying to read the final status we need to cancel.
    reader_->TryCancel();
    state_ = kEndOfStream;
  }
  if (state_ == kEndOfStream) {
    // The user didn't iterate over all the data, so finish the stream on
    // their behalf, although we have no way to communicate error status.
    auto status = reader_->Finish();
    if (!status.ok() && status.code() != StatusCode::kCancelled) {
      GCP_LOG(WARNING)
          << "PartialResultSetSource: Finish() failed in destructor: "
          << status;
    }
    state_ = kFinished;
  }
}

StatusOr<spanner::Row> PartialResultSetSource::NextRow() {
  while (rows_.empty()) {
    if (state_ == kFinished) return spanner::Row();
    internal::OptionsSpan span(options_);
    auto status = ReadFromStream();
    if (!status.ok()) return status;
  }
  auto row = std::move(rows_.front());
  rows_.pop_front();
  return row;
}

Status PartialResultSetSource::ReadFromStream() {
  absl::optional<PartialResultSet> result_set;
  if (state_ == kFinished || !rows_.empty()) {
    return Status(StatusCode::kInternal, "PartialResultSetSource state error");
  }
  if (state_ == kReading) {
    result_set = reader_->Read(resume_token_);
    if (!result_set) state_ = kEndOfStream;
  }
  if (state_ == kEndOfStream) {
    // If we have no buffered data, we're done.
    if (values_.empty()) {
      state_ = kFinished;
      return reader_->Finish();
    }
    // Otherwise, proceed with a `PartialResultSet` using a fake resume
    // token to flush the buffer. The service does not appear to yield
    // a resume token in its final response, despite it completing a row.
    result_set = PartialResultSet{{}, false};
    result_set->result.set_resume_token("<end-of-stream>");
  }

  if (result_set->result.has_metadata()) {
    // If we get metadata more than once, log it, but use the first one.
    if (metadata_) {
      GCP_LOG(WARNING) << "PartialResultSetSource: Additional metadata";
    } else {
      metadata_ = std::move(*result_set->result.mutable_metadata());
      // Copy the column names into a vector that will be shared with
      // every Row object returned from NextRow().
      columns_ = std::make_shared<std::vector<std::string>>();
      columns_->reserve(metadata_->row_type().fields_size());
      for (auto const& field : metadata_->row_type().fields()) {
        columns_->push_back(field.name());
      }
    }
  }
  if (result_set->result.has_stats()) {
    // If we get stats more than once, log it, but use the last one.
    if (stats_) {
      GCP_LOG(WARNING) << "PartialResultSetSource: Additional stats";
    }
    stats_ = std::move(*result_set->result.mutable_stats());
  }

  // If reader_->Read() resulted in a new PartialResultSetReader (i.e., it
  // used the token to resume an interrupted stream), then we must discard
  // any buffered data as it will be replayed.
  if (result_set->resumption) {
    if (!resume_token_) {
      // The reader claims to have resumed the stream even though we said it
      // should not. That leaves us in the untenable position of possibly
      // having returned data that will be replayed, so fail the stream now.
      return Status(StatusCode::kInternal,
                    "PartialResultSetSource reader resumed the stream"
                    " despite our having asked it not to");
    }
    values_back_incomplete_ = false;
    values_.Clear();
  }

  // If the final value in the previous `PartialResultSet` was incomplete,
  // it must be combined with the first value from the new set. And then
  // we move everything remaining from the new set to the end of `values_`.
  if (!result_set->result.values().empty()) {
    auto& new_values = *result_set->result.mutable_values();
    int append_start = 0;
    if (values_back_incomplete_) {
      auto& first = *new_values.Mutable(append_start++);
      auto status = MergeChunk(*values_.rbegin(), std::move(first));
      if (!status.ok()) return status;
    }
    ExtractSubrangeAndAppend(new_values, append_start, values_);
    values_back_incomplete_ = result_set->result.chunked_value();
  }

  // Deliver whatever rows we can muster.
  auto const n_values = values_.size() - (values_back_incomplete_ ? 1 : 0);
  auto const n_columns = columns_ ? static_cast<int>(columns_->size()) : 0;
  auto n_rows = n_columns ? n_values / n_columns : 0;
  if (n_columns == 0 && !values_.empty()) {
    return Status(StatusCode::kInternal,
                  "PartialResultSetSource metadata is missing row type");
  }

  // If we didn't receive a resume token, and have not exceeded our buffer
  // limit, then we choose to `Read()` again so as to maintain resumability.
  if (result_set->result.resume_token().empty()) {
    if (values_.SpaceUsedExcludingSelfLong() < values_space_limit_) {
      return {};  // OK
    }
  }

  // If we did receive a resume token then everything should be deliverable,
  // and we'll be able to resume the stream at this point after a breakage.
  // Otherwise, if we deliver anything at all, we must disable resumability.
  if (!result_set->result.resume_token().empty()) {
    resume_token_ = result_set->result.resume_token();
    if (n_rows * n_columns != values_.size()) {
      if (state_ != kEndOfStream) {
        return Status(StatusCode::kInternal,
                      "PartialResultSetSource reader produced a resume token"
                      " that is not on a row boundary");
      }
      if (n_rows == 0) {
        return Status(StatusCode::kInternal,
                      "PartialResultSetSource stream ended at a point"
                      " that is not on a row boundary");
      }
    }
  } else if (n_rows != 0) {
    resume_token_ = absl::nullopt;
  }

  // Combine the available values into new elements of `rows_`.
  int values_pos = 0;
  std::vector<spanner::Value> values;
  values.reserve(n_columns);
  for (; n_rows != 0; --n_rows) {
    for (auto const& field : metadata_->row_type().fields()) {
      auto& value = *values_.Mutable(values_pos++);
      values.push_back(FromProto(field.type(), std::move(value)));
    }
    rows_.push_back(RowFriend::MakeRow(std::move(values), columns_));
    values.clear();
  }

  // If we didn't combine all the values, leave the remainder for next time.
  auto* rem_values = result_set->result.mutable_values();
  ExtractSubrangeAndAppend(values_, values_pos, *rem_values);
  values_.Swap(rem_values);

  return {};  // OK
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner_internal
}  // namespace cloud
}  // namespace google
