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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_RESUMABLE_UPLOAD_SESSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_RESUMABLE_UPLOAD_SESSION_H

#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/version.h"
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * A decorator for `ResumableUploadSession` that logs each operation.
 */
class LoggingResumableUploadSession : public ResumableUploadSession {
 public:
  explicit LoggingResumableUploadSession(
      std::unique_ptr<ResumableUploadSession> session)
      : session_(std::move(session)) {}

  StatusOr<ResumableUploadResponse> UploadChunk(
      ConstBufferSequence const& buffers) override;
  StatusOr<ResumableUploadResponse> UploadFinalChunk(
      ConstBufferSequence const& buffers, std::uint64_t upload_size,
      HashValues const& full_object_hashes) override;
  StatusOr<ResumableUploadResponse> ResetSession() override;
  std::string const& session_id() const override;

 private:
  std::unique_ptr<ResumableUploadSession> session_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_RESUMABLE_UPLOAD_SESSION_H
