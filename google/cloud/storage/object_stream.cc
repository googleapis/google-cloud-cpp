// Copyright 2018 Google LLC
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

#include "google/cloud/storage/object_stream.h"
#include <sstream>
#include <thread>

namespace {
bool IsRetryableStatusCode(long status_code) {
  return status_code == 429 or status_code >= 500;
}
}  // namespace

namespace storage {
inline namespace STORAGE_CLIENT_NS {
static_assert(std::is_move_assignable<ObjectReadStream>::value,
              "storage::ObjectReadStream must be move assignable.");
static_assert(std::is_move_constructible<ObjectReadStream>::value,
              "storage::ObjectReadStream must be move constructible.");

std::streamsize ObjectReadStreamBuf::showmanyc() {
  // TODO(#554) - use the returned headers to have a better estimate.
  return 0;
}

ObjectReadStreamBuf::int_type ObjectReadStreamBuf::underflow() {
  // TODO(#554) - for now, just read the first 64KiB
  // TODO(#742) - use literals for KiB and MiB and GiB.
  std::int64_t const max_read_bytes = 64 * 1024;
  if (request_.end() >= max_read_bytes) {
    return traits_type::eof();
  }
  request_.set_begin(0).set_end(max_read_bytes);

  // TODO(#555) - use policies to implement retry loop.
  Status last_status;
  constexpr int MAX_NUM_RETRIES = 3;
  for (int i = 0; i != MAX_NUM_RETRIES; ++i) {
    auto result = client_->ReadObjectRangeMedia(request_);
    last_status = std::move(result.first);
    if (last_status.ok()) {
      buffer_ = std::move(result.second);
      return RepositionInputSequence();
    }
    // TODO(#714) - use policies to decide if the operation is idempotent.
    // TODO(#581) - use policies to determine what error codes are permanent.
    if (not IsRetryableStatusCode(last_status.status_code())) {
      std::ostringstream os;
      os << "Permanent error in " << __func__ << ": " << last_status;
      google::cloud::internal::RaiseRuntimeError(os.str());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  std::ostringstream os;
  os << "Retry policy exhausted in " << __func__ << ": " << last_status;
  google::cloud::internal::RaiseRuntimeError(os.str());
}

ObjectReadStreamBuf::int_type ObjectReadStreamBuf::RepositionInputSequence() {
  if (buffer_.empty()) {
    setg(&sentinel_, &sentinel_ + 1, &sentinel_ + 1);
    return traits_type::eof();
  }
  char* data = &buffer_[0];
  setg(data, data, data + buffer_.size());
  return traits_type::to_int_type(*data);
}
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
