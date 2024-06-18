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
#include "google/cloud/log.h"

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
  range.set_read_offset(offset_);
  range.set_read_limit(limit_);
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
  if (!wait_) return;
  auto p = std::move(*wait_);
  wait_.reset();
  lk.unlock();
  p.set_value(*status_);
}

void ReadRange::OnRead(google::storage::v2::ObjectRangeData data) {
  std::unique_lock<std::mutex> lk(mu_);
  if (status_) return;
  if (data.range_end()) status_ = Status{};
  // TODO(#28) - verify the checksum
  auto content = StealMutableContent(*data.mutable_checksummed_data());
  offset_ += content.size();
  if (limit_ != 0) limit_ -= std::min<std::int64_t>(content.size(), limit_);
  auto p = ReadPayloadImpl::Make(std::move(content));
  if (wait_) {
    if (!payload_) return Notify(std::move(lk), std::move(p));
    GCP_LOG(FATAL) << "broken class invariant, `payload_` set when there is an"
                   << " active `wait_`.";
  }
  if (payload_) return ReadPayloadImpl::Append(*payload_, std::move(p));
  payload_ = std::move(p);
}

void ReadRange::Notify(std::unique_lock<std::mutex> lk,
                       storage_experimental::ReadPayload p) {
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
