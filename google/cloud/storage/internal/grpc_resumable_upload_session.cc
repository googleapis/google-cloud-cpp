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

static_assert(
    (google::storage::v1::ServiceConstants::MAX_WRITE_CHUNK_BYTES %
     UploadChunkRequest::kChunkSizeQuantum) == 0,
    "Expected maximum insert request size to be a multiple of chunk quantum");
static_assert(google::storage::v1::ServiceConstants::MAX_WRITE_CHUNK_BYTES >
                  UploadChunkRequest::kChunkSizeQuantum * 2,
              "Expected maximum insert request size to be greater than twice "
              "the chunk quantum");

GrpcResumableUploadSession::GrpcResumableUploadSession(
    std::shared_ptr<GrpcClient> client,
    ResumableUploadSessionGrpcParams session_id_params)
    : client_(std::move(client)),
      session_id_params_(std::move(session_id_params)),
      session_url_(EncodeGrpcResumableUploadSessionUrl(session_id_params_)) {}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::UploadChunk(
    ConstBufferSequence const& payload) {
  return UploadGeneric(payload, false);
}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::UploadFinalChunk(
    ConstBufferSequence const& payload, std::uint64_t) {
  auto initial = UploadGeneric(payload, true);
  if (!initial) return initial;

  return HandleStreamClosed();
}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::ResetSession() {
  QueryResumableUploadRequest request(session_id_params_.upload_id);
  auto result = client_->QueryResumableUpload(request);
  last_response_ = std::move(result);
  if (!last_response_) return last_response_;

  done_ = (last_response_->upload_state == ResumableUploadResponse::kDone);
  if (last_response_->last_committed_byte == 0) {
    next_expected_ = 0;
  } else {
    next_expected_ = last_response_->last_committed_byte + 1;
  }
  return last_response_;
}

std::uint64_t GrpcResumableUploadSession::next_expected_byte() const {
  return next_expected_;
}

std::string const& GrpcResumableUploadSession::session_id() const {
  return session_url_;
}

bool GrpcResumableUploadSession::done() const { return done_; }

StatusOr<ResumableUploadResponse> const&
GrpcResumableUploadSession::last_response() const {
  return last_response_;
}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::UploadGeneric(
    ConstBufferSequence buffers, bool final_chunk) {
  CreateUploadWriter();

  std::size_t const maximum_chunk_size =
      google::storage::v1::ServiceConstants::MAX_WRITE_CHUNK_BYTES;
  std::string chunk;
  chunk.reserve(maximum_chunk_size);
  auto flush_chunk = [&](bool has_more) {
    if (chunk.size() < maximum_chunk_size && has_more) return true;
    if (chunk.empty() && !final_chunk) return true;

    google::storage::v1::InsertObjectRequest request;
    request.set_upload_id(session_id_params_.upload_id);
    request.set_write_offset(
        static_cast<google::protobuf::int64>(next_expected_));
    request.set_finish_write(false);

    auto& data = *request.mutable_checksummed_data();
    auto const n = chunk.size();
    data.set_content(std::move(chunk));
    chunk.clear();
    chunk.reserve(maximum_chunk_size);
    data.mutable_crc32c()->set_value(crc32c::Crc32c(data.content()));

    auto options = grpc::WriteOptions();
    if (final_chunk && !has_more) {
      // At this point we can set the full object checksums, there are two bugs
      // for this:
      // TODO(#4156) - compute the crc32c value inline
      // TODO(#4157) - compute the MD5 hash value inline
      request.set_finish_write(true);
      options.set_last_message();
    }

    if (!upload_writer_->Write(request, options)) return false;
    // After the first message, clear the object specification and checksums,
    // there is no need to resend it.
    request.clear_insert_object_spec();
    request.clear_object_checksums();

    next_expected_ += n;
    return true;
  };

  do {
    std::size_t consumed = 0;
    for (auto const& b : buffers) {
      // flush_chunk() guarantees that maximum_chunk_size < chunk.size()
      auto capacity = maximum_chunk_size - chunk.size();
      if (capacity == 0) break;
      auto n = (std::min)(capacity, b.size());
      chunk.append(b.data(), b.data() + n);
      consumed += n;
    }
    PopFrontBytes(buffers, consumed);

    if (!flush_chunk(!buffers.empty())) return HandleStreamClosed();
  } while (!buffers.empty());

  return ResumableUploadResponse{
      {}, next_expected_ - 1, {}, ResumableUploadResponse::kInProgress, {}};
}

void GrpcResumableUploadSession::CreateUploadWriter() {
  if (upload_writer_) return;
  // TODO(#4216) - set the timeout
  auto context = absl::make_unique<grpc::ClientContext>();
  upload_writer_ = client_->CreateUploadWriter(std::move(context));
}

StatusOr<ResumableUploadResponse>
GrpcResumableUploadSession::HandleStreamClosed() {
  auto result = upload_writer_->Close();
  upload_writer_ = nullptr;
  if (!result) {
    last_response_ = std::move(result).status();
    return last_response_;
  }
  done_ = true;
  last_response_ =
      ResumableUploadResponse{{},
                              next_expected_ - 1,
                              GrpcClient::FromProto(*std::move(result)),
                              ResumableUploadResponse::kDone,
                              {}};
  return last_response_;
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
