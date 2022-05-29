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
namespace {

std::chrono::milliseconds DefaultDownloadStallTimeout(
    std::chrono::milliseconds value) {
  if (value != std::chrono::milliseconds(0)) return value;
  return std::chrono::seconds(
      std::numeric_limits<std::chrono::seconds::rep>::max());
}

}  // namespace

GrpcObjectReadSource::GrpcObjectReadSource(
    std::unique_ptr<StreamingRpc> stream,
    std::chrono::milliseconds download_stall_timeout)
    : stream_(std::move(stream)),
      download_stall_timeout_(
          DefaultDownloadStallTimeout(download_stall_timeout)) {}

StatusOr<HttpResponse> GrpcObjectReadSource::Close() {
  if (stream_) stream_ = nullptr;
  if (!status_.ok()) return status_;
  return HttpResponse{HttpStatusCode::kOk, {}, {}};
}

/// Read more data from the download, returning any HTTP headers and error
/// codes.
StatusOr<ReadSourceResult> GrpcObjectReadSource::Read(char* buf,
                                                      std::size_t n) {
  std::size_t offset = 0;
  auto update_buf = [&offset, buf, n](absl::string_view source) {
    if (source.empty()) return source;
    auto const nbytes = std::min(n - offset, source.size());
    auto const* end = source.data() + nbytes;
    std::copy(source.data(), end, buf + offset);
    offset += nbytes;
    return absl::string_view(end, source.size() - nbytes);
  };

  ReadSourceResult result;
  result.response.status_code = HttpStatusCode::kContinue;
  result.bytes_received = 0;
  auto update_result = [&](google::storage::v2::ReadObjectResponse response) {
    // The google.storage.v1.Storage documentation says this field can be
    // empty.
    if (response.has_checksummed_data()) {
      // Sometimes protobuf bytes are not strings, but the explicit conversion
      // always works.
      spill_ = std::string(
          std::move(*response.mutable_checksummed_data()->mutable_content()));
      spill_view_ = update_buf(spill_);
      result.bytes_received = offset;
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
  };

  spill_view_ = update_buf(spill_view_);
  result.bytes_received = offset;
  while (offset < n && stream_) {
    auto data_future = stream_->Read();
    auto state = data_future.wait_for(download_stall_timeout_);
    if (state != std::future_status::ready) {
      status_ = Status(StatusCode::kDeadlineExceeded,
                       "Deadline exceeded waiting for data in ReadObject");
      stream_->Cancel();

      // Schedule a call to `Finish()` to close the stream.  gRPC requires the
      // `Read()` call to complete before calling `Finish()`, and we do not
      // want to block waiting for that here.
      using ::google::storage::v2::ReadObjectResponse;
      struct WaitForFinish {
        std::unique_ptr<StreamingRpc> stream;
        void operator()(future<Status>) {}
      };
      struct WaitForRead {
        std::unique_ptr<StreamingRpc> stream;
        void operator()(future<absl::optional<ReadObjectResponse>>) {
          auto finish = stream->Finish();
          (void)finish.then(WaitForFinish{std::move(stream)});
        }
      };
      (void)data_future.then(WaitForRead{std::move(stream_)});
      return status_;
    }
    auto data = data_future.get();
    if (!data.has_value()) {
      status_ = stream_->Finish().get();
      auto metadata = stream_->GetRequestMetadata();
      result.response.headers.insert(metadata.begin(), metadata.end());
      stream_.reset();
      if (!status_.ok()) return status_;
      return result;
    }
    update_result(std::move(*data));
  }

  return result;
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
