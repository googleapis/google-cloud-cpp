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

#include "google/cloud/storage/internal/grpc_object_read_source.h"
#include "google/cloud/storage/internal/grpc_object_metadata_parser.h"
#include "absl/strings/string_view.h"
#include <algorithm>
#include <limits>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

GrpcObjectReadSource::GrpcObjectReadSource(TimerSource timer_source,
                                           std::unique_ptr<StreamingRpc> stream)
    : timer_source_(std::move(timer_source)), stream_(std::move(stream)) {}

StatusOr<HttpResponse> GrpcObjectReadSource::Close() {
  if (stream_) stream_ = nullptr;
  if (!status_.ok()) return status_;
  return HttpResponse{HttpStatusCode::kOk, {}, {}};
}

/// Read more data from the download, returning any HTTP headers and error
/// codes.
StatusOr<ReadSourceResult> GrpcObjectReadSource::Read(char* buf,
                                                      std::size_t n) {
  using google::storage::v2::ReadObjectResponse;

  std::size_t offset = 0;
  auto buffer_manager = [&offset, buf, n](absl::string_view source) {
    if (source.empty()) return std::make_pair(source, offset);
    auto const nbytes = std::min(n - offset, source.size());
    auto const* end = source.data() + nbytes;
    std::copy(source.data(), end, buf + offset);
    offset += nbytes;
    return std::make_pair(absl::string_view(end, source.size() - nbytes),
                          offset);
  };

  ReadSourceResult result;
  result.response.status_code = HttpStatusCode::kContinue;
  std::tie(spill_view_, result.bytes_received) = buffer_manager(spill_view_);

  while (result.bytes_received < n && stream_) {
    auto watchdog = timer_source_().then([this](auto f) {
      if (!f.get()) return false;  // timer cancelled, no action needed
      stream_->Cancel();
      return true;
    });
    auto data = stream_->Read();
    watchdog.cancel();
    if (watchdog.get()) {
      status_ = Status(StatusCode::kDeadlineExceeded,
                       "Deadline exceeded waiting for data in ReadObject");
      // The stream is already cancelled, but we need to wait for its status.
      while (!absl::holds_alternative<Status>(data)) data = stream_->Read();
      return status_;
    }
    if (absl::holds_alternative<Status>(data)) {
      status_ = absl::get<Status>(std::move(data));
      auto metadata = stream_->GetRequestMetadata();
      result.response.headers.insert(metadata.begin(), metadata.end());
      stream_.reset();
      if (!status_.ok()) return status_;
      return result;
    }
    HandleResponse(result, absl::get<ReadObjectResponse>(std::move(data)),
                   buffer_manager);
  }

  return result;
}

void GrpcObjectReadSource::HandleResponse(
    ReadSourceResult& result, google::storage::v2::ReadObjectResponse response,
    BufferManager buffer_manager) {
  // The google.storage.v1.Storage documentation says this field can be
  // empty.
  if (response.has_checksummed_data()) {
    // Sometimes protobuf bytes are not strings, but the explicit conversion
    // always works.
    spill_ = std::string(
        std::move(*response.mutable_checksummed_data()->mutable_content()));
    std::tie(spill_view_, result.bytes_received) = buffer_manager(spill_);
  }
  if (response.has_object_checksums()) {
    auto const& checksums = response.object_checksums();
    if (checksums.has_crc32c()) {
      result.hashes = Merge(
          std::move(result.hashes),
          HashValues{
              GrpcObjectMetadataParser::Crc32cFromProto(checksums.crc32c()),
              {}});
    }
    if (!checksums.md5_hash().empty()) {
      result.hashes = Merge(std::move(result.hashes),
                            HashValues{{},
                                       GrpcObjectMetadataParser::MD5FromProto(
                                           checksums.md5_hash())});
    }
  }
  if (response.has_metadata()) {
    auto const& metadata = response.metadata();
    result.generation = result.generation.value_or(metadata.generation());
    result.metageneration =
        result.metageneration.value_or(metadata.metageneration());
    result.storage_class =
        result.storage_class.value_or(metadata.storage_class());
    result.size = result.size.value_or(metadata.size());
  }
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
