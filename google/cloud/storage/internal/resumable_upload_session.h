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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RESUMABLE_UPLOAD_SESSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RESUMABLE_UPLOAD_SESSION_H

#include "google/cloud/storage/internal/const_buffer.h"
#include "google/cloud/storage/internal/hash_values.h"
#include "google/cloud/storage/internal/http_response.h"
#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"
#include "absl/types/optional.h"
#include <cstdint>
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
struct ResumableUploadResponse;

/**
 * Defines the interface for a resumable upload session.
 */
class ResumableUploadSession {
 public:
  virtual ~ResumableUploadSession() = default;

  /**
   * Uploads a chunk and returns the resulting response.
   *
   * @param buffer the chunk to upload.
   * @return The result of uploading the chunk.
   */
  virtual StatusOr<ResumableUploadResponse> UploadChunk(
      ConstBufferSequence const& buffers) = 0;

  /**
   * Uploads the final chunk in a stream, committing all previous data.
   *
   * @param buffer the chunk to upload.
   * @param upload_size the total size of the upload, use `0` if the size is not
   *   known.
   * @return The final result of the upload, including the object metadata.
   */
  virtual StatusOr<ResumableUploadResponse> UploadFinalChunk(
      ConstBufferSequence const& buffers, std::uint64_t upload_size,
      HashValues const& full_object_hashes) = 0;

  /// Resets the session by querying its current state.
  virtual StatusOr<ResumableUploadResponse> ResetSession() = 0;

  /**
   * Returns the current upload session id.
   *
   * Note that the session id might change during an upload.
   */
  virtual std::string const& session_id() const = 0;
};

struct ResumableUploadResponse {
  enum UploadState { kInProgress, kDone };

  static StatusOr<ResumableUploadResponse> FromHttpResponse(
      HttpResponse response);

  std::string upload_session_url;
  UploadState upload_state;
  absl::optional<std::uint64_t> committed_size;
  absl::optional<google::cloud::storage::ObjectMetadata> payload;
  std::string annotations;
};

bool operator==(ResumableUploadResponse const& lhs,
                ResumableUploadResponse const& rhs);
bool operator!=(ResumableUploadResponse const& lhs,
                ResumableUploadResponse const& rhs);

std::ostream& operator<<(std::ostream& os, ResumableUploadResponse const& r);

/**
 * The return type for functions creating or restoring resumable upload
 * sessions.
 *
 * Restoring a session produces both a new object to manage the session and the
 * initial result of querying the session state.
 *
 * TODO(#8559) - this will become simply the resumable upload response as the
 *   code evolves towards session-less resumable uploads.
 */
struct CreateResumableSessionResponse {
  std::unique_ptr<ResumableUploadSession> session;
  ResumableUploadResponse state;
};

/**
 * A resumable upload session that always returns an error.
 *
 * When an unrecoverable error is detected (or the policies to recover from an
 * error are exhausted), we create an object of this type to represent a session
 * that will never succeed. This is cleaner than returning a nullptr and then
 * checking for null in each call.
 */
class ResumableUploadSessionError : public ResumableUploadSession {
 public:
  explicit ResumableUploadSessionError(Status status)
      : error_(std::move(status)) {}

  ResumableUploadSessionError(Status status, std::string id)
      : error_(std::move(status)), id_(std::move(id)) {}

  ~ResumableUploadSessionError() override = default;

  StatusOr<ResumableUploadResponse> UploadChunk(
      ConstBufferSequence const&) override {
    return error_;
  }

  StatusOr<ResumableUploadResponse> UploadFinalChunk(
      ConstBufferSequence const&, std::uint64_t, HashValues const&) override {
    return error_;
  }

  StatusOr<ResumableUploadResponse> ResetSession() override { return error_; }

  std::string const& session_id() const override { return id_; }

 private:
  Status error_;
  std::string id_;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_RESUMABLE_UPLOAD_SESSION_H
