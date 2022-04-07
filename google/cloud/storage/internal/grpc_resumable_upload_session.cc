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
  return UploadGeneric(payload, false, {});
}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::UploadFinalChunk(
    ConstBufferSequence const& payload, std::uint64_t,
    HashValues const& full_object_hashes) {
  return UploadGeneric(payload, true, full_object_hashes);
}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::ResetSession() {
  QueryResumableUploadRequest request(session_id_params_.upload_id);
  auto result = client_->QueryResumableUpload(request);
  if (!result) return result;

  committed_size_ = result->committed_size.value_or(0);
  return result;
}

std::string const& GrpcResumableUploadSession::session_id() const {
  return session_url_;
}

StatusOr<ResumableUploadResponse> GrpcResumableUploadSession::UploadGeneric(
    ConstBufferSequence buffers, bool final_chunk, HashValues const& hashes) {
  auto context = absl::make_unique<grpc::ClientContext>();
  ApplyQueryParameters(*context, request_, "resource");
  auto writer = client_->CreateUploadWriter(std::move(context));

  std::size_t const maximum_chunk_size =
      google::storage::v2::ServiceConstants::MAX_WRITE_CHUNK_BYTES;
  std::string chunk;
  chunk.reserve(maximum_chunk_size);
  auto flush_chunk = [&](bool has_more) {
    if (chunk.size() < maximum_chunk_size && has_more) return true;
    if (chunk.empty() && !final_chunk) return true;

    google::storage::v2::WriteObjectRequest request;
    request.set_upload_id(session_id_params_.upload_id);
    request.set_write_offset(
        static_cast<google::protobuf::int64>(committed_size_));
    request.set_finish_write(false);

    auto& data = *request.mutable_checksummed_data();
    auto const n = chunk.size();
    data.set_content(std::move(chunk));
    chunk.clear();
    chunk.reserve(maximum_chunk_size);
    data.set_crc32c(crc32c::Crc32c(data.content()));

    auto options = grpc::WriteOptions();
    if (final_chunk && !has_more) {
      if (!hashes.md5.empty()) {
        auto md5 = GrpcObjectMetadataParser::MD5ToProto(hashes.md5);
        if (md5) {
          request.mutable_object_checksums()->set_md5_hash(*std::move(md5));
        }
      }
      if (!hashes.crc32c.empty()) {
        auto crc32c = GrpcObjectMetadataParser::Crc32cToProto(hashes.crc32c);
        if (crc32c) {
          request.mutable_object_checksums()->set_crc32c(*std::move(crc32c));
        }
      }
      request.set_finish_write(true);
      options.set_last_message();
    }

    if (!writer->Write(request, options)) return false;
    // After the first message, clear the object specification and checksums,
    // there is no need to resend it.
    request.clear_write_object_spec();
    request.clear_object_checksums();

    committed_size_ += n;
    return true;
  };

  auto close_writer = [&]() -> StatusOr<ResumableUploadResponse> {
    auto result = writer->Close();
    writer.reset();
    if (!result) return std::move(result).status();
    return GrpcObjectRequestParser::FromProto(*std::move(result),
                                              client_->options());
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

    if (!flush_chunk(!buffers.empty())) return close_writer();
  } while (!buffers.empty());

  return close_writer();
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
