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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_RESUMABLE_UPLOAD_SESSION_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_RESUMABLE_UPLOAD_SESSION_H_

#include "google/cloud/storage/internal/resumable_upload_session.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * A decorator for `ResumableUploadSession` that logs each operation.
 */
class LoggingResumableUploadSession : public ResumableUploadSession {
 public:
  explicit LoggingResumableUploadSession(
      std::unique_ptr<ResumableUploadSession> session)
      : session_(std::move(session)) {}

  std::pair<Status, ResumableUploadResponse> UploadChunk(
      std::string const& buffer, std::uint64_t upload_size) override;
  std::pair<Status, ResumableUploadResponse> ResetSession() override;
  std::uint64_t next_expected_byte() const override;

 private:
  std::unique_ptr<ResumableUploadSession> session_;
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_LOGGING_RESUMABLE_UPLOAD_SESSION_H_
