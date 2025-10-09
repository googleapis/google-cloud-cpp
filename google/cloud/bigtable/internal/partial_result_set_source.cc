// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/internal/partial_result_set_source.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include "absl/container/fixed_array.h"
#include "absl/types/optional.h"

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN


StatusOr<std::unique_ptr<PartialResultSourceInterface>>
PartialResultSetSource::Create(std::unique_ptr<PartialResultSetReader> reader) {
  std::unique_ptr<PartialResultSetSource> source(
      new PartialResultSetSource(std::move(reader)));

  // Do an initial read from the stream to determine the fate of the factory.
  auto status = source->ReadFromStream();

  // If the initial read finished the stream, and `Finish()` failed, then
  // creating the `PartialResultSetSource` should fail with the same error.
  if (source->state_ == kFinished && !status.ok()) return status;

  return {std::move(source)};
}

PartialResultSetSource::PartialResultSetSource(
    std::unique_ptr<PartialResultSetReader> reader)
    : options_(internal::CurrentOptions()),
      reader_(std::move(reader)),
      values_(absl::make_optional(
          google::protobuf::Arena::Create<
              google::protobuf::RepeatedPtrField<google::protobuf::Value>>(
              &arena_))) {
  if (options_.has<bigtable::StreamingResumabilityBufferSizeOption>()) {
    values_space_limit_ =
        options_.get<bigtable::StreamingResumabilityBufferSizeOption>();
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

StatusOr<bigtable::QueryRow> PartialResultSetSource::NextRow() {
  if (usable_rows_ == 0 && rows_returned_ > 0) {
    // There may be complete or partial rows in values_ that haven't been
    // returned to the clients yet. Let's copy it over before we reset
    // the arena.
    auto* values = *values_;
    int partial_size =
        static_cast<int>(values->size() - rows_returned_ * columns_->size());
    absl::FixedArray<google::protobuf::Value*> tmp(partial_size);
    if (!tmp.empty()) {
      values->ExtractSubrange(values->size() - partial_size, partial_size,
                              tmp.data());
    }
    values_.reset();
    values_space_.Clear();
    arena_.Reset();
    values_.emplace(
        google::protobuf::Arena::Create<
            google::protobuf::RepeatedPtrField<google::protobuf::Value>>(
            &arena_));
    values = *values_;
    for (auto* elem : tmp) {
      values->Add(std::move(*elem));
      delete elem;
    }
    rows_returned_ = 0;
  }
  while (usable_rows_ == 0) {
    if (state_ == kFinished) return bigtable::QueryRow();
    internal::OptionsSpan span(options_);
    auto status = ReadFromStream();
    if (!status.ok()) return status;
  }
  ++rows_returned_;
  --usable_rows_;
  std::vector<bigtable::Value> values;
  return QueryRowFriend::MakeQueryRow(std::move(values), columns_);
}

Status PartialResultSetSource::ReadFromStream() {
  if (state_ == kFinished || usable_rows_ != 0 || rows_returned_ != 0) {
    return internal::InternalError("PartialResultSetSource state error",
                                   GCP_ERROR_INFO());
  }
  auto* values = *values_;
  auto* raw_result_set =
      google::protobuf::Arena::Create<google::bigtable::v2::PartialResultSet>(
          &arena_);
  auto result_set =
      UnownedPartialResultSet::FromPartialResultSet(*raw_result_set);
  if (state_ == kReading) {
    if (!reader_->Read(resume_token_, result_set)) state_ = kEndOfStream;
  }
  if (state_ == kEndOfStream) {
    // If we have no buffered data, we're done.
    if (values->empty()) {
      state_ = kFinished;
      return reader_->Finish();
    }
    // Otherwise, proceed with a `PartialResultSet` using a fake resume
    // token to flush the buffer. The service does not appear to yield
    // a resume token in its final response, despite it completing a row.
    result_set.result.set_resume_token("<end-of-stream>");
  }

  // ExecuteQueryResponse contains metadata and partialResult
//   // Copy the column names into a vector that will be shared with
//   // every Row object returned from NextRow().
    //   columns_ = std::make_shared<std::vector<std::string>>();
    //   columns_->reserve(result_set.result.proto_rows_batch.fields_size());
    //   for (auto const& field : metadata_->row_type().fields()) {
    //     columns_->push_back(field.name());
    //   }
//     }
//   }

  // If reader_->Read() resulted in a new PartialResultSetReader (i.e., it
  // used the token to resume an interrupted stream), then we must discard
  // any buffered data as it will be replayed.
  if (result_set.resumption) {
    if (!resume_token_) {
      // The reader claims to have resumed the stream even though we said it
      // should not. That leaves us in the untenable position of possibly
      // having returned data that will be replayed, so fail the stream now.
      return internal::InternalError(
          "PartialResultSetSource reader resumed the stream"
          " despite our having asked it not to",
          GCP_ERROR_INFO());
    }
    values_back_incomplete_ = false;
    values->Clear();
    values_space_.Clear();
  }

  // Deliver whatever rows we can muster.
  auto const n_values = values->size() - (values_back_incomplete_ ? 1 : 0);
  auto const n_columns = columns_ ? static_cast<int>(columns_->size()) : 0;
  auto n_rows = n_columns ? n_values / n_columns : 0;
  if (n_columns == 0 && !values->empty()) {
    return internal::InternalError(
        "PartialResultSetSource metadata is missing row type",
        GCP_ERROR_INFO());
  }

  // If we didn't receive a resume token, and have not exceeded our buffer
  // limit, then we choose to `Read()` again so as to maintain resumability.
  if (result_set.result.resume_token().empty() && values_space_limit_ > 0) {
    for (auto it = values->begin() + values_space_.index; it != values->end();
         ++it) {
      values_space_.space_used += it->SpaceUsedLong();
    }
    values_space_.index = values->size();
    if (values_space_.space_used < values_space_limit_) {
      return {};  // OK
    }
  }

  // If we did receive a resume token then everything should be deliverable,
  // and we'll be able to resume the stream at this point after a breakage.
  // Otherwise, if we deliver anything at all, we must disable resumability.
  if (!result_set.result.resume_token().empty()) {
    resume_token_ = result_set.result.resume_token();
    if (n_rows * n_columns != values->size()) {
      if (state_ != kEndOfStream) {
        return internal::InternalError(
            "PartialResultSetSource reader produced a resume token"
            " that is not on a row boundary",
            GCP_ERROR_INFO());
      }
      if (n_rows == 0) {
        return internal::InternalError(
            "PartialResultSetSource stream ended at a point"
            " that is not on a row boundary",
            GCP_ERROR_INFO());
      }
    }
  } else if (n_rows != 0) {
    resume_token_ = absl::nullopt;
  }

  usable_rows_ = n_rows;
  return {};  // OK
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
