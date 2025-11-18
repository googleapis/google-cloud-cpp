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
#include "google/cloud/bigtable/internal/crc32c.h"
#include "google/cloud/bigtable/options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"

namespace google {
namespace cloud {
namespace bigtable_internal {

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
// Some Bigtable proto fields use Cord internally and string externally.
template <typename T, typename std::enable_if<
                          std::is_same<T, std::string>::value>::type* = nullptr>
std::string AsString(T const& s) {
  return s;
}

template <typename T, typename std::enable_if<
                          std::is_same<T, absl::Cord>::value>::type* = nullptr>
std::string AsString(T const& s) {
  return std::string(s);
}
}  // namespace

StatusOr<std::unique_ptr<bigtable::ResultSourceInterface>>
PartialResultSetSource::Create(
    absl::optional<google::bigtable::v2::ResultSetMetadata> metadata,
    std::shared_ptr<OperationContext> operation_context,
    std::unique_ptr<PartialResultSetReader> reader) {
  std::unique_ptr<PartialResultSetSource> source(new PartialResultSetSource(
      std::move(metadata), std::move(operation_context), std::move(reader)));

  // Do an initial read from the stream to determine the fate of the factory.
  auto status = source->ReadFromStream();

  // If the initial read finished the stream, and `Finish()` failed, then
  // creating the `PartialResultSetSource` should fail with the same error.
  if (source->state_ == State::kFinished && !status.ok()) return status;

  return {std::move(source)};
}

PartialResultSetSource::PartialResultSetSource(
    absl::optional<google::bigtable::v2::ResultSetMetadata> metadata,
    std::shared_ptr<OperationContext> operation_context,
    std::unique_ptr<PartialResultSetReader> reader)
    : options_(internal::CurrentOptions()),
      reader_(std::move(reader)),
      operation_context_(std::move(operation_context)),
      metadata_(std::move(metadata)) {
  if (metadata_.has_value()) {
    columns_ = std::make_shared<std::vector<std::string>>();
    columns_->reserve(metadata_->proto_schema().columns_size());
    for (auto const& c : metadata_->proto_schema().columns()) {
      columns_->push_back(c.name());
    }
  }
}

PartialResultSetSource::~PartialResultSetSource() {
  internal::OptionsSpan span(options_);
  if (state_ == State::kReading) {
    // Finish() can deadlock if there is still data in the streaming RPC,
    // so before trying to read the final status we need to cancel.
    reader_->TryCancel();
    state_ = State::kEndOfStream;
  }
  if (state_ == State::kEndOfStream) {
    // The user didn't iterate over all the data, so finish the stream on
    // their behalf, although we have no way to communicate error status.
    auto status = reader_->Finish();
    if (!status.ok() && status.code() != StatusCode::kCancelled) {
      GCP_LOG(WARNING)
          << "PartialResultSetSource: Finish() failed in destructor: "
          << status;
    }
    state_ = State::kFinished;
  }

  operation_context_->OnDone(last_status_);
}

StatusOr<bigtable::QueryRow> PartialResultSetSource::NextRow() {
  operation_context_->ElementRequest(reader_->context());
  while (rows_.empty()) {
    if (state_ == State::kFinished) {
      operation_context_->ElementDelivery(reader_->context());
      return bigtable::QueryRow();
    }
    internal::OptionsSpan span(options_);
    // Continue fetching if there are more rows in the stream.
    auto status = ReadFromStream();
    last_status_ = status;
    if (!status.ok()) return status;
  }
  // Returns the row at the front of the queue
  auto row = std::move(rows_.front());
  rows_.pop_front();
  operation_context_->ElementDelivery(reader_->context());
  return row;
}

Status PartialResultSetSource::ReadFromStream() {
  if (state_ == State::kFinished) {
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
  }
  state_ = State::kFinished;
  // buffered_rows_ and read_buffer_ are expected to be empty because the last
  // successful read would have had a sentinel resume_token, causing
  // ProcessDataFromStream to commit them.
  if (!buffered_rows_.empty() || !read_buffer_.empty()) {
    return internal::InternalError("Stream ended with uncommitted rows.",
                                   GCP_ERROR_INFO());
  }
  last_status_ = reader_->Finish();
  return last_status_;
}

Status PartialResultSetSource::ProcessDataFromStream(
    google::bigtable::v2::PartialResultSet& result) {
  // If the `reset` is true then all the data buffered since the last
  // resume_token should be discarded.
  if (result.reset()) {
    read_buffer_.clear();
    buffered_rows_.clear();
  }

  // Reserve space of the buffer at the start of a new batch of data.
  if (result.estimated_batch_size() > 0 && read_buffer_.empty()) {
    read_buffer_.reserve(result.estimated_batch_size());
  }

  if (result.has_proto_rows_batch()) {
    absl::StrAppend(&read_buffer_, result.proto_rows_batch().batch_data());
  }

  if (result.has_batch_checksum() && !read_buffer_.empty()) {
    if (bigtable_internal::Crc32c(read_buffer_) != result.batch_checksum()) {
      state_ = State::kFinished;
      read_buffer_.clear();
      buffered_rows_.clear();
      return internal::InternalError("Unexpected checksum mismatch",
                                     GCP_ERROR_INFO());
    }
    if (proto_rows_.ParseFromString(read_buffer_)) {
      auto status = BufferProtoRows();
      proto_rows_.Clear();
      read_buffer_.clear();
      if (!status.ok()) return status;
    } else {
      read_buffer_.clear();
      buffered_rows_.clear();
      return internal::InternalError("Failed to parse ProtoRows from buffer",
                                     GCP_ERROR_INFO());
    }
  }

  // Buffered rows in buffered_rows_ are ready to be committed into rows_
  // once the resume_token is received.
  if (!result.resume_token().empty()) {
    rows_.insert(rows_.end(), buffered_rows_.begin(), buffered_rows_.end());
    buffered_rows_.clear();
    read_buffer_.clear();
    resume_token_ = AsString(result.resume_token());
  }
  return {};  // OK
}

Status PartialResultSetSource::BufferProtoRows() {
  if (metadata_.has_value()) {
    auto const& proto_schema = metadata_->proto_schema();
    auto const columns_size = proto_schema.columns_size();
    auto const& proto_values = proto_rows_.values();

    if (columns_size == 0 && !proto_values.empty()) {
      return internal::InternalError(
          "ProtoRows has values but the schema has no columns.",
          GCP_ERROR_INFO());
    }
    if (proto_values.size() % columns_size != 0) {
      return internal::InternalError(
          "The number of values in ProtoRows is not a multiple of the "
          "number of columns in the schema.",
          GCP_ERROR_INFO());
    }

    auto parsed_value = proto_values.begin();
    std::vector<bigtable::Value> values;
    values.reserve(columns_size);

    while (parsed_value != proto_values.end()) {
      for (auto const& column : proto_schema.columns()) {
        auto value = FromProto(column.type(), *parsed_value);
        values.push_back(std::move(value));
        ++parsed_value;
      }
      buffered_rows_.push_back(
          QueryRowFriend::MakeQueryRow(std::move(values), columns_));
      values.clear();
    }
  }
  return {};
}
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google
