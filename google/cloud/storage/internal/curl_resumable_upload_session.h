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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_RESUMABLE_UPLOAD_SESSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_RESUMABLE_UPLOAD_SESSION_H

#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * Implement a ResumableUploadSession that delegates to a CurlClient.
 */
class CurlResumableUploadSession : public ResumableUploadSession {
 public:
  explicit CurlResumableUploadSession(std::shared_ptr<CurlClient> client,
                                      ResumableUploadRequest request,
                                      std::string session_id)
      : client_(std::move(client)),
        request_(std::move(request)),
        session_id_(std::move(session_id)) {}

  StatusOr<ResumableUploadResponse> UploadChunk(
      ConstBufferSequence const& buffers) override;

  StatusOr<ResumableUploadResponse> UploadFinalChunk(
      ConstBufferSequence const& buffers, std::uint64_t upload_size,
      HashValues const& full_object_hashes) override;

  StatusOr<ResumableUploadResponse> ResetSession() override;

  std::uint64_t next_expected_byte() const override;

  std::string const& session_id() const override { return session_id_; }

  bool done() const override { return done_; }

  StatusOr<ResumableUploadResponse> const& last_response() const override {
    return last_response_;
  }

 private:
  void Update(StatusOr<ResumableUploadResponse> const& result,
              std::size_t chunk_size);

  std::shared_ptr<CurlClient> client_;
  ResumableUploadRequest request_;
  std::string session_id_;
  std::uint64_t next_expected_ = 0;
  bool done_ = false;
  StatusOr<ResumableUploadResponse> last_response_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_CURL_RESUMABLE_UPLOAD_SESSION_H
