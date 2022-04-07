// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/logging_resumable_upload_session.h"
#include "google/cloud/log.h"
#include <ios>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

StatusOr<ResumableUploadResponse> LoggingResumableUploadSession::UploadChunk(
    ConstBufferSequence const& buffers) {
  GCP_LOG(INFO) << __func__ << "() << {buffer.size=" << TotalBytes(buffers)
                << "}";
  auto response = session_->UploadChunk(buffers);
  if (response.ok()) {
    GCP_LOG(INFO) << __func__ << "() >> payload={" << response.value() << "}";
  } else {
    GCP_LOG(INFO) << __func__ << "() >> status={" << response.status() << "}";
  }
  return response;
}

StatusOr<ResumableUploadResponse>
LoggingResumableUploadSession::UploadFinalChunk(
    ConstBufferSequence const& buffers, std::uint64_t upload_size,
    HashValues const& full_object_hashes) {
  GCP_LOG(INFO) << __func__ << "() << upload_size=" << upload_size
                << ", buffer.size=" << TotalBytes(buffers) << ", crc32c=<"
                << full_object_hashes.crc32c << ">, md5=<"
                << full_object_hashes.md5 << ">";
  auto response =
      session_->UploadFinalChunk(buffers, upload_size, full_object_hashes);
  if (response.ok()) {
    GCP_LOG(INFO) << __func__ << "() >> payload={" << response.value() << "}";
  } else {
    GCP_LOG(INFO) << __func__ << "() >> status={" << response.status() << "}";
  }
  return response;
}

StatusOr<ResumableUploadResponse>
LoggingResumableUploadSession::ResetSession() {
  GCP_LOG(INFO) << __func__ << "() << {}";
  auto response = session_->ResetSession();
  if (response.ok()) {
    GCP_LOG(INFO) << __func__ << "() >> payload={" << response.value() << "}";
  } else {
    GCP_LOG(INFO) << __func__ << "() >> status={" << response.status() << "}";
  }
  return response;
}

std::string const& LoggingResumableUploadSession::session_id() const {
  GCP_LOG(INFO) << __func__ << "() << {}";
  auto const& response = session_->session_id();
  GCP_LOG(INFO) << __func__ << "() >> " << response;
  return response;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
