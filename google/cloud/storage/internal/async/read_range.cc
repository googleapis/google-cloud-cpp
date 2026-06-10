// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/async/read_range.h"
#include "google/cloud/storage/internal/async/read_payload_impl.h"
#include "google/cloud/storage/internal/grpc/ctype_cord_workaround.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/log.h"
#include "absl/strings/str_cat.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

bool ReadRange::IsDone() const {
  std::lock_guard<std::mutex> lk(mu_);
  return status_.has_value();
}

absl::optional<google::storage::v2::ReadRange> ReadRange::RangeForResume(
    std::int64_t read_id) const {
  auto range = google::storage::v2::ReadRange{};
  range.set_read_id(read_id);
  std::lock_guard<std::mutex> lk(mu_);
  if (status_.has_value()) return absl::nullopt;
  if (requested_length_.has_value() && *requested_length_ >= 0 &&
      received_bytes_ >= static_cast<std::size_t>(*requested_length_)) {
    return absl::nullopt;
  }
  range.set_read_offset(offset_);
  range.set_read_length(length_);
  return range;
}

future<ReadRange::ReadResponse> ReadRange::Read() {
  std::unique_lock<std::mutex> lk(mu_);
  if (!payload_ && !status_) {
    auto p = promise<ReadResponse>{};
    auto f = p.get_future();
    wait_.emplace(std::move(p));
    return f;
  }
  if (payload_) {
    auto p = std::move(*payload_);
    payload_.reset();
    return make_ready_future(ReadResponse(std::move(p)));
  }
  return make_ready_future(ReadResponse(*status_));
}

void ReadRange::OnFinish(Status status) {
  std::unique_lock<std::mutex> lk(mu_);
  if (status_) return;
  status_ = std::move(status);
  CheckOverrun();
  if (!wait_) return;
  auto p = std::move(*wait_);
  wait_.reset();
  lk.unlock();
  p.set_value(*status_);
}

void ReadRange::OnRead(google::storage::v2::ObjectRangeData data,
                       bool is_transcoded,
                       absl::optional<std::int64_t> object_size) {
  std::unique_lock<std::mutex> lk(mu_);
  is_transcoded_ = is_transcoded_ || is_transcoded;
  if (object_size.has_value()) object_size_ = object_size;
  if (status_) return;
  auto* check_summed_data = data.mutable_checksummed_data();
  auto content = StealMutableContent(*data.mutable_checksummed_data());
  auto status =
      hash_function_->Update(offset_, content, check_summed_data->crc32c());
  if (!status.ok()) {
    status_ = std::move(status);
    return Notify(std::move(lk), ReadPayloadImpl::Make(std::move(content)));
  }

  offset_ += content.size();
  received_bytes_ += content.size();
  if (length_ != 0) length_ -= std::min<std::int64_t>(content.size(), length_);
  auto p = ReadPayloadImpl::Make(std::move(content));

  if (data.range_end()) {
    status_ = Status{};
    CheckOverrun();
    auto result = std::move(*hash_validator_).Finish(hash_function_->Finish());
    bool transcoded_download =
        is_transcoded_ &&
        (!object_size_.has_value() ||
         (received_bytes_ != static_cast<std::size_t>(*object_size_)));
    if (result.is_mismatch && !transcoded_download) {
      status_ = google::cloud::internal::DataLossError(
          absl::StrCat("mismatched checksums detected at the end of the "
                       "download, received={",
                       FormatReceivedHashes(result), "}, computed={",
                       FormatComputedHashes(result), "}"),
          GCP_ERROR_INFO());
    }
  }
  if (wait_) {
    if (!payload_) return Notify(std::move(lk), std::move(p));
    GCP_LOG(FATAL) << "broken class invariant, `payload_` set when there is an"
                   << " active `wait_`.";
  }
  if (payload_) return ReadPayloadImpl::Append(*payload_, std::move(p));
  payload_ = std::move(p);
}

void ReadRange::CheckOverrun() {
  if (requested_length_.has_value() && *requested_length_ >= 0 &&
      received_bytes_ > static_cast<std::size_t>(*requested_length_) &&
      !is_transcoded_ && !logged_warning_) {
    logged_warning_ = true;
    GCP_LOG(WARNING) << "storage: received "
                     << (received_bytes_ - *requested_length_)
                     << " more bytes than requested from GCS for bucket \""
                     << bucket_name_ << "\", object \"" << object_name_ << "\"";
  }
}

void ReadRange::Notify(std::unique_lock<std::mutex> lk,
                       storage::ReadPayload p) {
  auto wait = std::move(*wait_);
  wait_.reset();
  payload_.reset();
  lk.unlock();
  wait.set_value(std::move(p));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google
