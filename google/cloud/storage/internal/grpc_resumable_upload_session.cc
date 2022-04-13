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

#include "google/cloud/storage/internal/grpc_resumable_upload_session.h"
#include "google/cloud/storage/internal/grpc_configure_client_context.h"
#include "google/cloud/storage/internal/grpc_object_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_object_request_parser.h"
#include "google/cloud/grpc_error_delegate.h"
#include "absl/memory/memory.h"
#include <crc32c/crc32c.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

static_assert(
    (google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES %
     UploadChunkRequest::kChunkSizeQuantum) == 0,
    "Expected maximum insert request size to be a multiple of chunk quantum");
static_assert(google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES >
                  UploadChunkRequest::kChunkSizeQuantum * 2,
              "Expected maximum insert request size to be greater than twice "
              "the chunk quantum");

GrpcResumableUploadSession::GrpcResumableUploadSession(
    std::shared_ptr<GrpcClient> client, ResumableUploadRequest request,
    ResumableUploadSessionGrpcParams session_id_params)
    : client_(std::move(client)),
      request_(std::move(request)),
      session_id_params_(std::move(session_id_params)),
      session_url_(EncodeGrpcResumableUploadSessionUrl(session_id_params_)) {}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::UploadChunk(
    ConstBufferSequence const& payload) {
  auto request = UploadChunkRequest(session_id_params_.upload_id,
                                    committed_size_, payload);
  request.set_multiple_options(
      request_.GetOption<UserProject>(), request_.GetOption<Fields>(),
      request_.GetOption<QuotaUser>(), request_.GetOption<UserIp>());
  return HandleResponse(client_->UploadChunk(request));
}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::UploadFinalChunk(
    ConstBufferSequence const& payload, std::uint64_t,
    HashValues const& full_object_hashes) {
  auto request = UploadChunkRequest(
      session_id_params_.upload_id, committed_size_, payload,
      committed_size_ + TotalBytes(payload), full_object_hashes);
  request.set_multiple_options(
      request_.GetOption<UserProject>(), request_.GetOption<Fields>(),
      request_.GetOption<QuotaUser>(), request_.GetOption<UserIp>());
  return HandleResponse(client_->UploadChunk(request));
}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::ResetSession() {
  QueryResumableUploadRequest request(session_id_params_.upload_id);
  auto result = client_->QueryResumableSession(request);
  if (!result) return result;

  committed_size_ = result->committed_size.value_or(0);
  return result;
}

std::string const& GrpcResumableUploadSession::session_id() const {
  return session_url_;
}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::HandleResponse(
    StatusOr<QueryResumableUploadResponse> response) {
  if (!response) return std::move(response).status();
  committed_size_ = response->committed_size.value_or(0);

  auto const upload_state = response->payload.has_value()
                                ? ResumableUploadResponse::kDone
                                : ResumableUploadResponse::kInProgress;
  return ResumableUploadResponse{
      /*.upload_session_url=*/session_url_,
      /*.upload_state=*/upload_state,
      /*.committed_size=*/std::move(response->committed_size),
      /*.payload=*/std::move(response->payload),
      /*.annotations=*/std::string{}};
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
