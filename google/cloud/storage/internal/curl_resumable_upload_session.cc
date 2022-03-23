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

#include "google/cloud/storage/internal/curl_resumable_upload_session.h"

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

StatusOr<ResumableUploadResponse> CurlResumableUploadSession::UploadChunk(
    ConstBufferSequence const& buffers) {
  UploadChunkRequest request(session_id_, next_expected_, buffers);
  request.set_multiple_options(
      request_.GetOption<CustomHeader>(), request_.GetOption<Fields>(),
      request_.GetOption<IfMatchEtag>(), request_.GetOption<IfNoneMatchEtag>(),
      request_.GetOption<QuotaUser>(), request_.GetOption<UserIp>());
  auto result = client_->UploadChunk(request);
  Update(result, TotalBytes(buffers));
  return result;
}

StatusOr<ResumableUploadResponse> CurlResumableUploadSession::UploadFinalChunk(
    ConstBufferSequence const& buffers, std::uint64_t upload_size,
    HashValues const& /*full_object_hashes*/) {
  UploadChunkRequest request(session_id_, next_expected_, buffers, upload_size);
  request.set_multiple_options(
      request_.GetOption<CustomHeader>(), request_.GetOption<Fields>(),
      request_.GetOption<IfMatchEtag>(), request_.GetOption<IfNoneMatchEtag>(),
      request_.GetOption<QuotaUser>(), request_.GetOption<UserIp>());
  auto result = client_->UploadChunk(request);
  Update(result, TotalBytes(buffers));
  return result;
}

StatusOr<ResumableUploadResponse> CurlResumableUploadSession::ResetSession() {
  QueryResumableUploadRequest request(session_id_);
  request.set_multiple_options(
      request_.GetOption<CustomHeader>(), request_.GetOption<Fields>(),
      request_.GetOption<IfMatchEtag>(), request_.GetOption<IfNoneMatchEtag>(),
      request_.GetOption<QuotaUser>(), request_.GetOption<UserIp>());
  auto result = client_->QueryResumableUpload(request);
  Update(result, 0);
  return result;
}

void CurlResumableUploadSession::Update(
    StatusOr<ResumableUploadResponse> const& result, std::size_t chunk_size) {
  if (!result.ok()) {
    return;
  }
  if (result->upload_state == ResumableUploadResponse::kDone) {
    // Sometimes (e.g. when the user sets the X-Upload-Content-Length header)
    // the upload completes but does *not* include a `last_committed_byte`
    // value. In this case we update the next expected byte using the chunk
    // size, as we know the upload was successful.
    next_expected_ += chunk_size;
  } else {
    // Nothing has been committed on the server side yet, keep resending.
    next_expected_ = result->committed_size.value_or(0);
  }
  if (session_id_.empty() && !result->upload_session_url.empty()) {
    session_id_ = result->upload_session_url;
  }
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
