// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_RESUMABLE_UPLOAD_SESSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_RESUMABLE_UPLOAD_SESSION_H

#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/internal/grpc_resumable_upload_session_url.h"
#include "google/cloud/storage/internal/resumable_upload_session.h"
#include "google/cloud/storage/version.h"
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/// Implements the ResumableUploadSession interface for a gRPC client.
class GrpcResumableUploadSession : public ResumableUploadSession {
 public:
  explicit GrpcResumableUploadSession(
      std::shared_ptr<GrpcClient> client, ResumableUploadRequest request,
      ResumableUploadSessionGrpcParams session_id_params);

  StatusOr<ResumableUploadResponse> UploadChunk(
      ConstBufferSequence const& payload) override;

  StatusOr<ResumableUploadResponse> UploadFinalChunk(
      ConstBufferSequence const& payload, std::uint64_t upload_size,
      HashValues const& full_object_hashes) override;

  StatusOr<ResumableUploadResponse> ResetSession() override;

  std::string const& session_id() const override;

 private:
  /**
   * Uploads a multiple of the upload quantum bytes from @p buffers
   *
   * This function is used by both UploadChunk() and UploadFinalChunk()
   */
  StatusOr<ResumableUploadResponse> UploadGeneric(ConstBufferSequence buffers,
                                                  bool final_chunk,
                                                  HashValues const& hashes);

  std::shared_ptr<GrpcClient> client_;
  ResumableUploadRequest request_;
  ResumableUploadSessionGrpcParams session_id_params_;
  std::string session_url_;

  std::uint64_t committed_size_ = 0;
};

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_GRPC_RESUMABLE_UPLOAD_SESSION_H
