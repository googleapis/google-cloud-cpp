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
#include "absl/functional/function_ref.h"
#include "absl/strings/string_view.h"
#include <algorithm>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

GrpcObjectReadSource::GrpcObjectReadSource(std::unique_ptr<StreamingRpc> stream)
    : stream_(std::move(stream)) {}

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

  using BufferUpdater = absl::FunctionRef<absl::string_view(absl::string_view)>;
  struct Visitor {
    Visitor(GrpcObjectReadSource& source, BufferUpdater update)
        : self(source), update_buf(std::move(update)) {
      result.response.status_code = HttpStatusCode::kContinue;
    }

    GrpcObjectReadSource& self;
    BufferUpdater update_buf;
    ReadSourceResult result;

    void operator()(Status s) {
      // A status, whether success or failure, closes the stream.
      self.status_ = std::move(s);
      auto metadata = self.stream_->GetRequestMetadata();
      result.response.headers.insert(metadata.begin(), metadata.end());
      self.stream_ = nullptr;
    }
    void operator()(google::storage::v2::ReadObjectResponse response) {
      // The google.storage.v1.Storage documentation says this field can be
      // empty.
      if (response.has_checksummed_data()) {
        // Sometimes protobuf bytes are not strings, but the explicit conversion
        // always works.
        self.spill_ = std::string(
            std::move(*response.mutable_checksummed_data()->mutable_content()));
        self.spill_view_ = update_buf(self.spill_);
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
          result.hashes =
              Merge(std::move(result.hashes),
                    HashValues{{},
                               GrpcObjectMetadataParser::MD5FromProto(
                                   checksums.md5_hash())});
        }
      }
      if (response.has_metadata()) {
        result.generation =
            result.generation.value_or(response.metadata().generation());
        result.metageneration = result.metageneration.value_or(
            response.metadata().metageneration());
        result.storage_class =
            result.storage_class.value_or(response.metadata().storage_class());
        result.size = result.size.value_or(response.metadata().size());
      }
    }
  };

  spill_view_ = update_buf(spill_view_);
  Visitor visitor{*this, update_buf};
  while (offset < n && stream_) {
    absl::visit(visitor, stream_->Read());
  }

  if (!status_.ok()) return status_;
  visitor.result.bytes_received = offset;
  return std::move(visitor.result);
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
