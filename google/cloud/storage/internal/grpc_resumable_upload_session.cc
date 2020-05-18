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

#include "google/cloud/storage/internal/grpc_resumable_upload_session.h"
#include "google/cloud/grpc_error_delegate.h"
#include "absl/memory/memory.h"
#include <crc32c/crc32c.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::UploadChunk(
    std::string const& buffer) {
  CreateUploadWriter();

  bool success = true;
  // This loop must run at least once because we need to send at least one
  // Write() call for empty objects.
  std::size_t offset = 0;
  do {
    // This limit is for the *message*, not just the payload. It includes any
    // additional information such as checksums. We need to use a stricter
    // limit, a chunk quantum seems to work in practice.
    std::size_t const maximum_buffer_size =
        GrpcClient::kMaxInsertObjectWriteRequestSize -
        UploadChunkRequest::kChunkSizeQuantum;

    google::storage::v1::InsertObjectRequest request;
    request.set_upload_id(session_id_);
    request.set_write_offset(next_expected_);
    request.set_finish_write(false);

    auto& data = *request.mutable_checksummed_data();
    auto const n = (std::min)(buffer.size() - offset, maximum_buffer_size);
    data.set_content(buffer.substr(offset, n));
    data.mutable_crc32c()->set_value(crc32c::Crc32c(data.content()));

    success = upload_writer_->Write(request);

    if (!success) break;
    // After the first message, clear the object specification and checksums,
    // there is no need to resend it.
    request.clear_insert_object_spec();
    request.clear_object_checksums();
    offset += n;

    auto result = ResumableUploadResponse{{},
                                          next_expected_ + n - 1,
                                          {},
                                          ResumableUploadResponse::kInProgress,
                                          {}};
    Update(result);
  } while (success && offset < buffer.size());
  if (success) {
    return ResumableUploadResponse{
        {}, next_expected_ - 1, {}, ResumableUploadResponse::kInProgress, {}};
  }
  auto grpc_status = upload_writer_->Finish();
  upload_writer_ = nullptr;
  upload_context_ = nullptr;
  if (!grpc_status.ok()) {
    return google::cloud::MakeStatusFromRpcError(grpc_status);
  }
  return Status(StatusCode::kUnavailable,
                "GrpcResumableUploadSession: stream closed unexpectedly");
}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::UploadFinalChunk(
    std::string const& buffer, std::uint64_t) {
  std::size_t const maximum_buffer_size =
      GrpcClient::kMaxInsertObjectWriteRequestSize -
      UploadChunkRequest::kChunkSizeQuantum;
  static_assert(
      GrpcClient::kMaxInsertObjectWriteRequestSize %
              UploadChunkRequest::kChunkSizeQuantum ==
          0,
      "Expected maximum insert request size to be a multiple of chunk quantum");
  static_assert(GrpcClient::kMaxInsertObjectWriteRequestSize >
                    UploadChunkRequest::kChunkSizeQuantum * 2,
                "Expected maximum insert request size to be greater than twice "
                "the chunk quantum");

  std::string trailer;
  if (buffer.size() < maximum_buffer_size) {
    trailer = buffer;
  } else {
    auto const last_full_chunk_pos = [&buffer] {
      if (buffer.size() % UploadChunkRequest::kChunkSizeQuantum == 0) {
        return buffer.size() - UploadChunkRequest::kChunkSizeQuantum;
      }
      return (buffer.size() / UploadChunkRequest::kChunkSizeQuantum) *
             UploadChunkRequest::kChunkSizeQuantum;
    }();
    auto initial = UploadChunk(buffer.substr(0, last_full_chunk_pos));
    if (!initial) return initial;
    trailer = buffer.substr(last_full_chunk_pos);
  }

  CreateUploadWriter();
  google::storage::v1::InsertObjectRequest request;
  request.set_upload_id(session_id_);
  request.set_write_offset(next_expected_);
  request.set_finish_write(true);
  request.mutable_checksummed_data()->set_content(trailer);
  request.mutable_checksummed_data()->mutable_crc32c()->set_value(
      crc32c::Crc32c(trailer));
  // TODO(#4156) - compute the crc32c value inline and set it here
  // TODO(#4157) - compute the MD5 hash value and set it here
  auto success =
      upload_writer_->Write(request, grpc::WriteOptions().set_last_message());
  if (!success) {
    return Status(
        StatusCode::kUnavailable,
        "GrpcResumableUploadSession: error writing last chunk to stream");
  }
  auto status = upload_writer_->Finish();
  upload_writer_ = nullptr;
  upload_context_ = nullptr;
  if (!status.ok()) return google::cloud::MakeStatusFromRpcError(status);

  ResumableUploadResponse result{{},
                                 next_expected_ + buffer.size() - 1,
                                 GrpcClient::FromProto(upload_object_),
                                 ResumableUploadResponse::kDone,
                                 {}};
  Update(result);
  return result;
}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::ResetSession() {
  QueryResumableUploadRequest request(session_id_);
  auto result = client_->QueryResumableUpload(request);
  Update(result);
  return result;
}

std::uint64_t GrpcResumableUploadSession::next_expected_byte() const {
  return next_expected_;
}

std::string const& GrpcResumableUploadSession::session_id() const {
  return session_id_;
}

bool GrpcResumableUploadSession::done() const { return done_; }

StatusOr<ResumableUploadResponse> const&
GrpcResumableUploadSession::last_response() const {
  return last_response_;
}

void GrpcResumableUploadSession::CreateUploadWriter() {
  if (upload_writer_) {
    return;
  }
  // TODO(#4216) - set the timeout
  upload_context_ = absl::make_unique<grpc::ClientContext>();
  google::storage::v1::InsertObjectRequest request;
  request.set_upload_id(session_id_);
  upload_writer_ =
      client_->CreateUploadWriter(*upload_context_, upload_object_);
}

void GrpcResumableUploadSession::Update(
    StatusOr<ResumableUploadResponse> const& result) {
  last_response_ = result;
  if (!result.ok()) {
    return;
  }
  done_ = result->upload_state == ResumableUploadResponse::kDone;
  if (result->last_committed_byte == 0) {
    next_expected_ = 0;
  } else {
    next_expected_ = result->last_committed_byte + 1;
  }
  if (!result->upload_session_url.empty()) {
    session_id_ = result->upload_session_url;
  }
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
