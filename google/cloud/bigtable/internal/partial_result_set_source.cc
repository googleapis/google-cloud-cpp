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

namespace {
bool isValidChecksum(std::string data, uint32_t expected_checksum) {
  absl::crc32c_t computed_crc = absl::ComputeCrc32c(data);
  return static_cast<uint32_t>(computed_crc) == expected_checksum;
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

StatusOr<std::unique_ptr<PartialResultSourceInterface>>
PartialResultSetSource::Create(
    absl::optional<google::bigtable::v2::ResultSetMetadata> metadata,
    std::unique_ptr<PartialResultSetReader> reader) {
  std::unique_ptr<PartialResultSetSource> source(
      new PartialResultSetSource(std::move(metadata), std::move(reader)));

  // Do an initial read from the stream to determine the fate of the factory.
  auto status = source->ReadFromStream();

  // If the initial read finished the stream, and `Finish()` failed, then
  // creating the `PartialResultSetSource` should fail with the same error.
  if (source->state_ == kFinished && !status.ok()) return status;

  std::unique_ptr<PartialResultSourceInterface> interface_ptr =
      std::move(source);
  return interface_ptr;
}

PartialResultSetSource::PartialResultSetSource(
    absl::optional<google::bigtable::v2::ResultSetMetadata> metadata,
    std::unique_ptr<PartialResultSetReader> reader)
    : options_(internal::CurrentOptions()),
      reader_(std::move(reader)),
      metadata_(std::move(metadata)),
      values_(absl::make_optional(
          google::protobuf::Arena::Create<
              google::protobuf::RepeatedPtrField<google::protobuf::Value>>(
              &arena_))) {
  if (metadata_.has_value()) {
    columns_ = std::make_shared<std::vector<std::string>>();
    columns_->reserve(metadata_->proto_schema().columns_size());
    for (auto const& c : metadata_->proto_schema().columns()) {
      columns_->push_back(c.name());
    }
  }
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
  while (rows_.empty()) {
    if (state_ == kFinished) return bigtable::QueryRow();
    internal::OptionsSpan span(options_);
    // Continue fetching if there are more rows in the stream.
    auto status = ReadFromStream();
    if (!status.ok()) return status;
  }
  // Returns the row at the front of the queue
  auto row = std::move(rows_.front());
  rows_.pop_front();
  return row;
}

Status PartialResultSetSource::ReadFromStream() {
  if (state_ == kFinished) {
    return internal::InternalError("PartialResultSetSource already finished",
                                   GCP_ERROR_INFO());
  }
  // The application should consume rows_ before calling ReadFromStream again.
  if (!rows_.empty()) {
    return internal::InternalError("PartialResultSetSource has unconsumed rows",
                                   GCP_ERROR_INFO());
  }

  auto* raw_result_set =
      google::protobuf::Arena::Create<google::bigtable::v2::PartialResultSet>(
          &arena_);
  auto result_set =
      UnownedPartialResultSet::FromPartialResultSet(*raw_result_set);

  // The resume_token_ member holds the token from the previous
  // PartialResultSet. It's empty on the first call.
  if (reader_->Read(resume_token_, result_set)) {
    return ProcessDataFromStream(result_set.result);
  } else {
    state_ = kFinished;
    // The uncommitted_rows_ is expected to be empty because the last successful
    // read would have had a sentinel resume_token, causing
    // ProcessDataFromStream to commit them.
    if (!uncommitted_rows_.empty()) {
      return internal::InternalError("Stream ended with uncommitted rows.",
                                     GCP_ERROR_INFO());
    }
    return reader_->Finish();
  }
}

Status PartialResultSetSource::ProcessDataFromStream(
    google::bigtable::v2::PartialResultSet& result) {
  // If the `reset` is true then all the data buffered since the last
  // resume_token should be discarded.
  if (result.reset()) {
    read_buffer_.clear();
    uncommitted_rows_.clear();
  }

  // Reserve space of the buffer at the start of a new batch of data.
  if (result.estimated_batch_size() > 0 && read_buffer_.empty()) {
    read_buffer_.reserve(result.estimated_batch_size());
  }

  if (result.has_proto_rows_batch()) {
    absl::StrAppend(&read_buffer_, result.proto_rows_batch().batch_data());
  }

  if (result.has_batch_checksum() && !read_buffer_.empty()) {
    if (!isValidChecksum(read_buffer_, result.batch_checksum())) {
      read_buffer_.clear();
      uncommitted_rows_.clear();
      return internal::InternalError("Batch checksum mismatch");
    }
    google::bigtable::v2::ProtoRows proto_rows;
    if (proto_rows.ParseFromString(read_buffer_)) {
      if (metadata_.has_value()) {
        auto columns_size = metadata_.value().proto_schema().columns_size();
        auto parsed_value = proto_rows.values().begin();
        std::vector<bigtable::Value> values;
        values.reserve(columns_size);

        while (parsed_value != proto_rows.values().end()) {
          for (auto const& column : metadata_->proto_schema().columns()) {
            auto value = FromProto(column.type(), *parsed_value);
            values.push_back(std::move(value));
            ++parsed_value;
          }
          uncommitted_rows_.push_back(
              QueryRowFriend::MakeQueryRow(std::move(values), columns_));
          values.clear();
        }
      }
    } else {
      read_buffer_.clear();
      uncommitted_rows_.clear();
      return internal::InternalError("Failed to parse ProtoRows from buffer");
    }
  }

  // Buffered rows in uncommitted_rows_ are ready to be committed into rows_
  // once the resume_token is received.
  if (!result.resume_token().empty()) {
    rows_.insert(rows_.end(), uncommitted_rows_.begin(),
                 uncommitted_rows_.end());
    uncommitted_rows_.clear();
    resume_token_ = result.resume_token();
  }
  return {};  // OK
}
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
