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

StatusOr<HttpResponse> RetryObjectReadSource::Read(std::string& buffer) {
  GCP_LOG(INFO) << __func__ << "() current_offset=" << current_offset_
                << " child=" << static_cast<bool>(child_);
  if (!child_) {
    return Status(StatusCode::kFailedPrecondition, "Stream is not open");
  }
  auto response = child_->Read(buffer);
  if (!response) {
    GCP_LOG(DEBUG) << __func__ << "() current_offset=" << current_offset_
                   << " child=" << static_cast<bool>(child_)
                   << " status=" << response.status()
                   << " size=" << buffer.size();
    // A Read() request failed, most likely that means the connection closed,
    // try to create a new child.
    request_.set_option(ReadFromOffset(current_offset_));
    if (generation_) {
      request_.set_option(Generation(*generation_));
    }
    auto new_child = client_->ReadObjectNotWrapped(request_);
    GCP_LOG(DEBUG) << __func__ << "() current_offset=" << current_offset_
                   << " child=" << static_cast<bool>(child_)
                   << " status=" << response.status()
                   << " new_child=" << new_child.status();
    if (!new_child) {
      // Failing to create a new child is an unrecoverable error: the
      // ReadObjectNotWrapped() function already retried multiple times.
      child_.reset();
      return new_child.status();
    }
    child_ = *std::move(new_child);
    response = child_->Read(buffer);
    GCP_LOG(DEBUG) << __func__ << "() current_offset=" << current_offset_
                   << "child=" << static_cast<bool>(child_)
                   << " status=" << response.status()
                   << " size=" << buffer.size();
    if (!response) {
      child_.reset();
      return response;
    }
  }
  GCP_LOG(DEBUG) << __func__ << "() current_offset=" << current_offset_
                 << " child=" << static_cast<bool>(child_)
                 << " status=" << response.status()
                 << " size=" << buffer.size();
  // assert(response.ok());
  auto g = response->headers.find("x-goog-generation");
  if (g != response->headers.end()) {
    generation_ = std::stoll(g->second);
  }
  current_offset_ += buffer.size();
  return response;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
