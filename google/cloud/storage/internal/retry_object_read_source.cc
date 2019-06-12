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

#include "google/cloud/storage/internal/retry_object_read_source.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
RetryObjectReadSource::RetryObjectReadSource(
    std::shared_ptr<RetryClient> client, ReadObjectRangeRequest request,
    std::unique_ptr<ObjectReadSource> child)
    : client_(std::move(client)),
      request_(std::move(request)),
      child_(std::move(child)),
      current_offset_(request_.StartingByte()) {}

StatusOr<ReadSourceResult> RetryObjectReadSource::Read(char* buf,
                                                       std::size_t n) {
  GCP_LOG(INFO) << __func__ << "() current_offset=" << current_offset_;
  if (!child_) {
    return Status(StatusCode::kFailedPrecondition, "Stream is not open");
  }
  auto result = child_->Read(buf, n);
  if (!result) {
    // A Read() request failed, most likely that means the connection closed,
    // try to create a new child. The current child is no longer usable, we will
    // try to create a new one and replace it. Should that fail the current
    // child would be erased anyway so we reset it here.
    child_.reset();
    request_.set_option(ReadFromOffset(current_offset_));
    if (generation_) {
      request_.set_option(Generation(*generation_));
    }
    auto new_child = client_->ReadObjectNotWrapped(request_);
    if (!new_child) {
      // Failing to create a new child is an unrecoverable error: the
      // ReadObjectNotWrapped() function already retried multiple times.
      return new_child.status();
    }
    // Repeat the Read() request on the new child.
    result = (*new_child)->Read(buf, n);
    if (!result) {
      // This is a permanent failure, we created a new child but it turned out
      // to be unusable.
      return result;
    }
    child_ = *std::move(new_child);
  }
  // assert(response.ok());
  auto g = result->response.headers.find("x-goog-generation");
  if (g != result->response.headers.end()) {
    generation_ = std::stoll(g->second);
  }
  current_offset_ += result->bytes_received;
  return result;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
